#!/usr/bin/env bash
set -euo pipefail

REGION="us-central1"
REPO_NAME="gomoku-repo"
export PROJECT_ID="${PROJECT_ID:-fine-booking-486503-k7}"
REGISTRY="$REGION-docker.pkg.dev/$PROJECT_ID/$REPO_NAME"
REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

# Parse which services to update (default: all)
SERVICES="${*:-httpd api}"

gcloud auth configure-docker "$REGION-docker.pkg.dev" --quiet

if [[ "$SERVICES" == *"httpd"* ]]; then
    IMAGE="$REGISTRY/gomoku-httpd:latest"
    echo "Building and pushing gomoku-httpd..."
    docker buildx build --platform linux/amd64 -t "$IMAGE" --load "$REPO_ROOT/gomoku-c/"
    docker push "$IMAGE"
    gcloud run services update gomoku-httpd --region="$REGION" --image="$IMAGE"
fi

if [[ "$SERVICES" == *"api"* ]]; then
    IMAGE="$REGISTRY/gomoku-api:latest"
    echo "Building and pushing gomoku-api (includes frontend)..."
    docker buildx build --platform linux/amd64 -t "$IMAGE" --load "$REPO_ROOT/api/"
    docker push "$IMAGE"
    gcloud run services update gomoku-api --region="$REGION" --image="$IMAGE"
fi

echo "Update complete."
