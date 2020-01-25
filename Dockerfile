FROM madduci/docker-cpp-env:latest AS build
WORKDIR /src
COPY . ./
RUN cmake . && make

FROM alpine
RUN apk --update add libgcc libstdc++
COPY --from=build /src/tssd ./
CMD ./tssd --dont_d
