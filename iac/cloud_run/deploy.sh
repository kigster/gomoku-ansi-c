#!/bin/bash
set -e

REGION="us-central1"
REPO_NAME="gomoku-repo"
export PROJECT_ID="${PROJECT_ID:-fine-booking-486503-k7}"

if [ -z "$PROJECT_ID" ]; then
    echo "Error: PROJECT_ID environment variable is not set."
    exit 1
fi

if [ -z "$TF_VAR_jwt_secret" ]; then
    echo "Error: TF_VAR_jwt_secret must be set for the API service."
    echo "Usage: export TF_VAR_jwt_secret='your-secret' && ./deploy.sh"
    exit 1
fi

echo "Starting deployment for Project: $PROJECT_ID, Region: $REGION"

# 1. Initialize Terraform
echo "Initializing Terraform..."
terraform init -upgrade

# 2. Create Artifact Registry Repository (and enable APIs)
echo "Ensuring APIs and registry exist..."
terraform apply -target=google_project_service.run_api \
    -target=google_project_service.artifact_registry_api \
    -target=google_project_service.cloudbuild_api \
    -target=google_project_service.sqladmin_api \
    -target=google_artifact_registry_repository.repo \
    -var="project_id=$PROJECT_ID" \
    -var="region=$REGION" \
    -var="container_image=placeholder" \
    -var="jwt_secret=$TF_VAR_jwt_secret" \
    -auto-approve

# 3. Configure Docker auth
gcloud auth configure-docker "$REGION-docker.pkg.dev" --quiet

# 4. Build and Push all images
REGISTRY="$REGION-docker.pkg.dev/$PROJECT_ID/$REPO_NAME"

HTTPD_IMAGE="$REGISTRY/gomoku-httpd:latest"
echo "Building gomoku-httpd for linux/amd64..."
docker buildx build --platform linux/amd64 -t "$HTTPD_IMAGE" --load ../../
docker push "$HTTPD_IMAGE"

API_IMAGE="$REGISTRY/gomoku-api:latest"
echo "Building gomoku-api for linux/amd64..."
docker buildx build --platform linux/amd64 -t "$API_IMAGE" --load ../../api/
docker push "$API_IMAGE"

FRONTEND_IMAGE="$REGISTRY/gomoku-frontend:latest"
echo "Building gomoku-frontend for linux/amd64..."
docker buildx build --platform linux/amd64 -t "$FRONTEND_IMAGE" --load ../../frontend/
docker push "$FRONTEND_IMAGE"

# 5. Deploy all Cloud Run Services
echo "Deploying Cloud Run Services..."
terraform apply \
    -var="project_id=$PROJECT_ID" \
    -var="region=$REGION" \
    -var="container_image=$HTTPD_IMAGE" \
    -var="api_image=$API_IMAGE" \
    -var="frontend_image=$FRONTEND_IMAGE" \
    -var="jwt_secret=$TF_VAR_jwt_secret" \
    -auto-approve

echo ""
echo "Deployment Complete!"
echo "Backend (internal):  $(terraform output -raw httpd_url)"
echo "API (internal):      $(terraform output -raw api_url)"
echo "Frontend (public):   $(terraform output -raw frontend_url)"
