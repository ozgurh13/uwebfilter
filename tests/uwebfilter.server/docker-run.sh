docker run                                                   \
        --rm                                                 \
        -it                                                  \
        --network="host"                                     \
        -v .:/root/uwebfilter.server                         \
        ubuntu:24.04                                         \
        bash
