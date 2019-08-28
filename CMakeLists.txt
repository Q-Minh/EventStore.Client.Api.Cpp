cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project (event-store-client-api)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    include (cmake/msvc.cmake)
    configure_msvc_runtime()
endif()

find_package(asio CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(protobuf CONFIG REQUIRED)
find_package(boost REQUIRED COMPONENTS uuid container-hash)
find_package(Catch2 CONFIG REQUIRED)

set(_sources "")
list(APPEND _sources
	"src/messages.pb.cc"
)

add_library(${PROJECT_NAME} INTERFACE "")
target_sources(${PROJECT_NAME} 
INTERFACE
    # ${_sources}

    # headers
    # top-level
	include/all_events_slice.hpp
	include/append_to_stream.hpp
    include/connection_settings.hpp
	include/delete_stream.hpp
	include/delete_stream_result.hpp
    include/duration_conversions.hpp
	include/event_data.hpp
	include/event_read_result.hpp
    include/guid.hpp
    include/logger.hpp
    include/operations_manager.hpp
	include/position.hpp
	include/read_all_events.hpp
	include/read_all_events_backward.hpp
	include/read_all_events_forward.hpp
	include/read_direction.hpp
	include/read_stream_event.hpp
	include/read_stream_events.hpp
	include/read_stream_events_backward.hpp
	include/read_stream_events_forward.hpp
	include/recorded_event.hpp
	include/resolved_event.hpp
	include/stream_events_slice.hpp
    include/version.hpp
	include/write_result.hpp

	# buffer
	include/buffer/basic_mutable_buffer_sequence.hpp

    # connection
    include/connection/authentication_info.hpp
    include/connection/basic_tcp_connection.hpp
    include/connection/connection_phase.hpp
    include/connection/connection_state.hpp
    include/connection/constants.hpp
    include/connection/heartbeat_info.hpp
    include/connection/identity_info.hpp
    include/connection/node_preference.hpp
    include/connection/reconnection_info.hpp

	#error
	include/error/error.hpp

    # tcp
	include/tcp/authenticate.hpp
    include/tcp/connect.hpp
    include/tcp/discovery_service.hpp
    include/tcp/gossip_seed.hpp
    include/tcp/heartbeat.hpp
    include/tcp/identify.hpp
	include/tcp/operation.hpp
    include/tcp/read.hpp
    include/tcp/tcp_commands.hpp
    include/tcp/tcp_flags.hpp
    include/tcp/tcp_package.hpp

    # user
    include/user/user_credentials.hpp
)
target_include_directories(${PROJECT_NAME} 
INTERFACE
    include/
    ${Boost_INCLUDE_DIRS}
)
target_compile_definitions(${PROJECT_NAME}
INTERFACE
    BOOST_UUID_FORCE_AUTO_LINK
    _WIN32_WINNT=0x0A00
)
target_link_libraries(${PROJECT_NAME}
INTERFACE
    asio
    asio::asio
    spdlog::spdlog
    protobuf::libprotobuf
    protobuf::libprotobuf-lite
)
# set_target_properties(${PROJECT_NAME} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON) only if dll

# examples
function(build_executable executable entrypoint _sources)
    add_executable(${executable} ${entrypoint})
    # needed for protobuf
    target_sources(${executable}
    PRIVATE
        ${entrypoint}
        include/message/messages.pb.h
        ${_sources}
    )
    target_link_libraries(${executable}
    PRIVATE
        ${PROJECT_NAME}
    )
endfunction()

# build_executable(connect-to-es-01 "examples/connect-to-es-01.cpp" "${_sources}")
build_executable(append-to-stream "examples/append_to_stream.cpp" "${_sources}")
build_executable(read-stream-event "examples/read_stream_event.cpp" "${_sources}")
build_executable(delete-stream "examples/delete_stream.cpp" "${_sources}")
build_executable(read-stream-events "examples/read_stream_events.cpp" "${_sources}")
build_executable(read-all-events "examples/read_all_events.cpp" "${_sources}")

# tests
add_executable(unit-tests "tests/main.cpp")
target_sources(unit-tests
PRIVATE
	# source for protobuf
    ${_sources}

	# unit tests
	"tests/tcp/operations_manager.cpp"
	"tests/tcp/read.cpp"
    "tests/tcp/tcp_package.cpp"

	# headers
	"tests/mock_async_read_stream.hpp"
	"tests/mock_tcp_connection.hpp"
)
target_include_directories(unit-tests
PRIVATE
    include/
    ${Boost_INCLUDE_DIRS}
	"tests/"
)
target_compile_definitions(unit-tests
PRIVATE
    BOOST_UUID_FORCE_AUTO_LINK
)
target_link_libraries(unit-tests 
PRIVATE 
    Catch2::Catch2
    asio
    asio::asio
    spdlog::spdlog
    protobuf::libprotobuf
    protobuf::libprotobuf-lite
)