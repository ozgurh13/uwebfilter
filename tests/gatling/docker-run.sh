docker run                                         \
        --rm                                       \
        -it                                        \
        --network="host"                           \
        -v .:/root/gatling                         \
        ubuntu:24.04                               \
        bash
