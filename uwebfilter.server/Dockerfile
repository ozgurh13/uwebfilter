FROM golang:1.23 AS builder
WORKDIR /build
ADD src.tar.gz .
RUN make release

FROM scratch
COPY --from=builder /build/server /server
CMD ["/server"]
