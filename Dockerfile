FROM alpine:3.8 AS build

RUN set -x && \
    apk add \
        build-base \
        cmake

COPY . /src

RUN set -x && \
    cd src && \
    rm -rf build && mkdir -p build && cd build && \
    cmake .. && make


FROM alpine:3.8

WORKDIR /opt/tssd

COPY --from=build /src/build/tssd ./

RUN set -x && \
    apk add libstdc++

CMD ["./tssd", "--dont_d"]
