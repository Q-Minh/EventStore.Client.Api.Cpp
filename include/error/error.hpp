#pragma once

#ifndef ES_ERROR_HPP
#define ES_ERROR_HPP

#include <system_error>

namespace es {

enum class connection_errors
{
	// cannot start at 0, 0 is used as default success code in std
	authentication_failed = 1,
	identification_failed = 2,
	operation_timeout = 3,
	heartbeat_timeout = 4,
	max_reconnections = 5,
	max_operation_retries = 6,
	endpoint_discovery = 7,
	unexpected_response = 8,
	connection_closed = 9,
	authentication_timeout = 10
};

enum class communication_errors
{
	bad_length_prefix = 1,
	unexpected_response = 2,
	server_error = 3
};

enum class stream_errors
{
	wrong_expected_version = 1,
	stream_deleted = 2,
	access_denied = 3,
	invalid_transaction = 4,
	stream_not_found = 5,
	version_mismatch = 6
};

namespace error {

struct connection_category : public std::error_category
{
	const char* name() const noexcept override
	{
		return "eventstore.connection";
	}

	std::string message(int e) const override
	{
		switch (static_cast<connection_errors>(e))
		{
		case connection_errors::authentication_failed:
			return "failed to authenticate to server";
		case connection_errors::identification_failed:
			return "failed to identify to server";
		case connection_errors::operation_timeout:
			return "request timed out";
		case connection_errors::heartbeat_timeout:
			return "server failed to respond to heartbeat request";
		case connection_errors::max_reconnections:
			return "reached maximum number of reconnection attempts to server";
		case connection_errors::max_operation_retries:
			return "reached maximum number of retries for operation";
		case connection_errors::endpoint_discovery:
			return "failed to discover nodes";
		case connection_errors::unexpected_response:
			return "server response is not coherent with client request";
		case connection_errors::connection_closed:
			return "connection to server terminated";
		case connection_errors::authentication_timeout:
			return "authentication timed out";
		default:
			return "unknown error";
		}
	}
};

struct communication_category : public std::error_category
{
	const char* name() const noexcept override
	{
		return "eventstore.communication";
	}

	std::string message(int e) const override
	{
		switch (static_cast<communication_errors>(e))
		{
		case communication_errors::bad_length_prefix:
			return "tcp length-prefixed frame had invalid message length";
		case communication_errors::unexpected_response:
			return "server response could not be understood";
		case communication_errors::server_error:
			return "error on server side";
		default:
			return "unknown error";
		}
	}
};

struct stream_category : public std::error_category
{
	const char* name() const noexcept override
	{
		return "eventstore.stream";
	}

	std::string message(int e) const override
	{
		switch (static_cast<stream_errors>(e))
		{
		case stream_errors::access_denied:
			return "access denied for stream";
		case stream_errors::invalid_transaction:
			return "invalid transaction";
		case stream_errors::stream_deleted:
			return "stream has been deleted";
		case stream_errors::wrong_expected_version:
			return "wrong expected version, stream was updated before this operation or did not exist";
		case stream_errors::stream_not_found:
			return "the stream was not found";
		case stream_errors::version_mismatch:
			return "the expected version does not match actual stream version";
		default:
			return "unknown error";
		}
	}
};

inline const connection_category& get_connection_category()
{
	static connection_category conn_category{};
	return conn_category;
}

inline const communication_category& get_communication_category()
{
	static communication_category comm_category{};
	return comm_category;
}

inline const stream_category& get_stream_category()
{
	static stream_category str_category{};
	return str_category;
}

} // error

inline std::error_code make_error_code(es::connection_errors ec) noexcept
{
	return std::error_code{ static_cast<int>(ec), es::error::get_connection_category() };
}

inline std::error_code make_error_code(es::communication_errors ec) noexcept
{
	return std::error_code{ static_cast<int>(ec), es::error::get_communication_category() };
}

inline std::error_code make_error_code(es::stream_errors ec) noexcept
{
	return std::error_code{ static_cast<int>(ec), es::error::get_stream_category() };
}

static const std::error_category& connection_category = error::get_connection_category();
static const std::error_category& communication_category = error::get_communication_category();
static const std::error_category& stream_category = error::get_stream_category();

} // es

namespace std {

template <>
struct is_error_code_enum<es::connection_errors> : true_type {};

template <>
struct is_error_code_enum<es::communication_errors> : true_type {};

template <>
struct is_error_code_enum<es::stream_errors> : true_type {};

} // std

#endif // ES_ERROR_HPP