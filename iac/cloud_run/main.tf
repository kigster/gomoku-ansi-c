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
  ingress  = "INGRESS_TRAFFIC_INTERNAL_ONLY"

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

      command = ["./gomoku-httpd"]
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
      min_instance_count = 0
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
