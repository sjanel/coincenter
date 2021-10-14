# Multi stage build to separate docker build image from executable (to make the latter smaller)
FROM alpine:latest AS base-env

# Install base dependencies (especially run ones)
RUN apk update && apk upgrade && apk add gcc g++ libc-dev curl-dev bash

# Set default directory for application
WORKDIR /app

# Define a new temporary image with additional tools for build
FROM base-env as build

# Install build tools
RUN apk update && apk upgrade && apk add cmake ninja git linux-headers

# Copy all files, excluding the ones in '.dockerignore' (WARNING: secrets should never be shipped in a Docker image!)
COPY . .

# Create and go to 'build' directory
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
RUN if [[ "$TEST" == "1" ]]; then ctest; else echo "No tests"; fi

# Multi stage build to separate docker build image from executable (to make the latter smaller)
FROM base-env

# Copy config and data
COPY --from=build /app/config config
COPY --from=build /app/data data

WORKDIR /app/bin

# Copy executable
COPY --from=build /app/bin/coincenter .

CMD [ "/app/bin/coincenter" ]
