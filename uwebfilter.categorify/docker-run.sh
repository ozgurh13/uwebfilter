docker run                                   \
        --rm                                 \
        -p 9005:9005                         \
        -v ./models:/app/models              \
        uwebfilter.categorify
