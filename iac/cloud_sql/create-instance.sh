#!/usr/bin/env bash
set -euo pipefail

# Cloud SQL instance for Gomoku game history
PROJECT="fine-booking-486503-k7"
REGION="us-central1"
INSTANCE="gomoku-db"
DATABASE="gomoku"

echo "=== Creating Cloud SQL PostgreSQL 17 instance ==="
gcloud sql instances create "$INSTANCE" \
  --project="$PROJECT" \
  --database-version=POSTGRES_17 \
  --tier=db-f1-micro \
  --region="$REGION" \
  --no-assign-ip \
  --network=default \
  --storage-type=SSD \
  --storage-size=10GB \
  --database-flags=log_min_duration_statement=1000

echo "=== Creating database ==="
gcloud sql databases create "$DATABASE" \
  --project="$PROJECT" \
  --instance="$INSTANCE"

echo "=== Setting up schema ==="
# Connect via proxy and run setup.sql
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
gcloud sql connect "$INSTANCE" \
  --project="$PROJECT" \
  --database="$DATABASE" \
  --user=postgres \
  < "$SCRIPT_DIR/setup.sql"

echo "=== Connecting Cloud Run services ==="
CONN="${PROJECT}:${REGION}:${INSTANCE}"

gcloud run services update gomoku-httpd \
  --project="$PROJECT" \
  --region="$REGION" \
  --add-cloudsql-instances="$CONN" \
  --set-env-vars="DB_SOCKET=/cloudsql/${CONN},DB_NAME=${DATABASE},DB_USER=postgres"

echo ""
echo "Done. Cloud Run connects via Unix socket at /cloudsql/${CONN}"
echo "No public IP, no passwords — IAM auth only."
