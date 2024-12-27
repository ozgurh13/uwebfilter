resource "docker_image" "uwebfilter_server-image" {
  name         = "uwebfilter.server"
  keep_locally = true
}

resource "docker_image" "uwebfilter_logcollect-image" {
  name         = "uwebfilter.logcollect"
  keep_locally = true
}

resource "docker_image" "uwebfilter_categorify-image" {
  name         = "uwebfilter.categorify"
  keep_locally = true
}

resource "docker_image" "uwebfilter_db_postgres-image" {
  name         = "uwebfilter.db.postgres"
  keep_locally = true
}

resource "docker_image" "uwebfilter_db_redis-master-image" {
  name         = "uwebfilter.db.redis"
  keep_locally = true
}

resource "docker_image" "uwebfilter_db_redis-replica-image" {
  name         = "uwebfilter.db.redis"
  keep_locally = true
}





resource "docker_container" "uwebfilter_server" {
  name         = "uwebfilter.server"
  image        = docker_image.uwebfilter_server-image.image_id
  network_mode = "host"

  depends_on = [
    docker_container.uwebfilter_db_redis-replica,
    docker_container.uwebfilter_db_postgres,
  ]
}

resource "docker_container" "uwebfilter_logcollect" {
  name         = "uwebfilter.logcollect"
  image        = docker_image.uwebfilter_logcollect-image.image_id
  network_mode = "host"

  volumes {
    container_path = "/files"
    host_path      = var.uwebfilter_logcollect_files
    read_only      = true
  }

  depends_on = [
    docker_container.uwebfilter_db_postgres,
  ]
}

resource "docker_container" "uwebfilter_categorify" {
  name  = "uwebfilter.categorify"
  image = docker_image.uwebfilter_categorify-image.image_id
  ports {
    internal = 9005
    external = 9005
  }
}

resource "docker_container" "uwebfilter_db_postgres" {
  name  = "uwebfilter.db.postgres"
  image = docker_image.uwebfilter_db_postgres-image.image_id

  ports {
    internal = 5432
    external = 5432
  }

  env = ["POSTGRES_PASSWORD=password"]
}

resource "docker_container" "uwebfilter_db_redis-master" {
  name         = "uwebfilter.db.redis-master"
  image        = docker_image.uwebfilter_db_redis-master-image.image_id
  network_mode = "host"

  volumes {
    container_path = "/data"
    host_path      = var.uwebfilter_db_redis_master_data
  }
}

resource "docker_container" "uwebfilter_db_redis-replica" {
  name         = "uwebfilter.db.redis-replica"
  image        = docker_image.uwebfilter_db_redis-replica-image.image_id
  network_mode = "host"

  volumes {
    container_path = "/data"
    host_path      = var.uwebfilter_db_redis_replica_data
  }
}
