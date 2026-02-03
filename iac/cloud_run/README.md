# Gomoku HTTPD on Google Cloud Run

This directory contains Terraform configuration to deploy the `gomoku-httpd` application to **Google Cloud Run**.

## Why Cloud Run?

This is the "easiest and fastest" architecture compared to managing VMs and HAProxy manually:
1.  **Stateless Autoscaling**: Cloud Run manages scaling from 0 to N automatically.
2.  **Concurrency Management**: We set `max_instance_request_concurrency = 1`. This perfectly matches your single-threaded binary requirement. When an instance is busy with one request, Cloud Run automatically routes new requests to other instances or spins up new ones.
3.  **No Maintenance**: No need to patch OS, configure Nginx, or tune HAProxy health checks.
4.  **Cost Effective**: You only pay for CPU when the request is processing (if scaling to 0).

## Architecture
- **Load Balancer**: Cloud Run provides a Global External HTTPS Load Balancer URL automatically.
- **Workers**: Ephemeral containers running `gomoku-httpd`.
- **Port**: Configured to listen on port `8797` internally, exposed via standard HTTPS (443).

## Prerequisites
1.  [Google Cloud SDK](https://cloud.google.com/sdk/docs/install) installed.
2.  [Terraform](https://www.terraform.io/) installed.
3.  A Google Cloud Project.

## Deployment Instructions

### 1. Initialize Terraform
```bash
terraform init
```

### 2. Build and Push Docker Image
You need to build the image and push it to a registry (like Google Artifact Registry) so Cloud Run can fetch it.

```bash
# Set your project ID
export PROJECT_ID="your-project-id"
export REGION="us-central1"

# Enable Artifact Registry API if not enabled
gcloud services enable artifactregistry.googleapis.com --project $PROJECT_ID

# Create repository (if using the terraform to create it, do step 3 first, then push)
# Use the helper script or manual commands:
gcloud builds submit --tag "$REGION-docker.pkg.dev/$PROJECT_ID/gomoku-repo/gomoku-httpd:latest" ../../
```

### 3. Deploy
```bash
terraform apply -var="project_id=$PROJECT_ID" -var="container_image=$REGION-docker.pkg.dev/$PROJECT_ID/gomoku-repo/gomoku-httpd:latest"
```
