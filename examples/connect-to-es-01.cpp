#include <string>
#include <sstream>

#include <asio/io_context.hpp>
#include <asio/ip/basic_resolver.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/ip/address.hpp>
#include <asio/buffer.hpp>

#include "logger.hpp"
#include "connection_settings.hpp"

#include "connection/basic_tcp_connection.hpp"
#include "error/error.hpp"
#include "tcp/discovery_service.hpp"
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
	ES_DEFAULT_LEVEL(lvl);

	try
	{
		asio::io_context ioc;

		asio::ip::tcp::endpoint endpoint;
		endpoint.address(asio::ip::make_address_v4("127.0.0.1"));
		endpoint.port(1113);
		
		es::user::user_credentials credentials("admin", "changeit");

		auto connection_settings =
			es::connection_settings_builder()
			.with_default_user_credentials(credentials)
			.build();

		using discovery_service_type = es::tcp::services::discovery_service;
		auto& discovery_service = asio::make_service<discovery_service_type>(ioc, endpoint, asio::ip::tcp::endpoint(), false);
		
		using connection_type = 
			es::connection::basic_tcp_connection<
				asio::steady_timer, 
				discovery_service_type, 
				es::operation<>
			>;

		std::vector<std::uint8_t> buffer_storage;

		std::shared_ptr<connection_type> tcp_connection = 
			std::make_shared<connection_type>(
				ioc, 
				connection_settings, 
				asio::dynamic_buffer(buffer_storage)
			);

		tcp_connection->async_connect(
			[self=tcp_connection](asio::error_code ec, es::detail::tcp::tcp_package_view view)
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