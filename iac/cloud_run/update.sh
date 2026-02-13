#!/bin/bash
set -e

# Default values
REGION="us-central1"
REPO_NAME="gomoku-repo"
BACKEND_IMAGE_NAME="gomoku-httpd"
FRONTEND_IMAGE_NAME="gomoku-frontend"

export PROJECT_ID="fine-booking-486503-k7"

if [ -z "$PROJECT_ID" ]; then
    echo "Error: PROJECT_ID environment variable is not set."
    echo "Usage: export PROJECT_ID=your-project-id && ./deploy.sh"
    exit 1
fi

# Frontend only
IMAGE="$REGION-docker.pkg.dev/$PROJECT_ID/gomoku-repo/gomoku-frontend:latest"
docker buildx build --platform linux/amd64 -t "$IMAGE" --load frontend/
docker push "$IMAGE"
gcloud run services update gomoku-frontend --region=$REGION --image=$IMAGE

# Backend only
IMAGE="$REGION-docker.pkg.dev/$PROJECT_ID/gomoku-repo/gomoku-httpd:latest"
docker buildx build --platform linux/amd64 -t "$IMAGE" --load .
docker push "$IMAGE"
gcloud run services update gomoku-httpd --region=$REGION --image=$IMAGE
