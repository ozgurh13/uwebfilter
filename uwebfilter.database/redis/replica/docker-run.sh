docker run                                                        \
        --rm                                                      \
        --name uwebfilter.db.redis-replica                        \
        --network="host"                                          \
        -v ./data:/data                                           \
        uwebfilter.db.redis
