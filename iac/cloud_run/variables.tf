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
  description = "The full Docker image path (e.g., gcr.io/PROJECT/IMAGE:TAG or artifact registry URL)"
  type        = string
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

variable "frontend_image" {
  description = "The full Docker image path for the frontend (nginx + React SPA)"
  type        = string
  default     = "placeholder"
}
