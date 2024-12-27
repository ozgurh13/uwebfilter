variable "uwebfilter_logcollect_files" {
  type        = string
  description = "location of files directory to mount to uwebfilter.logcollect docker image"
  nullable    = false
}

variable "uwebfilter_db_redis_master_data" {
  type        = string
  description = "location of files directory to mount to uwebfilter.db.redis master docker image"
  nullable    = false
}

variable "uwebfilter_db_redis_replica_data" {
  type        = string
  description = "location of files directory to mount to uwebfilter.db.redis replica docker image"
  nullable    = false
}
