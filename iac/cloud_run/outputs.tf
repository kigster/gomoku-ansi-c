output "httpd_url" {
  description = "The URL of the gomoku-httpd Cloud Run service (internal)"
  value       = google_cloud_run_v2_service.httpd.uri
}

output "api_url" {
  description = "The URL of the gomoku-api Cloud Run service (public)"
  value       = google_cloud_run_v2_service.api.uri
}

output "artifact_registry_repo" {
  description = "The Artifact Registry repository path"
  value       = google_artifact_registry_repository.repo.name
}
