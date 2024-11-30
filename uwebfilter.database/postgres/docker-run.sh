docker run                                                        \
        --rm                                                      \
        --name uwebfilter.db.postgres                             \
        -v ./data:/var/lib/postgresql/data                        \
        -e POSTGRES_PASSWORD=password                             \
        -p 5432:5432                                              \
        uwebfilter.db.postgres
