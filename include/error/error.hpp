#pragma once

#ifndef ES_ERROR_HPP
#define ES_ERROR_HPP

#include <boost/system/error_code.hpp>

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
	server_error = 3,
	bad_request = 4
};

enum class operation_errors
{
	not_authenticated = 1,
	not_handled = 2,
	not_ready = 3,
	too_busy = 4,
	not_master = 5,
	server_timeout = 6
};

enum class stream_errors
{
	wrong_expected_version = 1,
	stream_deleted = 2,
	access_denied = 3,
	invalid_transaction = 4,
	stream_not_found = 5,
	version_mismatch = 6,
	transaction_committed = 7,
	transaction_rolled_back = 8
};

enum class subscription_errors
{
	unsubscribed = 1,
	access_denied = 2,
	not_found = 3,
	unknown = 4,
	not_authenticated = 5,
	subscription_request_not_sent = 6,
	bad_subscription_confirmation = 7,
	persistent_subscription_deleted = 8,
	subscriber_max_count_reached = 9,
	persistent_subscription_already_exists = 10,
	persistent_subscription_fail = 11,
	persistent_subscription_does_not_exist = 12
};

namespace error {

struct connection_category : public boost::system::error_category
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

struct communication_category : public boost::system::error_category
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
		case communication_errors::bad_request:
			return "bad request";
		default:
			return "unknown error";
		}
	}
};

struct operation_category : public boost::system::error_category
{
	const char* name() const noexcept override
	{
		return "eventstore.operation";
	}

	std::string message(int e) const override
	{
		switch (static_cast<operation_errors>(e))
		{
		case operation_errors::not_authenticated:
			return "not authenticated for this operation";
		case operation_errors::not_handled:
			return "request was not handled by this cluster node";
		case operation_errors::not_ready:
			return "server was not ready, should retry";
		case operation_errors::too_busy:
			return "server was too busy, should retry";
		case operation_errors::not_master:
			return "cluster node is not master and require master was true, attempting to reconnect";
		case operation_errors::server_timeout:
			return "server timed out on prepare timeout, commit timeout or forward timeout, should retry";
		default:
			return "unknown error";
		}
	}
};

struct stream_category : public boost::system::error_category
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
		case stream_errors::transaction_committed:
			return "the transaction was already committed";
		case stream_errors::transaction_rolled_back:
			return "the transaction was already rolled back";
		default:
			return "unknown error";
		}
	}
};

struct subscription_category : public boost::system::error_category
{
	const char* name() const noexcept override
	{
		return "eventstore.subscription";
	}

	std::string message(int e) const override
	{
		switch (static_cast<subscription_errors>(e))
		{
		case subscription_errors::access_denied:
			return "subscription failed due to access denied";
		case subscription_errors::not_found:
			return "subscription not found";
		case subscription_errors::unsubscribed:
			return "user unsubscribed";
		case subscription_errors::unknown:
			return "unsubscribed for unknown reason";
		case subscription_errors::not_authenticated:
			return "not authenticated";
		case subscription_errors::subscription_request_not_sent:
			return "an error occurred before sending the subscription request";
		case subscription_errors::bad_subscription_confirmation:
			return "subscription confirmation did not have last_event_number or last_commit_position";
		case subscription_errors::persistent_subscription_deleted:
			return "persistent subscription was deleted";
		case subscription_errors::subscriber_max_count_reached:
			return "the maximum number of subscriber for this subscription group was reached";
		case subscription_errors::persistent_subscription_already_exists:
			return "could not create the persistent subscription since it already exists";
		case subscription_errors::persistent_subscription_fail:
			return "the operation on the persistent subscription failed";
		case subscription_errors::persistent_subscription_does_not_exist:
			return "the persistent subscription did not exist";
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

inline const operation_category& get_operation_category()
{
	static operation_category instance{};
	return instance;
}

inline const stream_category& get_stream_category()
{
	static stream_category str_category{};
	return str_category;
}

inline const subscription_category& get_subscription_category()
{
	static subscription_category sub_category{};
	return sub_category;
}

} // error

inline boost::system::error_code make_error_code(es::connection_errors ec) noexcept
{
	return boost::system::error_code{ static_cast<int>(ec), es::error::get_connection_category() };
}

inline boost::system::error_code make_error_code(es::communication_errors ec) noexcept
{
	return boost::system::error_code{ static_cast<int>(ec), es::error::get_communication_category() };
}

inline boost::system::error_code make_error_code(es::operation_errors ec) noexcept
{
	return boost::system::error_code{ static_cast<int>(ec), es::error::get_operation_category() };
}

inline boost::system::error_code make_error_code(es::stream_errors ec) noexcept
{
	return boost::system::error_code{ static_cast<int>(ec), es::error::get_stream_category() };
}

inline boost::system::error_code make_error_code(es::subscription_errors ec) noexcept
{
	return boost::system::error_code{ static_cast<int>(ec), es::error::get_subscription_category() };
}

static const boost::system::error_category& connection_category = error::get_connection_category();
static const boost::system::error_category& communication_category = error::get_communication_category();
static const boost::system::error_category& operation_category = error::get_operation_category();
static const boost::system::error_category& stream_category = error::get_stream_category();
static const boost::system::error_category& subscription_category = error::get_subscription_category();

} // es

namespace boost {
namespace system {

template <>
struct is_error_code_enum<es::connection_errors>
{
	static constexpr bool value = true;
};

template <>
struct is_error_code_enum<es::communication_errors>
{
	static constexpr bool value = true;
};

template <>
struct is_error_code_enum<es::operation_errors>
{
	static constexpr bool value = true;
};

template <>
struct is_error_code_enum<es::stream_errors>
{
	static constexpr bool value = true;
};

template <>
struct is_error_code_enum<es::subscription_errors>
{
	static constexpr bool value = true;
};

} // system
} // std

#endif // ES_ERROR_HPP