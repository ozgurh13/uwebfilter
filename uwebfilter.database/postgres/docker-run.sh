docker run                                                        \
        --rm                                                      \
        --name postgres                                           \
        -v ./data:/var/lib/postgresql/data                        \
        -e POSTGRES_PASSWORD=password                             \
        -p 5432:5432                                              \
        uwebfilter.db.postgres
