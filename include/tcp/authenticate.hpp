#pragma once

#ifndef ES_AUTHENTICATE_HPP
#define ES_AUTHENTICATE_HPP

#include <memory>
#include <type_traits>

#include <boost/asio/error.hpp>
#include <boost/asio/basic_waitable_timer.hpp>

#include "logger.hpp"
#include "guid.hpp"
#include "error/error.hpp"

#include "connection/authentication_info.hpp"

namespace es {
namespace tcp {
namespace operations {

template <class ConnectionType, class PackageReceivedHandler>
class authenticate_op
	: public ConnectionType::template friend_base_type<authenticate_op<ConnectionType, PackageReceivedHandler>>
{
public:
	using connection_type = ConnectionType;
	using clock_type = typename connection_type::clock_type;
	using waitable_timer_type = boost::asio::basic_waitable_timer<clock_type>;
	using handler_type = PackageReceivedHandler;

	explicit authenticate_op(
		std::shared_ptr<connection_type> const& connection,
		handler_type&& handler
	) : connection_(connection), info_(), deadline_(), handler_(std::move(handler))
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
		tcp_package auth_package(
			tcp_command::authenticate,
			tcp_flags::authenticated,
			info_.correlation_id(),
			conn->settings().default_user_credentials().username(),
			conn->settings().default_user_credentials().password()
		);
		auto test = std::string_view(auth_package.data(), auth_package.size());
		ES_TRACE("authenticate_op::initiate : tcp-package-size={}, cmd={}, authenticated={}, corr-id={}",
			auth_package.size(),
			"authenticate",
			true,
			es::to_string(info_.correlation_id())
		);

		conn->async_send(std::move(auth_package));
		auto& op_map = this->get_operations_map(*conn);
		using operation_type = typename std::decay_t<decltype(op_map)>::operation_type;

		op_map.register_op(
			info_.correlation_id(),
			operation_type(
				[handler = std::move(handler_), deadline = deadline_](boost::system::error_code ec, tcp_package_view view) mutable
				{
					deadline->cancel();
					handler(ec, view);
				})
		);
		ES_TRACE("authenticate_op::initiate : package send initiated, handled registered to operations manager");

		ES_TRACE("authenticate_op::initiate : authentication operation timeout={} ms", ES_MILLISECONDS(conn->settings().operation_timeout()));
		deadline_->expires_after(conn->settings().operation_timeout());
		deadline_->async_wait(std::move(*this));
	}

	void operator()(boost::system::error_code ec)
	{
		// should set error code
		if (connection_.expired()) return;
		auto conn = connection_.lock();

		auto& op_map = this->get_operations_map(*conn);
		using operation_type = typename std::decay_t<decltype(op_map)>::operation_type;

		if (!ec)
		{
			ES_ERROR("authentication request timed out");
			// authentication timed out
			ec = make_error_code(connection_errors::authentication_timeout);
			op_map[info_.correlation_id()](ec, {});
			return;
		}

		// if operation was aborted, it means response was received in time
		if (ec != boost::asio::error::operation_aborted)
		{
			op_map[info_.correlation_id()](ec, {});
			return;
		}
	}

private:
	std::weak_ptr<connection_type> connection_;
	detail::connection::authentication_info<clock_type> info_;
	std::shared_ptr<waitable_timer_type> deadline_;
	handler_type handler_;
};

}
}
}

#endif // ES_AUTHENTICATE_HPP