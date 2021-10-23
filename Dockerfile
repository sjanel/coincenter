# Multi stage build to separate docker build image from executable (to make the latter smaller)
FROM alpine AS base-env

# Install base dependencies (especially run ones)
RUN apk update && apk upgrade && apk add g++ libc-dev curl-dev

# Set default directory for application
WORKDIR /app

# Define a new temporary image with additional tools for build
FROM base-env as build

# Install build tools
RUN apk update && apk upgrade && apk add cmake ninja git linux-headers

# Copy all files, excluding the ones in '.dockerignore' (WARNING: secrets should never be shipped in a Docker image!)
COPY . .

# Create and go to 'bin' directory
WORKDIR /app/bin

# Declare and set default values of following arguments
ARG BUILD_MODE=Release
ARG TEST=0
ARG ASAN=0

# Create ninja file
RUN cmake -DCMAKE_BUILD_TYPE=${BUILD_MODE} -DCCT_ENABLE_TESTS=${TEST} -DCCT_ENABLE_ASAN=${ASAN} -GNinja ..

# Compile
RUN ninja

# Launch tests if requested
RUN if [ "$TEST" = "1" -o "$TEST" = "ON" ]; then ctest -j 2 --output-on-failure; else echo "No tests"; fi

# Multi stage build to separate docker build image from executable (to make the latter smaller)
FROM base-env

WORKDIR /app/bin

# Copy executable
COPY --from=build /app/bin/coincenter .

# Docker image will only contain the executable and its dependencies, not the 'data' directory (including config and secrets).
# It should be mounted when launching the container.
# To do this, you can use --mount option to bind host 'data' directory inside a container:
# docker run --mount type=bind,source=<path-to-data-dir-on-host>,target=/app/data sjanel/coincenter
ENTRYPOINT [ "/app/bin/coincenter" ]
