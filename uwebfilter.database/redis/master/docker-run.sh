docker run                                                        \
        --rm                                                      \
        --name uwebfilter.db.redis-master                         \
        --network="host"                                          \
        -v ./data:/data                                           \
        uwebfilter.db.redis
