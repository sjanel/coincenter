# Multi stage build to separate docker build image from executable (to make the latter smaller)
FROM alpine:3.16.0 AS build

# Install base & build dependencies
RUN apk add g++ libc-dev curl-dev cmake ninja git

# Set default directory for application
WORKDIR /app

# Copy all files, excluding the ones in '.dockerignore'
COPY . .

# Create and go to 'bin' directory
WORKDIR /app/bin

# Declare and set default values of following arguments
ARG BUILD_MODE=Release
ARG BUILD_TEST=0
ARG BUILD_ASAN=0
ARG BUILD_WITH_PROMETHEUS=1

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
FROM alpine:3.16.0
# TODO: using alpine instead of scratch only to install the ca certificate. How could we install certificate from scratch?

# Install needed certificate for curl to work with https
RUN apk add ca-certificates && rm -rf /var/cache/apk/*

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
