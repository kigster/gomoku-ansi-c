variable "project_id" {
  description = "The Google Cloud Project ID"
  type        = string
}

variable "region" {
  description = "The Google Cloud region to deploy to"
  type        = string
  default     = "us-central1"
}

# Environment selector. Drives:
#   - Cloud Run service names: production keeps the bare names
#     (`gomoku-api`, `gomoku-httpd`); other envs append `-${env}`
#     (`gomoku-api-staging`, `gomoku-httpd-staging`).
#   - The ENVIRONMENT runtime env var on the api container so Pydantic +
#     the OTLP exporter pick the right config.
#   - Default sizing knobs (api min_instances, httpd max_instances).
# State separation per environment is handled at `terraform init` time
# via `-backend-config="prefix=cloud-run/${ENVIRONMENT}/gomoku"`, NOT
# here — keep this var purely declarative.
variable "environment" {
  description = "Deployment environment name (production | staging | other)"
  type        = string
  default     = "production"
  validation {
    condition     = contains(["production", "staging"], var.environment)
    error_message = "environment must be one of: production, staging."
  }
}

variable "httpd_image" {
  description = "Docker image for gomoku-httpd (C game engine)"
  type        = string
}

variable "api_image" {
  description = "Docker image for gomoku-api (FastAPI + React SPA)"
  type        = string
  default     = "placeholder"
}

# httpd is single-threaded (max_instance_request_concurrency = 1) so each
# inflight game move pins an entire instance. The api is configured for
# 80 concurrent in-flight requests, so production needs at least
# httpd_max_instances == 80 to fully saturate one api instance without
# queueing.
variable "httpd_min_instances" {
  description = "Minimum number of gomoku-httpd instances to keep warm. 0 = scale to zero."
  type        = number
  default     = 0
}

variable "httpd_max_instances" {
  description = "Maximum number of gomoku-httpd instances. >= api_max_instances * 80 to avoid queueing under saturation."
  type        = number
  # Default 20 fits inside the GCP `CpuAllocPerProjectRegion` baseline
  # of 56,000 mCPU (= 56 instances at 1 vCPU each). To realize the
  # documented "1 api saturates 80 httpd workers" contract you must
  # first request a quota increase via
  #   gcloud quotas update --service=run.googleapis.com \
  #     --metric=run.googleapis.com/cpu_allocation \
  #     --consumer=projects/PROJECT_ID --value=120000
  # then bump this default (or pass TF_VAR_httpd_max_instances=80).
  default = 20
}

variable "api_min_instances" {
  description = "Minimum number of gomoku-api instances to keep warm. Production: 1. Staging: 0."
  type        = number
  default     = 1
}

variable "api_max_instances" {
  description = "Maximum number of gomoku-api instances."
  type        = number
  default     = 5
}

variable "jwt_secret" {
  description = "JWT signing secret for the API"
  type        = string
  sensitive   = true
}

variable "database_url" {
  description = "PostgreSQL connection string (e.g. Neon DSN)"
  type        = string
  sensitive   = true
}

variable "cors_origins" {
  description = "List of allowed CORS origins"
  type        = list(string)
  default     = ["*"]
}

variable "honeycomb_api_key" {
  description = "Honeycomb ingest key for OTLP traces. Empty disables tracing."
  type        = string
  default     = ""
  sensitive   = true
}

variable "honeycomb_dataset" {
  description = "Honeycomb dataset name for classic keys. Ignored for env-aware keys."
  type        = string
  default     = ""
}

variable "custom_domain" {
  description = "Custom domain to map to gomoku-api (e.g. gomoku.us, app.gomoku.us, staging.gomoku.games). Empty disables the mapping."
  type        = string
  default     = ""
}
