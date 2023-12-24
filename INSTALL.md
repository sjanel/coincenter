# Installation

<details><summary>Sections</summary>
<p>

- [Installation](#installation)
  - [Public docker image](#public-docker-image)
  - [Prerequisites](#prerequisites)
    - [Linux](#linux)
      - [Debian / Ubuntu](#debian--ubuntu)
      - [Alpine](#alpine)
    - [With a package manager on any platform (easiest solution on Windows)](#with-a-package-manager-on-any-platform-easiest-solution-on-windows)
      - [vcpkg](#vcpkg)
      - [conan](#conan)
  - [Build](#build)
    - [External libraries](#external-libraries)
    - [With cmake](#with-cmake)
      - [cmake build options](#cmake-build-options)
      - [As a static library](#as-a-static-library)
    - [Build with monitoring support](#build-with-monitoring-support)
    - [With Docker](#with-docker)
      - [Docker Build](#docker-build)
      - [Docker Run](#docker-run)
  - [Tests](#tests)

</p>
</details>

Unless explicitly stated otherwise, all commands detailed in next sections are to be run from the top level directory of `coincenter` repository.

## Public docker image

If you don't want to build `coincenter` locally, you can just download the public docker image, corresponding to the latest version of branch `main`.

```bash
docker run -t sjanel/coincenter -h
```

Docker image does not contain additional `data` directory needed by `coincenter` (see [Configuration](CONFIG.md))

To bind your 'data' directory from host to the docker container, you can use `--mount` option:

```bash
docker run --mount type=bind,source=<path-to-data-dir-on-host>,target=/app/data sjanel/coincenter
```

## Prerequisites

This is a **C++20** project.

It's still currently (as of beginning of 2023) not fully supported by most compilers (MSVC does, GCC is very close, see [here](https://en.cppreference.com/w/cpp/compiler_support)).

But they have partial support which is sufficient to build `coincenter`.

The following compilers are known to compile `coincenter` (and are tested in the CI):

- **GCC** version >= 11
- **Clang** version >= 18
- **MSVC** version >= 19.39

Other compilers have not been tested.

In addition, the basic minimum requirements are:

- **Git**
- **CMake** >= 3.15
- **curl** >= 7.58.0 (it may work with an earlier version, it's just the minimum tested on **Ubuntu 18**)
- **openssl** >= 1.1.0

### Linux

#### Debian / Ubuntu

Provided that your distribution is sufficiently recent, meta package `build-essential` should provide `gcc` with the correct version.
Otherwise you can still force it:

```bash
sudo apt update
sudo apt install build-essential ninja-build zlib1g-dev libcurl4-openssl-dev libssl-dev cmake git ca-certificates
```

You can refer to the provided [Dockerfile](Dockerfile) for more information.

#### Alpine

With `ninja` generator for instance:

```bash
sudo apk add --update --upgrade g++ libc-dev zlib-dev openssl-dev curl-dev cmake ninja git ca-certificates
```

You can refer to the provided [Dockerfile](alpine.Dockerfile) for more information.

### With a package manager on any platform (easiest solution on Windows)

#### [vcpkg](https://vcpkg.io/en/)

The vcpkg manifest [vcpkg.json](vcpkg.json) defines needed dependencies. You can install them manually with (optional, it will be done automatically by **cmake** at configure time):

```bash
vcpkg install
```

From this step, the dependencies can be found by `cmake` with `find_package` by giving the toolchain file of `vcpkg` at configure time:

```bash
cmake -DCMAKE_TOOLCHAIN_FILE="<vcpkg-root-dir>/scripts/buildsystems/vcpkg.cmake" <usual-cmake-arguments>
```

You can refer to the [Windows workflow](.github/workflows/windows.yml) and the [vcpkg install page](https://vcpkg.io/en/getting-started?platform=windows) for more information.

#### [conan](https://conan.io/)

It's very similar than with vcpkg. The build process is described in details in [this page](https://docs.conan.io/2/tutorial/consuming_packages/build_simple_cmake_project.html).

```bash
# If not already done - add profile to your system
conan profile detect --force

# install libraries defined in conanfile.txt
conan install . --output-folder=build --build=missing
```

If your **cmake** version is >= 3.23, then simply run:

```bash
cmake -S . -B build --preset conan-release
```

## Build

### External libraries

`coincenter` uses various external open source libraries.
In all cases, they do not need to be installed. If they are not found at configure time, `cmake` will fetch sources and compile them automatically.
If you are building frequently `coincenter` you can install them to speed up its compilation.

| Library                                                        | Description                                        | License              |
| -------------------------------------------------------------- | -------------------------------------------------- | -------------------- |
| [amc](https://github.com/AmadeusITGroup/amc.git)               | High performance C++ containers (maintained by me) | MIT                  |
| [googletest](https://github.com/google/googletest.git)         | Google Testing and Mocking Framework               | BSD-3-Clause License |
| [json](https://github.com/nlohmann/json)                       | JSON for Modern C++                                | MIT                  |
| [spdlog](https://github.com/gabime/spdlog.git)                 | Fast C++ logging library                           | MIT                  |
| [prometheus-cpp](https://github.com/jupp0r/prometheus-cpp.git) | Prometheus Client Library for Modern C++           | MIT                  |
| [jwt-cpp](https://github.com/Thalhammer/jwt-cpp)               | Creating and validating json web tokens in C++     | MIT                  |

### With cmake

This project can be easily built with [cmake](https://cmake.org/).

The minimum tested version is cmake `3.15`, but it's recommended that you use the latest available one.

#### cmake build options

| Option                  | Default                                                | Description                                     |
| ----------------------- | ------------------------------------------------------ | ----------------------------------------------- |
| `CCT_ENABLE_TESTS`      | `ON` if main project                                   | Build and launch unit tests                     |
| `CCT_BUILD_EXEC`        | `ON` if main project                                   | Build an executable instead of a static library |
| `CCT_ENABLE_ASAN`       | `ON` if Debug mode                                     | Compile with AddressSanitizer                   |
| `CCT_ENABLE_CLANG_TIDY` | `ON` if Debug mode and `clang-tidy` is found in `PATH` | Compile with clang-tidy checks                  |
| `CCT_ENABLE_PROTO`      | `ON`                                                   | Compile with protobuf support                   |

Example on Linux: to compile it in `Release` mode and `ninja` generator

```bash
mkdir -p build
cd build
cmake -GNinja -DCMAKE_BUILD_TYPE=Release ..
ninja
```

On Windows, you can use your preferred IDE to build `coincenter` (**Visual Studio Code**, **Visual Studio 2022**, etc), or build it from command line, with generator `-G "Visual Studio 17 2022"` for instance. Refer to the GitHub Windows workflow to have the detailed installation steps.

#### As a static library

**coincenter** can also be used as a sub project, such as a trading bot for instance. It is the case by default if built as a sub-module in `cmake`.

To build your `cmake` project with **coincenter** library, you can do it with `FetchContent`:

```cmake
include(FetchContent)

FetchContent_Declare(
  coincenter
  GIT_REPOSITORY https://github.com/sjanel/coincenter.git
  GIT_TAG        main
)

FetchContent_MakeAvailable(coincenter)
```

Then, a static library named `coincenter` is defined and you can link it as usual:

```cmake
target_link_libraries(<MyProgram> PRIVATE coincenter)
```

### Build with monitoring support

You can build `coincenter` with [prometheus-cpp](https://github.com/jupp0r/prometheus-cpp) if needed.
If you have it installed on your machine, `cmake` will link coincenter with it. Otherwise you can still activate `cmake` flag `CCT_BUILD_PROMETHEUS_FROM_SRC` (default to OFF) to build it automatically from sources with `FetchContent`.

### With Docker

A **Docker** image is hosted in the public **Docker hub** registry with the name *sjanel/coincenter*, corresponding to latest successful build of `main` branch by the CI.

It is built in two flavors:

- **Ubuntu** based (default)
- **Alpine** based (tagged `alpine` for the latest version and fixed versions with `-alpine` suffixes)

You can create your own **Docker** image of `coincenter`.
Build options (all optional):

CMake build mode
`BUILD_MODE` (default: Release)

Compile and launch tests
`BUILD_TEST` (default: 0)

Activate Address Sanitizer
`BUILD_ASAN` (default: 0)

Compile with [prometheus-cpp](https://github.com/jupp0r/prometheus-cpp) to support metric export to Prometheus
`BUILD_WITH_PROMETHEUS` (default: 1)

#### Docker Build

```bash
docker build --build-arg BUILD_MODE=Release -t local-coincenter .
```

#### Docker Run

```bash
docker run -ti -e "TERM=xterm-256color" local-coincenter --help
```

## Tests

Tests are compiled only if `coincenter` is built as a main project by default. You can set `cmake` flag `CCT_ENABLE_TESTS` to 1 or 0 to change this behavior.

Note that exchanges API are also unit tested. If no private key is found, only public exchanges will be tested, private exchanges will be skipped and unit test will not fail.
