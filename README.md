# EventStore C++ Client Header-Only Library

### Introduction

An [Asio (standalone version)](https://www.boost.org/doc/libs/1_71_0/doc/html/boost_asio.html) based C++ client library for [EventStore](https://eventstore.org/).

### Prerequisites

- a C++ 17 compiler (subject to change for C++11 or C++14)
- cmake >= 3.14 (or build by hand)
- protoc (protobuf compiler) if you want to regenerate the message implementation

### Dependencies

- [protobuf](https://developers.google.com/protocol-buffers/)
- [standalone asio](https://www.boost.org/doc/libs/1_71_0/doc/html/boost_asio.html)
- [spdlog](https://github.com/gabime/spdlog) (for library maintainers)
- [Boost.Uuid](https://www.boost.org/doc/libs/1_71_0/libs/uuid/doc/index.html)
- [Boost.ContainerHash](https://www.boost.org/doc/libs/1_71_0/doc/html/hash.html)
- [Catch2](https://github.com/catchorg/Catch2) (for tests only)

### Help installing dependencies

You can use [vcpkg](https://github.com/microsoft/vcpkg) for dependency management. 

```
# for windows
./vcpkg install protobuf:x64-windows-static asio:x64-windows-static spdlog:x64-windows-static boost-uuid:x64-windows-static boost-container-hash:x64-windows-static catch2:x64-windows-static

# for linux
./vcpkg install protobuf asio spdlog boost-uuid boost-container-hash catch2
```

### Building

There are currently no imported targets and no custom FindEventStoreClientApiCpp.cmake module.
For an example on how to build the project, see how our [CMakeLists scripts](./CMakeLists.txt) builds the examples in the [examples subdirectory](./examples/).

To generate the build system with cmake, run:
```
mkdir build
cd build
# for windows (you can use any other generator of your choice)
cmake .. -G "Visual Studio 15 2017 Win64" -DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake -DCMAKE_CXX_STANDARD=17 -DVCPKG_TARGET_TRIPLET=x64-windows-static

# for linux
cmake .. -DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake -DCMAKE_CXX_STANDARD=17 -DCMAKE_BUILD_TYPE=Release # or Debug, RelWithDebInfo, MinSizeRel
```

To build an example (or your executable), like the connect-to-es-01 example, run (from ./build):
```
# windows, see CMakeLists.txt file for build targets such as "connect-to-es-01"
cmake --build . --target connect-to-es-01 --config Release # or Debug, RelWithDebInfo, MinSizeRel

# linux
cmake --build . --target connect-to-es-01
```
### Features

Library is far from complete, roadmap is approximately :

- operations on streams
- operations on stream metadata
- user management
- subscriptions
- projections

*Note* : Only TCP communication is considered for the time being. EventStore also offers an HTTP api.

### Useful links

- [asio](https://www.boost.org/doc/libs/1_71_0/doc/html/boost_asio.html)
- [EventStore documentation](https://eventstore.org/docs/)
- [EventStore .NET Client Api](https://github.com/EventStore/EventStore) (see ClientAPI and ClusterNode projects for debugging/development)
- [protobuf for C++](https://developers.google.com/protocol-buffers/docs/cpptutorial)
- [always need cppreference](https://en.cppreference.com/w/)
- [always need stackoverflow](https://stackoverflow.com)