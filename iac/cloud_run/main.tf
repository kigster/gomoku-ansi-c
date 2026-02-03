terraform {
  required_version = ">= 1.0"
  required_providers {
    google = {
      source  = "hashicorp/google"
      version = "~> 5.0"
    }
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
      
      # We override the entrypoint/command to ensure it listens on the correct port
      # Cloud Run injects the PORT env var. We bind to it.
      # The user requested port 8797 specifically in their VM design. 
      # We can force that here by setting the PORT env var.
      
      ports {
        container_port = 8797
      }

      env {
        name  = "PORT"
        value = "8797"
      }

      # Override CMD to use the port
      args = ["-b", "0.0.0.0:8797", "-L", "info"]
      
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

# Make the service public (stateless/open as requested)
resource "google_cloud_run_service_iam_member" "public_access" {
  location = google_cloud_run_v2_service.default.location
  service  = google_cloud_run_v2_service.default.name
  role     = "roles/run.invoker"
  member   = "allUsers"
}
