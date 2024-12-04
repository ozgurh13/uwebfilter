OUT=server
DOCKER_IMAGE_NAME=uwebfilter.server

all:
	CGO_ENABLED=0 GOOS=linux GOARCH=amd64                \
	        go build                                     \
	                -o $(OUT)                            \
	                -trimpath                            \
	                -gcflags=all="-l -B"                 \
	                -ldflags="-w -s -buildid="           \
	                ./cmd/server/.

release: all
	./upx --best --ultra-brute $(OUT)

fmt:
	go fmt ./**/**

test:
	go test ./**/**

package:
	tar czvf src.tar.gz                       \
	        cmd pkg Makefile go.mod go.sum upx

docker-image:
	docker build -t $(DOCKER_IMAGE_NAME) .

docker-all: package docker-image
