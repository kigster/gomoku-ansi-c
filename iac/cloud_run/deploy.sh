#!/bin/bash
set -e

# Default values
REGION="us-central1"
REPO_NAME="gomoku-repo"
IMAGE_NAME="gomoku-httpd"

if [ -z "$PROJECT_ID" ]; then
    echo "Error: PROJECT_ID environment variable is not set."
    echo "Usage: export PROJECT_ID=your-project-id && ./deploy.sh"
    exit 1
fi

echo "🚀 Starting deployment for Project: $PROJECT_ID"

# 1. Initialize Terraform
echo "📦 Initializing Terraform..."
terraform init -upgrade

# 2. Create Artifact Registry Repository (Targeting just the repo resource)
echo "📂 Ensuring Artifact Registry Repository exists..."
terraform apply -target=google_artifact_registry_repository.repo \
    -var="project_id=$PROJECT_ID" \
    -var="region=$REGION" \
    -var="container_image=placeholder" \
    -auto-approve

# 3. Build and Push Image
FULL_IMAGE_NAME="$REGION-docker.pkg.dev/$PROJECT_ID/$REPO_NAME/$IMAGE_NAME:latest"
echo "🔨 Building and Pushing Docker Image to $FULL_IMAGE_NAME..."
# We use gcloud builds submit to build in the cloud (avoids local Docker issues)
# pointing to the root of the repo (../../)
gcloud builds submit --tag "$FULL_IMAGE_NAME" ../../

# 4. Deploy Cloud Run Service
echo "☁️ Deploying Cloud Run Service..."
terraform apply \
    -var="project_id=$PROJECT_ID" \
    -var="region=$REGION" \
    -var="container_image=$FULL_IMAGE_NAME" \
    -auto-approve

echo "✅ Deployment Complete!"
echo "Run 'terraform output service_url' to see your endpoint."
