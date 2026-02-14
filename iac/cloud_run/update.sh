#!/usr/bin/env bash

set +e

[[ -d ~/.bashmatic ]] || bash -c "$(curl -fsSL https://bashmatic.re1.re); bashmatic-install -q" >/dev/null 2>&1
# shellcheck disable=SC1090
source ~/.bashmatic/init >/dev/null 2>&1

# Default values
REGION="us-central1"
REPO_NAME="gomoku-repo"
BACKEND_IMAGE_NAME="gomoku-httpd"
FRONTEND_IMAGE_NAME="gomoku-frontend"

export PROJECT_ID="${PROJECT_ID:-"fine-booking-486503-k7"}"
if [[ -z "$PROJECT_ID" ]]; then 
    error "PROJECT_ID environment variable is not set."
    panel-info "USAGE: export PROJECT_ID=your-project-id && ./update.sh"
    exit 1
fi

set -e

# Frontend only
IMAGE="$REGION-docker.pkg.dev/$PROJECT_ID/gomoku-repo/gomoku-frontend:latest"
docker buildx build --platform linux/amd64 -t "$IMAGE" --load ../../frontend/
docker push "$IMAGE"
gcloud run services update gomoku-frontend --region=$REGION --image=$IMAGE

# Backend only
IMAGE="$REGION-docker.pkg.dev/$PROJECT_ID/gomoku-repo/gomoku-httpd:latest"
docker buildx build --platform linux/amd64 -t "$IMAGE" --load ../../
docker push "$IMAGE"
gcloud run services update gomoku-httpd --region=$REGION --image=$IMAGE

