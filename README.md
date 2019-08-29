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
For an example on how to build the project, see how our [CMakeLists script](./CMakeLists.txt) builds the examples in the [examples subdirectory](./examples/).

To generate the build system with cmake, run:
```
mkdir build
cd build
# for windows (you can use any other generator of your choice)
cmake .. -G "Visual Studio 15 2017 Win64" -DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake -DCMAKE_CXX_STANDARD=17 -DVCPKG_TARGET_TRIPLET=x64-windows-static

# for linux
cmake .. -DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake -DCMAKE_CXX_STANDARD=17 -DCMAKE_BUILD_TYPE=Release # or Debug, RelWithDebInfo, MinSizeRel
```

To build an example (or your executable), like the `append-to-stream` example, run (from ./build):
```
# windows, see CMakeLists.txt file for build targets such as `append-to-stream`, `read-stream-event`, `delete-stream-event` and more to come
cmake --build . --target append-to-stream --config Release # or Debug, RelWithDebInfo, MinSizeRel

# linux
cmake --build . --target append-to-stream
```

### Running the examples

For usage information, execute the examples without arguments. For example, after building the `append-to-stream` examples, run:
```
# windows
./append-to-stream.exe

[2019-08-27 18:05:30.964] [stdout] [error] expected 5 arguments, got 0
[2019-08-27 18:05:30.964] [stdout] [error] usage: <executable> <ip endpoint> <port> <username> <password> [trace | debug | info | warn | error | critical | off]
[2019-08-27 18:33:20.657] [stdout] [error] example: ./append-to-stream 127.0.0.1 1113 admin changeit info
[2019-08-27 18:05:30.964] [stdout] [error] tool will write the following data to event store
[2019-08-27 18:05:30.964] [stdout] [error] stream=test-stream
        event-type=Test.Type
        is-json=true
        data={ "test": "data"}
        metadata=test metadata

# linux
$ ./append-to-stream
$ [2019-08-27 18:05:30.964] [stdout] [error] expected 5 arguments, got 0
$ [2019-08-27 18:05:30.964] [stdout] [error] usage: <executable> <ip endpoint> <port> <username> <password> [trace | debug | info | warn | error | critical | off]
$ [2019-08-27 18:33:20.657] [stdout] [error] example: ./append-to-stream 127.0.0.1 1113 admin changeit info
$ [2019-08-27 18:05:30.964] [stdout] [error] tool will write the following data to event store
$ [2019-08-27 18:05:30.964] [stdout] [error] stream=test-stream
$         event-type=Test.Type
$         is-json=true
$         data={ "test": "data"}
$         metadata=test metadata
```
where: 
- `endpoint` is the ipv4 address of the EventStore server
- `port` is the EventStore server's TCP listening port
- `username` and `password` should be valid credentials (use "admin" "changeit", EventStore creates that user by default)
- `verbosity` level should be specified

### Features

- operations on streams :heavy_check_mark:
- operations on stream metadata (without serialization) :heavy_check_mark:
- unit tests
- benchmarks
- subscriptions (IN PROGRESS)
- transactions :heavy_check_mark:
- connection configuration (retries, node preference, ssl, queue size, etc)
- user management
- cluster node discovery
- documentation
- more examples
- projections

*Note* : Only TCP communication is considered for the time being. EventStore also offers an HTTP api.

### Useful links

- [asio](https://www.boost.org/doc/libs/1_71_0/doc/html/boost_asio.html)
- [EventStore documentation](https://eventstore.org/docs/)
- [EventStore .NET Client Api](https://github.com/EventStore/EventStore) (see ClientAPI and ClusterNode projects for debugging/development)
- [protobuf for C++](https://developers.google.com/protocol-buffers/docs/cpptutorial)
- [cppreference](https://en.cppreference.com/w/)
- [stackoverflow](https://stackoverflow.com)
