variable "project_id" {
  description = "The Google Cloud Project ID"
  type        = string
}

variable "region" {
  description = "The Google Cloud region to deploy to"
  type        = string
  default     = "us-central1"
}

variable "container_image" {
  description = "The full Docker image path for gomoku-httpd"
  type        = string
}

variable "api_image" {
  description = "The full Docker image path for the FastAPI service"
  type        = string
  default     = "placeholder"
}

variable "frontend_image" {
  description = "The full Docker image path for the frontend (nginx + React SPA)"
  type        = string
  default     = "placeholder"
}

variable "min_instances" {
  description = "Minimum number of instances to keep warm"
  type        = number
  default     = 0
}

variable "max_instances" {
  description = "Maximum number of instances to scale up to"
  type        = number
  default     = 20
}

variable "jwt_secret" {
  description = "JWT signing secret for the API"
  type        = string
  sensitive   = true
}

variable "db_instance_connection" {
  description = "Cloud SQL instance connection name (project:region:instance)"
  type        = string
  default     = "fine-booking-486503-k7:us-central1:gomoku-db"
}
