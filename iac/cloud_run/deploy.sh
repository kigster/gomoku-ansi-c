#!/bin/bash
set -e

# Default values
REGION="us-central1"
REPO_NAME="gomoku-repo"
BACKEND_IMAGE_NAME="gomoku-httpd"
FRONTEND_IMAGE_NAME="gomoku-frontend"

if [ -z "$PROJECT_ID" ]; then
    echo "Error: PROJECT_ID environment variable is not set."
    echo "Usage: export PROJECT_ID=your-project-id && ./deploy.sh"
    exit 1
fi

echo "Starting deployment for Project: $PROJECT_ID, Region: $REGION"

# 1. Initialize Terraform
echo "Initializing Terraform..."
terraform init -upgrade

# 2. Create Artifact Registry Repository (and enable APIs)
echo "Ensuring Artifact Registry Repository exists..."
terraform apply -target=google_project_service.run_api \
    -target=google_project_service.artifact_registry_api \
    -target=google_project_service.cloudbuild_api \
    -target=google_artifact_registry_repository.repo \
    -var="project_id=$PROJECT_ID" \
    -var="region=$REGION" \
    -var="container_image=placeholder" \
    -auto-approve

# 3. Configure Docker auth for Artifact Registry
gcloud auth configure-docker "$REGION-docker.pkg.dev" --quiet

# 4. Build and Push Backend Image
BACKEND_FULL_IMAGE="$REGION-docker.pkg.dev/$PROJECT_ID/$REPO_NAME/$BACKEND_IMAGE_NAME:latest"
echo "Building backend Docker image for linux/amd64..."
docker buildx build --platform linux/amd64 -t "$BACKEND_FULL_IMAGE" --load ../../

echo "Pushing backend image to $BACKEND_FULL_IMAGE..."
docker push "$BACKEND_FULL_IMAGE"

# 5. Build and Push Frontend Image
FRONTEND_FULL_IMAGE="$REGION-docker.pkg.dev/$PROJECT_ID/$REPO_NAME/$FRONTEND_IMAGE_NAME:latest"
echo "Building frontend Docker image for linux/amd64..."
docker buildx build --platform linux/amd64 -t "$FRONTEND_FULL_IMAGE" --load ../../frontend/

echo "Pushing frontend image to $FRONTEND_FULL_IMAGE..."
docker push "$FRONTEND_FULL_IMAGE"

# 6. Deploy both Cloud Run Services
echo "Deploying Cloud Run Services..."
terraform apply \
    -var="project_id=$PROJECT_ID" \
    -var="region=$REGION" \
    -var="container_image=$BACKEND_FULL_IMAGE" \
    -var="frontend_image=$FRONTEND_FULL_IMAGE" \
    -auto-approve

echo ""
echo "Deployment Complete!"
echo "Backend URL:"
terraform output service_url
echo "Frontend URL:"
terraform output frontend_url
