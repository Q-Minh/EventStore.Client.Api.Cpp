cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project (event-store-client-api)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    include (cmake/msvc.cmake)
    configure_msvc_runtime()
	set (_additional_win_compile_defs "_WIN32_WINNT=0x0A00")
endif()

# find_package(asio CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(protobuf CONFIG REQUIRED)
find_package(Boost REQUIRED)
find_package(OpenSSL)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Catch2 CONFIG REQUIRED)

set(_sources "")
list(APPEND _sources
	"src/messages.pb.cc"
)

add_library(${PROJECT_NAME} INTERFACE)
target_sources(${PROJECT_NAME} 
INTERFACE
    # ${_sources}

    # headers
    # top-level
	include/all_events_slice.hpp
	include/append_to_stream.hpp
	include/catchup_subscription_settings.hpp
	include/cluster_settings.hpp
	include/commit_transaction.hpp
	include/conditional_append_to_stream.hpp
	include/connection_result.hpp
    include/connection_settings.hpp
	include/create_persistent_subscription.hpp
	include/delete_persistent_subscription.hpp
	include/delete_stream.hpp
	include/delete_stream_result.hpp
    include/duration_conversions.hpp
	include/event_data.hpp
	include/event_read_result.hpp
	include/get_stream_metadata.hpp
    include/guid.hpp
    include/logger.hpp
	include/node_endpoints.hpp
    include/operations_map.hpp
	include/persistent_subscription_settings.hpp
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
	include/set_stream_metadata.hpp
	include/start_transaction.hpp
	include/stream_events_slice.hpp
	include/stream_metadata_result_raw.hpp
	include/transaction.hpp
	include/transactional_write.hpp
	include/update_persistent_subscription.hpp
	include/version.hpp
	include/write_result.hpp

	# buffer
	include/buffer/basic_mutable_buffer_sequence.hpp
	include/buffer/buffer_queue.hpp
	include/buffer/dynamic_buffer.hpp

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

	# error
	include/error/error.hpp

	# messages
	include/message/cluster_messages.hpp

	# system
	include/system/consumer_strategies.hpp
	include/system/metadata.hpp
	include/system/event_types.hpp
	include/system/streams.hpp

	# subscription
	include/subscription/catchup_all_subscription.hpp
	include/subscription/catchup_subscription.hpp
	include/subscription/persistent_subscription.hpp
	include/subscription/subscription_base.hpp
	include/subscription/volatile_subscription.hpp
  
    # tcp
	include/tcp/authenticate.hpp
	include/tcp/cluster_discovery_service.hpp
    include/tcp/connect.hpp
    include/tcp/basic_discovery_service.hpp
    include/tcp/gossip_seed.hpp
	include/tcp/handle_operation_error.hpp
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
    # ${Boost_INCLUDE_DIRS}
)
target_compile_definitions(${PROJECT_NAME}
INTERFACE
	BOOST_UUID_FORCE_AUTO_LINK
	BOOST_ERROR_CODE_HEADER_ONLY
	${_additional_win_compile_defs}
)
get_target_property(_compile_defs ${PROJECT_NAME} INTERFACE_COMPILE_DEFINITIONS)
message (STATUS "compile defs: ${_compile_defs}")

target_link_libraries(${PROJECT_NAME}
INTERFACE
    # asio
    # boost::asio::asio
	Boost::boost
    spdlog::spdlog
    protobuf::libprotobuf
    protobuf::libprotobuf-lite
	nlohmann_json 
	nlohmann_json::nlohmann_json
	$<$<BOOL:${OpenSSL_FOUND}>:OpenSSL::SSL OpenSSL::Crypto>
)
get_target_property(_link_libraries ${PROJECT_NAME} INTERFACE_LINK_LIBRARIES)
message (STATUS "link libraries: ${_link_libraries}")

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
	target_compile_definitions(${executable}
	PRIVATE
		_SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING
	)
endfunction()

# build_executable(connect-to-es-01 "examples/connect-to-es-01.cpp" "${_sources}")
build_executable(append-to-stream "examples/append_to_stream.cpp" "${_sources}")
build_executable(append-to-stream-cluster "examples/append_to_stream_cluster.cpp" "${_sources}")
build_executable(append-to-stream-batch "examples/append_to_stream_batch.cpp" "${_sources}")
build_executable(read-stream-event "examples/read_stream_event.cpp" "${_sources}")
build_executable(delete-stream "examples/delete_stream.cpp" "${_sources}")
build_executable(read-stream-events "examples/read_stream_events.cpp" "${_sources}")
build_executable(read-all-events "examples/read_all_events.cpp" "${_sources}")
build_executable(transaction "examples/transaction.cpp" "${_sources}")
build_executable(stream-metadata "examples/stream_metadata.cpp" "${_sources}")
build_executable(volatile-subscription "examples/volatile_subscription.cpp" "${_sources}")
build_executable(catchup-subscription "examples/catchup_subscription.cpp" "${_sources}")
build_executable(catchup-all-subscription "examples/catchup_all_subscription.cpp" "${_sources}")
build_executable(persistent-subscription "examples/persistent_subscription.cpp" "${_sources}")
build_executable(persistent-subscription-operations "examples/persistent_subscription_operations.cpp" "${_sources}")
build_executable(reconnection "examples/reconnection.cpp" "${_sources}")

# tests
add_executable(unit-tests "tests/main.cpp")
target_sources(unit-tests
PRIVATE
	# source for protobuf
    ${_sources}

	# unit tests
	"tests/tcp/operations_map.cpp"
	"tests/tcp/read.cpp"
    "tests/tcp/tcp_package.cpp"
	"tests/buffer/buffer_queue.cpp"

	# headers
	"tests/mock_async_read_stream.hpp"
	"tests/mock_tcp_connection.hpp"
)
target_include_directories(unit-tests
PRIVATE
	"tests/"
)
target_compile_definitions(unit-tests
PRIVATE
	_SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING
)
target_link_libraries(unit-tests 
PRIVATE 
	${PROJECT_NAME}
    Catch2::Catch2
)