#pragma once

#ifndef ES_IDENTIFY_HPP
#define ES_IDENTIFY_HPP

#include <memory>
#include <type_traits>

#include <asio/error_code.hpp>
#include <asio/basic_waitable_timer.hpp>

#include "logger.hpp"
#include "guid.hpp"
#include "version.hpp"

#include "connection/identity_info.hpp"
#include "error/error.hpp"
#include "message/messages.pb.h"

namespace es {
namespace tcp {
namespace operations {

template <class ConnectionType, class PackageReceivedHandler>
class identify_op
	: public ConnectionType::template friend_base_type<identify_op<ConnectionType, PackageReceivedHandler>>
{
public:
	using connection_type = ConnectionType;
	using clock_type = typename connection_type::clock_type;
	using waitable_timer_type = asio::basic_waitable_timer<clock_type>;

	static_assert(
		std::is_invocable_v<PackageReceivedHandler, asio::error_code, detail::tcp::tcp_package_view>,
		"PackageReceivedHandler must have signature void(asio::error_code, es::internal::tcp::tcp_package_view"
	);

	explicit identify_op(
		std::shared_ptr<connection_type> const& connection,
		PackageReceivedHandler&& handler,
		bool authenticated = false
	) : connection_(connection), 
		info_(), 
		deadline_(), 
		reconnections_(0), 
		handler_(std::move(handler)), 
		authenticated_(authenticated)
	{
		deadline_ = std::make_shared<waitable_timer_type>(connection->get_io_context());
		auto conn = connection_.lock();
		info_.set_correlation_id(es::guid());
		info_.set_timestamp(conn->elapsed()); // not really needed, as we have a timer anyways...
	}

	void initiate()
	{
		using tcp_package = detail::tcp::tcp_package<>;
		using tcp_package_view = detail::tcp::tcp_package_view;
		using tcp_command = detail::tcp::tcp_command;
		using tcp_flags = detail::tcp::tcp_flags;

		// call user token with error message
		if (connection_.expired()) return;

		auto conn = connection_.lock();

		message::IdentifyClient message;
		message.set_version(ES_CLIENT_VERSION);
		message.set_connection_name(conn->connection_name());

		auto serialized = message.SerializeAsString();

		auto do_identify = [this, &conn](tcp_package&& identify_package)
		{
			ES_TRACE("identify_op::initiate : tcp-package-size={}, cmd={}, authenticated={}, corr-id={}",
				identify_package.size(),
				"identify_client",
				false,
				es::to_string(info_.correlation_id())
			);

			conn->async_send(std::move(identify_package));
			auto& op_manager = this->get_operations_manager(*conn);
			using operation_type = typename std::decay_t<decltype(op_manager)>::operation_type;

			op_manager.register_op(
				info_.correlation_id(),
				operation_type(
					[handler = std::move(handler_), deadline = deadline_, authenticated=authenticated_](asio::error_code ec, tcp_package_view view)
			{
				deadline->cancel();
				if (!authenticated && !ec) ec = make_error_code(connection_errors::authentication_failed);
				handler(ec, view);
			})
			);
			ES_TRACE("identify_op::initiate : package send initiated, handler registered to operations manager");
		};

		if (authenticated_)
		{
			tcp_package package(
				tcp_command::identify_client,
				tcp_flags::authenticated,
				info_.correlation_id(),
				conn->settings().default_user_credentials().username(),
				conn->settings().default_user_credentials().password(),
				(std::byte*)serialized.data(),
				serialized.size()
			);

			do_identify(std::move(package));
		}
		else
		{
			tcp_package package(
				tcp_command::identify_client,
				tcp_flags::none,
				info_.correlation_id(),
				(std::byte*)serialized.data(),
				serialized.size()
			);

			do_identify(std::move(package));
		}

		ES_TRACE("identify_op::initiate : identification operation timeout={} ms, reconnection-no={}",
			ES_MILLISECONDS(conn->settings().operation_timeout()),
			reconnections_
		);
		deadline_->expires_after(conn->settings().operation_timeout());
		deadline_->async_wait(std::move(*this));
	}

	// handle the timeout
	void operator()(asio::error_code ec)
	{
		// should set error code
		if (connection_.expired()) return;
		auto conn = connection_.lock();
		
		auto& op_manager = this->get_operations_manager(*conn);
		using operation_type = typename std::decay_t<decltype(op_manager)>::operation_type;

		if (!ec)
		{
			// check retries
			auto max_reconnections = conn->settings().max_reconnections();
			if (max_reconnections > 0 && reconnections_ < max_reconnections)
			{
				++reconnections_;
				initiate();
				return;
			}

			// else notify user of identify operation timing out
			ec = make_error_code(connection_errors::operation_timeout);
			op_manager[info_.correlation_id()](ec, {});
			return;
		}
		
		// if error code is operation aborted, it means the server received the response before timing out.
		// else, there was an error
		if (ec != asio::error::operation_aborted)
		{
			op_manager[info_.correlation_id()](ec, {});
			return;
		}
	}

private:
	std::weak_ptr<connection_type> connection_;
	detail::connection::identity_info<clock_type> info_;
	
	std::shared_ptr<waitable_timer_type> deadline_;
	std::uint32_t reconnections_;
	PackageReceivedHandler handler_;
	bool authenticated_;
};

} // operations
} // tcp
} // es

#endif // ES_IDENTIFY_HPP