FROM alpine:3.19.1 AS build

# Declare and set default values of following arguments
ARG BUILD_MODE=Release
ARG BUILD_TEST=0
ARG BUILD_ASAN=0
ARG BUILD_WITH_PROMETHEUS=1

# Install base & build dependencies, needed certificates for curl to work with https
RUN apk add --update --upgrade --no-cache g++ libc-dev openssl-dev curl-dev cmake ninja git ca-certificates

# Copy source files
WORKDIR /app/src
COPY src .

WORKDIR /app
COPY CMakeLists.txt *.md ./

WORKDIR /app/cmake
COPY cmake .

# Copy only the test secrets
WORKDIR /app/data/secret
COPY data/secret/secret_test.json .

# Copy mandatory static configuration files
WORKDIR /app/data/static
COPY data/static/currency_prefix_translator.json data/static/currencyacronymtranslator.json data/static/stablecoins.json data/static/withdrawfees.json ./

# Copy fiats cache (needed in tests to avoid live requests)
WORKDIR /app/data/cache
COPY data/cache/fiatcache.json ./

# Create and go to 'bin' directory
WORKDIR /app/bin

# Build and launch tests if any
RUN cmake -DCMAKE_BUILD_TYPE=${BUILD_MODE} \
    -DCCT_ENABLE_TESTS=${BUILD_TEST} \
    -DCCT_ENABLE_ASAN=${BUILD_ASAN} \
    -DCCT_BUILD_PROMETHEUS_FROM_SRC=${BUILD_WITH_PROMETHEUS} \
    -GNinja .. && \
    ninja && \
    if [ "$BUILD_TEST" = "1" -o "$BUILD_TEST" = "ON" ]; then \
    ctest -j 2 --output-on-failure; \
    fi

# Grasp all libraries required by executable and copy them to 'deps'
RUN ldd coincenter | tr -s '[:blank:]' '\n' | grep '^/' | xargs -I % sh -c 'mkdir -p $(dirname deps%); cp % deps%;'

# Multi stage build to separate docker build image from executable (to make the latter smaller)
FROM scratch

# Copy certificates
COPY --from=build /etc/ssl/certs /etc/ssl/certs

# Copy the dependencies from executable to new scratch image, keeping same path
COPY --from=build /app/bin/deps /

# Copy the default data directory (can be overriden by host one with mount)
COPY --from=build /app/data /app/data

# Copy executable
COPY --from=build /app/bin/coincenter /app/coincenter

# 'data' directory of host machine can be mounted when launching the container.
# To do this, you can use --mount option:
# docker run --mount type=bind,source=<path-to-data-dir-on-host>,target=/app/data sjanel/coincenter
ENTRYPOINT [ "/app/coincenter" ]
