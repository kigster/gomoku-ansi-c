terraform {
  required_version = ">= 1.0"
  required_providers {
    google = {
      source  = "hashicorp/google"
      version = "~> 5.0"
    }
  }
  backend "gcs" {
    bucket = "gomoku-tfstate"
    prefix = "cloud-run/gomoku"
  }
}

provider "google" {
  project = var.project_id
  region  = var.region
}

# Enable APIs
resource "google_project_service" "run_api" {
  service            = "run.googleapis.com"
  disable_on_destroy = false
}

resource "google_project_service" "artifact_registry_api" {
  service            = "artifactregistry.googleapis.com"
  disable_on_destroy = false
}

resource "google_project_service" "cloudbuild_api" {
  service            = "cloudbuild.googleapis.com"
  disable_on_destroy = false
}

# Artifact Registry Repository
resource "google_artifact_registry_repository" "repo" {
  location      = var.region
  repository_id = "gomoku-repo"
  description   = "Docker repository for Gomoku services"
  format        = "DOCKER"
  depends_on    = [google_project_service.artifact_registry_api]
}

# ──────────────────────────────────────────────
# gomoku-httpd — C game engine (INTERNAL only)
# ──────────────────────────────────────────────

resource "google_cloud_run_v2_service" "httpd" {
  name     = "gomoku-httpd"
  location = var.region
  # INGRESS_TRAFFIC_ALL + IAM-restricted invoker is the canonical Cloud-Run-
  # to-Cloud-Run pattern. INGRESS_TRAFFIC_INTERNAL_ONLY rejects the api's
  # public-URL request (returning a stock 404) unless the api routes through
  # a VPC connector — over-engineering for the security gain. Access is still
  # tightly scoped: only google_cloud_run_service_iam_member.api_invokes_httpd
  # below holds the invoker role; everyone else gets 403.
  ingress = "INGRESS_TRAFFIC_ALL"

  template {
    scaling {
      min_instance_count = var.min_instances
      max_instance_count = var.max_instances
    }

    containers {
      image = var.httpd_image

      ports {
        container_port = 8787
      }

      # Dockerfile WORKDIR is /app/source and the binary lives at bin/gomoku-httpd
      command = ["./bin/gomoku-httpd"]
      args    = ["-b", "0.0.0.0:8787", "-L", "info"]

      startup_probe {
        http_get {
          path = "/health"
          port = 8787
        }
        initial_delay_seconds = 2
        period_seconds        = 3
        failure_threshold     = 5
        timeout_seconds       = 3
      }

      resources {
        limits = {
          cpu    = "1000m"
          memory = "512Mi"
        }
      }
    }

    # Single-threaded: one request at a time per instance
    max_instance_request_concurrency = 1
  }

  traffic {
    type    = "TRAFFIC_TARGET_ALLOCATION_TYPE_LATEST"
    percent = 100
  }

  depends_on = [google_project_service.run_api]
}

# ──────────────────────────────────────────────
# gomoku-api — FastAPI + React SPA (PUBLIC)
# Serves static frontend, auth, scoring,
# leaderboard, and proxies game moves to httpd.
# ──────────────────────────────────────────────

resource "google_cloud_run_v2_service" "api" {
  name     = "gomoku-api"
  location = var.region
  ingress  = "INGRESS_TRAFFIC_ALL"

  template {
    scaling {
      # Keep at least one instance warm so app.gomoku.games loads instantly.
      # FastAPI cold-start under uvicorn + asyncpg pool init + telemetry SDK
      # bootstrap is several seconds — long enough that an unwarmed visitor
      # hits a blank tab while the container spins up. The cost of one
      # always-on small Cloud Run instance is marginal compared to the UX
      # hit, especially while traffic is low.
      min_instance_count = 1
      max_instance_count = 5
    }

    containers {
      image = var.api_image

      ports {
        container_port = 8000
      }

      env {
        name  = "GOMOKU_HTTPD_URL"
        value = google_cloud_run_v2_service.httpd.uri
      }

      env {
        name  = "DATABASE_URL"
        value = var.database_url
      }

      env {
        name  = "JWT_SECRET"
        value = var.jwt_secret
      }

      env {
        name  = "CORS_ORIGINS"
        value = jsonencode(var.cors_origins)
      }

      env {
        name  = "ENVIRONMENT"
        value = "production"
      }

      env {
        name  = "OTEL_SERVICE_NAME"
        value = "gomoku-api"
      }

      env {
        name  = "HONEYCOMB_API_KEY"
        value = var.honeycomb_api_key
      }

      env {
        name  = "HONEYCOMB_DATASET"
        value = var.honeycomb_dataset
      }

      startup_probe {
        http_get {
          path = "/health"
          port = 8000
        }
        initial_delay_seconds = 5
        period_seconds        = 3
        failure_threshold     = 5
        timeout_seconds       = 3
      }

      resources {
        limits = {
          cpu    = "1000m"
          memory = "512Mi"
        }
      }
    }

    # Async — can handle many concurrent requests
    max_instance_request_concurrency = 80
  }

  traffic {
    type    = "TRAFFIC_TARGET_ALLOCATION_TYPE_LATEST"
    percent = 100
  }

  depends_on = [google_project_service.run_api]
}

# Allow API to invoke httpd (service-to-service auth)
resource "google_cloud_run_service_iam_member" "api_invokes_httpd" {
  location = google_cloud_run_v2_service.httpd.location
  service  = google_cloud_run_v2_service.httpd.name
  role     = "roles/run.invoker"
  member   = "serviceAccount:${google_cloud_run_v2_service.api.template[0].service_account}"
}

# API is public-facing (serves the SPA + API)
resource "google_cloud_run_service_iam_member" "api_public_access" {
  location = google_cloud_run_v2_service.api.location
  service  = google_cloud_run_v2_service.api.name
  role     = "roles/run.invoker"
  member   = "allUsers"
}

# ──────────────────────────────────────────────
# Custom domain mapping for gomoku-api
# ──────────────────────────────────────────────
# Created only when var.custom_domain is non-empty. Survives service
# destroy/recreate cycles because it's part of the same `terraform apply` —
# without this, a renamed/recreated service silently leaves the domain
# pointing at a deleted target ("Page not found"). DNS verification still
# happens out-of-band: add the CNAME from the `custom_domain_dns_records`
# output at your DNS provider; Google then provisions the TLS cert.

resource "google_cloud_run_domain_mapping" "api" {
  count    = var.custom_domain != "" ? 1 : 0
  name     = var.custom_domain
  location = var.region

  metadata {
    namespace = var.project_id
  }

  spec {
    route_name = google_cloud_run_v2_service.api.name
  }

  depends_on = [google_cloud_run_service_iam_member.api_public_access]
}
