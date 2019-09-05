#pragma once

#ifndef ES_HEARTBEAT_HPP
#define ES_HEARTBEAT_HPP

#include <memory>

#include <boost/asio/error.hpp>
#include <boost/asio/basic_waitable_timer.hpp>

#include "logger.hpp"
#include "guid.hpp"
#include "duration_conversions.hpp"

#include "connection/heartbeat_info.hpp"
#include "error/error.hpp"

namespace es {
namespace tcp {
namespace operations {

template <class ConnectionType>
class heartbeat_op
	: public ConnectionType::template friend_base_type<heartbeat_op<ConnectionType>>
{
public:
	using connection_type = ConnectionType;
	using clock_type = typename ConnectionType::clock_type;
	using waitable_timer_type = boost::asio::basic_waitable_timer<clock_type>;

	explicit heartbeat_op(
		std::shared_ptr<connection_type> const& connection
	) : connection_(connection),
		info_()
	{
		deadline_ = std::make_shared<waitable_timer_type>(connection->get_io_context());

		auto conn = connection_.lock();
		info_.set_last_package_no(this->get_package_number(*conn));
		// we actually don't even need elapsed, since we can trust the timer.
		// we leave it for the time being
		info_.set_timestamp(conn->elapsed());
		info_.set_is_interval_stage(true);

		ES_TRACE("heartbeat_op::heartbeat_op : last-package-no={}, timestamp={} ms, interval-stage={}",
			this->get_package_number(*conn),
			ES_MILLISECONDS(conn->elapsed()),
			true
		);
	}

	void initiate()
	{
		if (connection_.expired())
		{
			ES_DEBUG("heartbeat_op::initiate : connection has been deallocated, cancelling heartbeat chain");
			this->operator()(make_error_code(connection_errors::connection_closed));
			return;
		}

		auto conn = connection_.lock();

		if (!conn->socket().is_open())
		{
			ES_DEBUG("heartbeat_op::initiate : socket closed, cancelling heartbeat chain");
			this->operator()(make_error_code(connection_errors::connection_closed));
			return;
		}
		
		if (info_.last_package_no() != this->get_package_number(*conn))
		{
			info_.set_last_package_no(this->get_package_number(*conn));
			info_.set_is_interval_stage(true);
			// we actually don't even need elapsed, since we can trust the timer.
			// we leave it for the time being
			info_.set_timestamp(conn->elapsed());

			ES_TRACE("heartbeat_op::initiate : heartbeat going into interval stage after updating last package no");
			ES_TRACE("heartbeat_op::initiate : last-package-no={}, timestamp={} ms, interval-stage={}, timeout={} ms",
				this->get_package_number(*conn),
				ES_MILLISECONDS(conn->elapsed()),
				true,
				ES_MILLISECONDS(conn->settings().heartbeat_interval())
			);
			deadline_->expires_after(conn->settings().heartbeat_interval());
			deadline_->async_wait(std::move(*this));

			return;
		}
		
		if (info_.is_interval_stage())
		{
			using tcp_package = detail::tcp::tcp_package<>;
			using tcp_command = detail::tcp::tcp_command;
			using tcp_flags = detail::tcp::tcp_flags;

			conn->async_send(tcp_package(
				tcp_command::heartbeat_request_command,
				tcp_flags::none,
				es::guid()
			));

			info_.set_is_interval_stage(false);
			// we actually don't even need elapsed, since we can trust the timer.
			// we leave it for the time being
			info_.set_timestamp(conn->elapsed());

			ES_TRACE("heartbeat_op::initiate : heartbeat sending heartbeat request");
			ES_TRACE("heartbeat_op::initiate : last-package-no={}, timestamp={} ms, interval-stage={}, timeout={} ms",
				this->get_package_number(*conn),
				ES_MILLISECONDS(conn->elapsed()),
				false,
				ES_MILLISECONDS(conn->settings().heartbeat_timeout())
			);
			deadline_->expires_after(conn->settings().heartbeat_timeout());
			deadline_->async_wait(std::move(*this));
			return;
		}
		else
		{
			// raise heartbeat timeout error
			this->operator()(make_error_code(connection_errors::heartbeat_timeout));
		}
	}

	void operator()(boost::system::error_code ec)
	{
		if (!ec)
		{
			initiate();
		}
		else if (ec == boost::asio::error::operation_aborted)
		{
			// heartbeat was cancelled for some reason
			ES_WARN("heartbeats aborted, {}", ec.message());
			return;
		}
		else if (ec == connection_errors::connection_closed)
		{
			ES_WARN("connection has been closed, heartbeats will terminate, {}", ec.message());
			return;
		}
		else
		{
			ES_ERROR("heartbeat_op::operator() : error={}, closing connection", ec.message());
			if (connection_.expired()) return;
			auto conn = connection_.lock();
			conn->close();
		}
	}

private:
	std::weak_ptr<connection_type> connection_;
	detail::connection::heartbeat_info<clock_type> info_;
	std::shared_ptr<waitable_timer_type> deadline_;
};

} // operations
} // tcp
} // es

#endif // ES_HEARTBEAT_HPP