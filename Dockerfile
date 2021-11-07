#1 Multi stage build to separate docker build image from executable (to make the latter smaller)
FROM alpine:3.14.2 AS base-env

# Install base dependencies (especially run ones)
RUN apk add g++ libc-dev curl-dev

# Set default directory for application
WORKDIR /app

# Define a new temporary image with additional tools for build
FROM base-env as build

# Install build tools. (git is needed for cmake FetchContent)
RUN apk add cmake ninja git linux-headers

# Copy all files, excluding the ones in '.dockerignore' (WARNING: secrets should never be shipped in a Docker image!)
COPY . .

# Create and go to 'bin' directory
WORKDIR /app/bin

# Declare and set default values of following arguments
ARG BUILD_MODE=Release
ARG BUILD_TEST=0
ARG BUILD_ASAN=0
ARG BUILD_WITH_PROMETHEUS=1

# Create ninja file
RUN cmake -DCMAKE_BUILD_TYPE=${BUILD_MODE} \
    -DCCT_ENABLE_TESTS=${BUILD_TEST} \
    -DCCT_ENABLE_ASAN=${BUILD_ASAN} \
    -DCCT_BUILD_PROMETHEUS_FROM_SRC=${BUILD_WITH_PROMETHEUS} \
    -GNinja ..

# Compile
RUN ninja

# Launch tests if requested
RUN if [ "$BUILD_TEST" = "1" -o "$BUILD_TEST" = "ON" ]; then \
    ctest -j 2 --output-on-failure; \
    else \
    echo "No tests"; \
    fi

# Grasp all libraries required by executable and copy them to 'deps'
RUN ldd coincenter | tr -s '[:blank:]' '\n' | grep '^/' | xargs -I % sh -c 'mkdir -p $(dirname deps%); cp % deps%;'

# Multi stage build to separate docker build image from executable (to make the latter smaller)
FROM scratch

# Copy the dependencies from executable to new scratch image, keeping same path
COPY --from=build /app/bin/deps /

# Copy executable
COPY --from=build /app/bin/coincenter /app/coincenter

# Docker image will only contain the executable and its dependencies, not the 'data' directory (including config and secrets).
# It should be mounted when launching the container.
# To do this, you can use --mount option to bind host 'data' directory inside a container:
# docker run --mount type=bind,source=<path-to-data-dir-on-host>,target=/app/data sjanel/coincenter
ENTRYPOINT [ "/app/coincenter" ]
