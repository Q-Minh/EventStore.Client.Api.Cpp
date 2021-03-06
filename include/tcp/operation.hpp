#pragma once

#ifndef ES_OPERATION_HPP
#define ES_OPERATION_HPP

#include <memory>
#include <boost/system/error_code.hpp>
#include <type_traits>

#include <boost/asio/error.hpp>

#include "logger.hpp"
#include "guid.hpp"
#include "duration_conversions.hpp"

#include "error/error.hpp"
#include "tcp/tcp_package.hpp"

namespace es {
namespace tcp {
namespace operations {

template <class ConnectionType, class WaitableTimer, class PackageReceivedHandler>
class operation_op
	: public ConnectionType::template friend_base_type<operation_op<ConnectionType, WaitableTimer, PackageReceivedHandler>>
{
public:
	using connection_type = ConnectionType;
	using handler_type = PackageReceivedHandler;
	using waitable_timer_type = WaitableTimer;
	using operations_map_type = typename connection_type::operations_map_type;
	using op_key_type = typename operations_map_type::key_type;

	explicit operation_op(
		std::shared_ptr<connection_type> const& connection,
		handler_type&& handler,
		op_key_type&& key
	) : connection_(connection), 
		handler_(std::move(handler)), 
		deadline_(std::make_shared<waitable_timer_type>(connection->get_io_context())), 
		key_(key)
	{}

	void initiate()
	{
		if (connection_.expired()) return;
		auto conn = connection_.lock();

		operations_map_type& op_map_ = this->get_operations_map(*conn);
		op_map_.register_op(
			key_,
			[handler = std::move(handler_), deadline = deadline_](boost::system::error_code ec, detail::tcp::tcp_package_view view)
		{
			deadline->cancel();
			handler(ec, view);
		}
		);

		ES_TRACE("operation_op::initiate : registered operation {} with timeout={} ms", 
			es::to_string(key_),
			ES_MILLISECONDS(conn->settings().operation_timeout())
		);

		deadline_->expires_after(conn->settings().operation_timeout());
		deadline_->async_wait(std::move(*this));
	}

	// start using boost::system::error_code instead of boost::system::error_code
	void operator()(boost::system::error_code ec)
	{
		if (connection_.expired()) return;
		auto conn = connection_.lock();

		operations_map_type& op_map_ = this->get_operations_map(*conn);

		// operation has timed out if timer completion has been called with success
		if (!ec)
		{
			ES_DEBUG("operation_op::operator() : operation {} timed out", es::to_string(key_));
			// call user's handler with error code
			ec = make_error_code(connection_errors::operation_timeout);
			op_map_[key_](ec, {});
			// remove operation from operations manager unless retries
			op_map_.erase(key_);
		}
		// response has been received in time, do nothing
		else if (ec == boost::asio::error::operation_aborted)
		{
			return;
		}
		// what ?? don't know what happened
		else
		{
			ES_ERROR("operation_op::operator() : operation {} failed, {}", es::to_string(key_), ec.message());
			// perform cleanup
			if (op_map_.contains(key_))
			{
				op_map_[key_](ec, {});
				op_map_.erase(key_);
			}
		}
	}

private:
	std::weak_ptr<ConnectionType> connection_;
	handler_type handler_;
	std::shared_ptr<waitable_timer_type> deadline_;
	op_key_type key_;
};

}
}
}

#endif // ES_OPERATION_HPP