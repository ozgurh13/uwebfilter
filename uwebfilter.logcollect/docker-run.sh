docker run                               \
        --rm                             \
        --name uwebfilter.logcollect     \
        --network="host"                 \
        -v ./files:/files                \
        uwebfilter.logcollect
