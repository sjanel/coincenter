# Installation

<details><summary>Sections</summary>
<p>

- [Installation](#installation)
  - [Public docker image](#public-docker-image)
  - [Prerequisites](#prerequisites)
    - [Linux](#linux)
      - [Debian / Ubuntu](#debian--ubuntu)
      - [Alpine](#alpine)
    - [Windows](#windows)
  - [Build](#build)
    - [External libraries](#external-libraries)
    - [With cmake](#with-cmake)
      - [cmake build options](#cmake-build-options)
      - [As a static library](#as-a-static-library)
    - [Build with monitoring support](#build-with-monitoring-support)
    - [With Docker](#with-docker)
      - [Build](#build-1)
      - [Run](#run)

</p>
</details>

## Public docker image

If you don't want to build `coincenter` locally, you can just download the public docker image, corresponding to the latest version of branch `main`.

```
docker run -t sjanel/coincenter -h
```

Docker image does not contain additional `data` directory needed by `coincenter` (see [Configuration](#configuration))

To bind your 'data' directory from host to the docker container, you can use `--mount` option:

```
docker run --mount type=bind,source=<path-to-data-dir-on-host>,target=/app/data sjanel/coincenter
```

## Prerequisites

- **Git**
- **C++** compiler supporting C++20 (gcc >= 10, clang >= 13, MSVC >= 19.28).
- **CMake** >= 3.15
- **curl** >= 7.58.0 (it may work with an earlier version, it's just the minimum tested)
- **openssl** >= 1.1.0

### Linux

#### Debian / Ubuntu

```
sudo apt update && sudo apt install libcurl4-gnutls-dev libssl-dev cmake g++-10
```

#### Alpine

With `ninja` generator for instance:
```
sudo apk update && sudo apk upgrade && sudo apk add g++ libc-dev curl-dev cmake ninja git linux-headers
```

You can refer to the provided `Dockerfile` for more information.

### Windows

On Windows, the easiest method is to use [chocolatey](https://chocolatey.org/install) to install **curl** and **OpenSSL**:

```
choco install curl openssl
```

Then, locate where curl is installed (by default, should be in `C:\ProgramData\chocolatey\lib\curl\tools\curl-xxx`, let's note this `CURL_DIR`) and add both `CURL_DIR/lib` and `CURL_DIR/bin` in your `PATH`. From this step, **curl** and **OpenSSL** can be found by `cmake` and will be linked statically to the executables.

## Build

### External libraries

`coincenter` uses various external open source libraries. 
In all cases, they do not need to be installed. If they are not found at configure time, `cmake` will fetch sources and compile them automatically. If you are building frequently `coincenter` you can install them to speed up its compilation.

| Library                                                        | Description                                        |
| -------------------------------------------------------------- | -------------------------------------------------- |
| [amc](https://github.com/AmadeusITGroup/amc.git)               | High performance C++ containers (maintained by me) |
| [googletest](https://github.com/google/googletest.git)         | Google Testing and Mocking Framework               |
| [json](https://github.com/nlohmann/json)                       | JSON for Modern C++                                |
| [spdlog](https://github.com/gabime/spdlog.git)                 | Fast C++ logging library                           |
| [prometheus-cpp](https://github.com/jupp0r/prometheus-cpp.git) | Prometheus Client Library for Modern C++           |
| [jwt-cpp](https://github.com/Thalhammer/jwt-cpp)               | Creating and validating json web tokens in C++     |

### With cmake

This is a **C++20** project.

The following compilers and their versions have been tested (and are tested in the CI):
 - GCC version >= 10
 - Clang version >= 13
 - MSVC version >= 19.28

Other compilers have not been tested yet.

#### cmake build options

| Option             | Default              | Description                                     |
| ------------------ | -------------------- | ----------------------------------------------- |
| `CCT_ENABLE_TESTS` | `ON` if main project | Build and launch unit tests                     |
| `CCT_BUILD_EXEC`   | `ON` if main project | Build an executable instead of a static library |
| `CCT_ENABLE_ASAN`  | `ON` if Debug mode   | Compile with AddressSanitizer                   |

Example on Linux: to compile it in `Release` mode and `ninja` generator
```
mkdir -p build && cd build && cmake -GNinja -DCMAKE_BUILD_TYPE=Release .. && ninja
```

On Windows, you can use your preferred IDE to build `coincenter` (**Visual Studio Code**, **Visual Studio 2019**, etc), or build it from command line, with generator `-G "Visual Studio 16 2019"`. Refer to the GitHub Windows workflow to have the detailed installation steps.

#### As a static library

**coincenter** can also be used as a sub project, such as a trading bot for instance. It is the case by default if built as a sub-module in `cmake`.

To build your `cmake` project with **coincenter** library, you can do it with `FetchContent`:
```
include(FetchContent)

FetchContent_Declare(
  coincenter
  GIT_REPOSITORY https://github.com/sjanel/coincenter.git
  GIT_TAG        main
)

FetchContent_MakeAvailable(coincenter)
```
Then, a static library named `coincenter` is defined and you can link it as usual:
```
target_link_libraries(<MyProgram> PRIVATE coincenter)
```

### Build with monitoring support

You can build `coincenter` with [prometheus-cpp](https://github.com/jupp0r/prometheus-cpp) if needed. 
If you have it installed on your machine, `cmake` will link coincenter with it. Otherwise you can still activate `cmake` flag `CCT_BUILD_PROMETHEUS_FROM_SRC` (default to OFF) to build it automatically from sources with `FetchContent`.

### With Docker

A **Docker** image is hosted in the public **Docker hub** registry with the name *sjanel/coincenter*, corresponding to latest successful build of `main` branch by the CI.

You can create your own **Docker** image of `coincenter`. It uses **Alpine** Linux distribution as base and multi stage build to reduce the image size.
Build options (all optional):

CMake build mode
`BUILD_MODE` (default: Release)

Compile and launch tests
`BUILD_TEST` (default: 0)

Activate Address Sanitizer
`BUILD_ASAN` (default: 0)

Compile with [prometheus-cpp](https://github.com/jupp0r/prometheus-cpp) to support metric export to Prometheus
`BUILD_WITH_PROMETHEUS` (default: 1)

#### Build

```
docker build --build-arg BUILD_MODE=Release -t local-coincenter .
```

#### Run

```
docker run -ti -e "TERM=xterm-256color" local-coincenter --help
```