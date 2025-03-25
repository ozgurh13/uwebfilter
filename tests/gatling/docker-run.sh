docker run --rm -it         \
    --name gatling          \
    --network=host          \
    -v .:/root/gatling      \
    ubuntu:24.04            \
    bash
