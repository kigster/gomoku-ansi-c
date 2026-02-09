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

# Enable Cloud Run API
resource "google_project_service" "run_api" {
  service = "run.googleapis.com"
  disable_on_destroy = false
}

# Enable Artifact Registry API (for storing the image)
resource "google_project_service" "artifact_registry_api" {
  service = "artifactregistry.googleapis.com"
  disable_on_destroy = false
}

# Enable Cloud Build API (for gcloud builds submit, if used)
resource "google_project_service" "cloudbuild_api" {
  service = "cloudbuild.googleapis.com"
  disable_on_destroy = false
}

# Artifact Registry Repository
resource "google_artifact_registry_repository" "repo" {
  location      = var.region
  repository_id = "gomoku-repo"
  description   = "Docker repository for Gomoku HTTPD"
  format        = "DOCKER"
  depends_on    = [google_project_service.artifact_registry_api]
}

# Cloud Run Service
resource "google_cloud_run_v2_service" "default" {
  name     = "gomoku-httpd"
  location = var.region
  ingress  = "INGRESS_TRAFFIC_ALL"

  template {
    scaling {
      min_instance_count = var.min_instances
      max_instance_count = var.max_instances
    }

    containers {
      image = var.container_image
      
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
    
    # Critical: Set max_instance_request_concurrency to 1
    # because the daemon is single-threaded. This triggers auto-scaling
    # immediately when a request comes in and the instance is busy.
    max_instance_request_concurrency = 1
  }

  traffic {
    type    = "TRAFFIC_TARGET_ALLOCATION_TYPE_LATEST"
    percent = 100
  }

  depends_on = [google_project_service.run_api]
}

# Make the backend service public
resource "google_cloud_run_service_iam_member" "public_access" {
  location = google_cloud_run_v2_service.default.location
  service  = google_cloud_run_v2_service.default.name
  role     = "roles/run.invoker"
  member   = "allUsers"
}

# ──────────────────────────────────────────────
# Frontend (nginx serving React SPA, proxying API to backend)
# ──────────────────────────────────────────────

# Derive backend hostname from the backend service URI (e.g., "https://gomoku-httpd-xxx.a.run.app")
locals {
  backend_host = replace(google_cloud_run_v2_service.default.uri, "https://", "")
  backend_url  = "${local.backend_host}:443"
}

resource "google_cloud_run_v2_service" "frontend" {
  name     = "gomoku-frontend"
  location = var.region
  ingress  = "INGRESS_TRAFFIC_ALL"

  template {
    scaling {
      min_instance_count = 0
      max_instance_count = 3
    }

    containers {
      image = var.frontend_image

      ports {
        container_port = 80
      }

      env {
        name  = "BACKEND_URL"
        value = local.backend_url
      }

      env {
        name  = "BACKEND_PROTO"
        value = "https"
      }

      env {
        name  = "BACKEND_HOST"
        value = local.backend_host
      }

      startup_probe {
        http_get {
          path = "/nginx-health"
          port = 80
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

    # nginx can handle many concurrent connections
    max_instance_request_concurrency = 80
  }

  traffic {
    type    = "TRAFFIC_TARGET_ALLOCATION_TYPE_LATEST"
    percent = 100
  }

  depends_on = [google_project_service.run_api]
}

# Make the frontend service public
resource "google_cloud_run_service_iam_member" "frontend_public_access" {
  location = google_cloud_run_v2_service.frontend.location
  service  = google_cloud_run_v2_service.frontend.name
  role     = "roles/run.invoker"
  member   = "allUsers"
}
