#pragma once

#ifndef ES_HANDLE_OPERATION_ERROR_HPP
#define ES_HANDLE_OPERATION_ERROR_HPP

#include "message/messages.pb.h"

#include "logger.hpp"
#include "node_endpoints.hpp"
#include "error/error.hpp"
#include "tcp/tcp_package.hpp"

namespace es {
namespace detail {

template <class DiscoveryService>
void handle_operation_error(boost::system::error_code& ec, detail::tcp::tcp_package_view view, DiscoveryService& discovery_service)
{
	switch (view.command())
	{
	case detail::tcp::tcp_command::not_authenticated:
	{
		ec = make_error_code(operation_errors::not_authenticated);
		break;
	}
	case detail::tcp::tcp_command::not_handled:
	{
		ec = make_error_code(operation_errors::not_handled);

		message::NotHandled response;
		response.ParseFromArray(view.data() + view.message_offset(), view.message_size());

		switch (response.reason())
		{
		case message::NotHandled_NotHandledReason_NotReady:
			ec = make_error_code(operation_errors::not_ready);
			break;
		case message::NotHandled_NotHandledReason_TooBusy:
			ec = make_error_code(operation_errors::too_busy);
			break;
		case message::NotHandled_NotHandledReason_NotMaster:
		{
			ec = make_error_code(operation_errors::not_master);
			message::NotHandled::MasterInfo master;
			master.ParseFromString(response.additional_info());

			boost::asio::ip::tcp::endpoint ep{ 
				boost::asio::ip::make_address_v4(master.external_tcp_address()), 
				static_cast<unsigned short>(master.external_tcp_port()) 
			};
			boost::asio::ip::tcp::endpoint secure_ep{ 
				boost::asio::ip::make_address_v4(master.external_secure_tcp_address()), 
				static_cast<unsigned short>(master.external_secure_tcp_port()) 
			};

			node_endpoints eps{ ep, secure_ep };
			discovery_service.set_master(eps);
			
			return;
		}
			break;
		default:
			break;
		}
		break;
	}
	case detail::tcp::tcp_command::bad_request:
	{
		ec = make_error_code(communication_errors::bad_request);
		break;
	}
	default:
		ES_TRACE("unexpected command received : {}", detail::tcp::to_string(view.command()));
		ec = make_error_code(communication_errors::unexpected_response);
		break;
	}

	return;
}

} // detail
} // es

#endif // ES_HANDLE_OPERATION_ERROR_HPP 