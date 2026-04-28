variable "project_id" {
  description = "The Google Cloud Project ID"
  type        = string
}

variable "region" {
  description = "The Google Cloud region to deploy to"
  type        = string
  default     = "us-central1"
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

variable "min_instances" {
  description = "Minimum number of gomoku-httpd instances to keep warm"
  type        = number
  default     = 0
}

variable "max_instances" {
  description = "Maximum number of gomoku-httpd instances to scale up to"
  type        = number
  default     = 20
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
  description = "Custom domain to map to gomoku-api (e.g. app.gomoku.games). Empty disables the mapping."
  type        = string
  default     = ""
}
