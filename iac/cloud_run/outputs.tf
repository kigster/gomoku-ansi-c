output "service_url" {
  description = "The URL of the deployed Cloud Run service"
  value       = google_cloud_run_v2_service.default.uri
}

output "artifact_registry_repo" {
  description = "The created Artifact Registry repository"
  value       = google_artifact_registry_repository.repo.name
}

output "frontend_url" {
  description = "The URL of the deployed frontend Cloud Run service"
  value       = google_cloud_run_v2_service.frontend.uri
}
