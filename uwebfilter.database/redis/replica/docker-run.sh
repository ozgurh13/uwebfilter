docker run                                                        \
        --rm                                                      \
        --name uwebfilter.db.redis-replica                        \
        --network="host"                                          \
        -v ./redis.conf:/data/redis.conf                          \
        uwebfilter.db.redis
