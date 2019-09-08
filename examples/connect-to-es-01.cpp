#include <string>
#include <mutex>
#include <condition_variable>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/basic_resolver.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/buffer.hpp>

#include "logger.hpp"
#include "connection_settings.hpp"
#include "event_data.hpp"
#include "append_to_stream.hpp"
#include "read_stream_event.hpp"

#include "connection/basic_tcp_connection.hpp"
#include "error/error.hpp"
#include "tcp/basic_discovery_service.hpp"
#include "user/user_credentials.hpp"

int main(int argc, char** argv)
{
	GOOGLE_PROTOBUF_VERSION;

	if (argc != 6)
	{
		ES_ERROR("expected 5 arguments, got {}", argc - 1);
		ES_ERROR("usage: <executable> <ip endpoint> <port> <username> <password> [trace | debug | info | warn | error | critical | off]");
		return 0;
	}

	std::string ep = argv[1];
	int port = std::stoi(argv[2]);
	std::string username = argv[3];
	std::string password = argv[4];
	std::string_view lvl = argv[5];
	ES_DEFAULT_LOG_LEVEL(lvl);

	try
	{
		boost::asio::io_context ioc;

		boost::asio::ip::tcp::endpoint endpoint;
		endpoint.address(boost::asio::ip::make_address_v4(ep));
		endpoint.port(port);
		
		es::user::user_credentials credentials(username, password);

		auto connection_settings =
			es::connection_settings_builder()
			.with_default_user_credentials(credentials)
			.build();

		using discovery_service_type = es::tcp::services::basic_discovery_service;
		auto& discovery_service = boost::asio::make_service<discovery_service_type>(ioc, endpoint, boost::asio::ip::tcp::endpoint(), false);
		
		auto tcp_connection = std::make_shared<connection_type>(ioc, connection_settings);


		tcp_connection->async_connect(
			[tcp_connection=tcp_connection](boost::system::error_code ec, es::detail::tcp::tcp_package_view view)
		{
			if (!ec || ec == es::connection_errors::authentication_failed)
			{
				// on success, command should be client identified
				if (view.command() != es::detail::tcp::tcp_command::client_identified) return;

				ES_INFO("client has been identified, {}", ec.message());
			}
			else
			{
				// es connection failed
				ES_ERROR("client has failed to identify with server, {}", ec.message());
			}
		}
		);

		ioc.run();
	}
	catch (std::exception const& e)
	{
		ES_ERROR(e.what());
	}

    return 0;
}