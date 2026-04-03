#!/usr/bin/env bash
set -euo pipefail

REGION="us-central1"
REPO_NAME="gomoku-repo"

# Required environment variables
: "${PROJECT_ID:?Set PROJECT_ID to your GCP project ID}"
: "${TF_VAR_jwt_secret:?Set TF_VAR_jwt_secret (openssl rand -base64 32)}"
: "${TF_VAR_database_url:?Set TF_VAR_database_url to your Neon PostgreSQL DSN}"

export PROJECT_ID
REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

echo "Deploying to project: $PROJECT_ID, region: $REGION"

# 1. Initialize Terraform
echo "Initializing Terraform..."
terraform init -upgrade

# 2. Enable APIs and create Artifact Registry
echo "Ensuring APIs and registry exist..."
terraform apply \
    -target=google_project_service.run_api \
    -target=google_project_service.artifact_registry_api \
    -target=google_project_service.cloudbuild_api \
    -target=google_artifact_registry_repository.repo \
    -var="project_id=$PROJECT_ID" \
    -var="region=$REGION" \
    -var="httpd_image=placeholder" \
    -var="jwt_secret=$TF_VAR_jwt_secret" \
    -var="database_url=$TF_VAR_database_url" \
    -auto-approve

# 3. Configure Docker auth for Artifact Registry
gcloud auth configure-docker "$REGION-docker.pkg.dev" --quiet

# 4. Build and push Docker images
REGISTRY="$REGION-docker.pkg.dev/$PROJECT_ID/$REPO_NAME"

HTTPD_IMAGE="$REGISTRY/gomoku-httpd:latest"
echo "Building and pushing gomoku-httpd..."
docker buildx build --platform linux/amd64 -t "$HTTPD_IMAGE" --load "$REPO_ROOT/gomoku-c/"
docker push "$HTTPD_IMAGE"

API_IMAGE="$REGISTRY/gomoku-api:latest"
echo "Building and pushing gomoku-api..."
docker buildx build --platform linux/amd64 -t "$API_IMAGE" --load "$REPO_ROOT/api/"
docker push "$API_IMAGE"

# 5. Deploy all Cloud Run services
echo "Deploying Cloud Run services..."
terraform apply \
    -var="project_id=$PROJECT_ID" \
    -var="region=$REGION" \
    -var="httpd_image=$HTTPD_IMAGE" \
    -var="api_image=$API_IMAGE" \
    -var="jwt_secret=$TF_VAR_jwt_secret" \
    -var="database_url=$TF_VAR_database_url" \
    -auto-approve

echo ""
echo "Deployment complete!"
echo "Game engine (internal): $(terraform output -raw httpd_url)"
echo "API + SPA (public):     $(terraform output -raw api_url)"
