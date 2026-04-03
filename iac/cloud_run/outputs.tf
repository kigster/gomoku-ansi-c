output "httpd_url" {
  description = "The URL of the gomoku-httpd Cloud Run service (internal)"
  value       = google_cloud_run_v2_service.httpd.uri
}

output "api_url" {
  description = "The URL of the FastAPI Cloud Run service (internal)"
  value       = google_cloud_run_v2_service.api.uri
}

output "frontend_url" {
  description = "The URL of the frontend Cloud Run service (public)"
  value       = google_cloud_run_v2_service.frontend.uri
}

output "artifact_registry_repo" {
  description = "The created Artifact Registry repository"
  value       = google_artifact_registry_repository.repo.name
}
