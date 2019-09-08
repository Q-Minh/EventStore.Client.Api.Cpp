#include <boost/asio/ip/basic_resolver.hpp>
#include <boost/asio/ip/address.hpp>

#include "event_data.hpp"
#include "start_transaction.hpp"
#include "transactional_write.hpp"
#include "commit_transaction.hpp"

#include "connection/basic_tcp_connection.hpp"
#include "tcp/basic_discovery_service.hpp"

int main(int argc, char** argv)
{
	GOOGLE_PROTOBUF_VERSION;

	if (argc != 7)
	{
		ES_ERROR("expected 6 arguments, got {}", argc - 1);
		ES_ERROR("usage: <executable> <ip endpoint> <port> <username> <password> <expected-version> [trace | debug | info | warn | error | critical | off]");
		ES_ERROR("example: ./transaction 127.0.0.1 1113 admin changeit 3 info");
		ES_ERROR("tool will write the following data to event store");
		ES_ERROR("stream={}\n\tevent-type={}\n\tis-json={}\n\tdata={}\n\tmetadata={}", "test-stream", "Test.Type", true, "{ \"test\": \"data\"}", "test metadata");
		return 0;
	}

	// get command arguments
	std::string ep = argv[1];
	int port = std::stoi(argv[2]);
	std::string username = argv[3];
	std::string password = argv[4];
	std::int64_t expected_version = std::stoll(argv[5]);
	std::string_view lvl = argv[6];
	ES_DEFAULT_LOG_LEVEL(lvl);

	boost::asio::io_context ioc;

	boost::asio::ip::tcp::endpoint endpoint;
	endpoint.address(boost::asio::ip::make_address_v4(ep));
	endpoint.port(port);

	// all operation will be authenticated by default
	es::user::user_credentials credentials(username, password);

	auto connection_settings =
		es::connection_settings_builder()
		.with_default_user_credentials(credentials)
		.require_master(false)
		.build();

	// register the discovery service to enable the es tcp connection to discover endpoints to connect to
	// for now, the discovery service doesn't do anything, since we haven't implemented cluster node discovery
	using discovery_service_type = es::tcp::services::basic_discovery_service;
	auto& discovery_service = boost::asio::make_service<discovery_service_type>(ioc, endpoint, boost::asio::ip::tcp::endpoint(), false);

	// parameterize our tcp connection with steady timer, the basic discovery service and our type-erased operation
	using connection_type =
		es::connection::basic_tcp_connection<
		boost::asio::steady_timer,
		discovery_service_type,
		es::operation<>
		>;

	auto tcp_connection = std::make_shared<connection_type>(ioc, connection_settings);

	// wait for connection before sending operations
	bool is_connected{ false };
	bool notification{ false };

	// the async connect will call the given completion handler
	// on error or success, we can inspect the error_code for more info
	tcp_connection->async_connect(
		[tcp_connection = tcp_connection, &is_connected, &notification](boost::system::error_code ec, std::optional<es::connection_result> result)
	{
		notification = true;

		if (!ec || ec == es::connection_errors::authentication_failed)
		{
			ES_INFO("client has been identified with connection-id={}, {}",
				es::to_string(result.value().connection_id()),
				ec.message()
			);
			is_connected = true;
		}
		else
		{
			// es connection failed
			ES_ERROR("client has failed to identify with server, {}", ec.message());
		}
	}
	);

	// wait for server connection response
	while (!notification)
	{
		ioc.poll();
	}

	if (!is_connected)
	{
		ES_ERROR("failed to connect to EventStore");
		return 0;
	}


	// append one stream event in a transaction, the given completion handler will
	// be called once a server response with respect to this 
	// operation has been received or has timed out
	std::string stream = "test-stream";
	std::optional<es::transaction<connection_type>> transaction;
	notification = false;

	// start the transction, nothing has been written yet, but we will receive a
	// transaction object to take ownership from
	es::async_start_transaction(
		tcp_connection,
		stream,
		expected_version,
		[stream = stream, &transaction, &notification](boost::system::error_code ec, std::optional<es::transaction<connection_type>> result)
	{
		notification = true;

		if (!ec)
		{
			transaction = std::move(result);
			return 0;
		}
		else
		{
			ES_ERROR("transaction could not be started, {}", ec.message());
			return 0;
		}
	}
	);

	while (!notification)
	{
		ioc.poll();
	}

	// means transaction was not started
	if (!transaction.has_value()) return 0;

	std::vector<es::event_data> events;
	auto guid = es::guid();
	es::event_data event{ guid, "Test.Type", true, "{ \"test\": \"data\"}", "test metadata" };
	events.push_back(event);

	// write events in the transaction
	notification = false;
	boost::system::error_code err;
	transaction.value().async_write(
		std::move(events),
		[&err, &notification](boost::system::error_code ec)
		{
			notification = true;
			err = ec;
		}
	);
	
	while (!notification)
	{
		ioc.poll();
	}

	if (!err)
	{
		transaction.value().async_commit(
			[&stream](boost::system::error_code ec, std::optional<es::write_result> result)
		{
			if (!ec)
			{
				ES_INFO("transaction committed successfully!");
				ES_INFO("successfully appended to stream {}, next expected version={}", stream, result.value().next_expected_version());
				return;
			}
			else
			{
				ES_ERROR("transaction failed to commit, {}", ec.message());
				return;
			}
		});
	}
	else
	{
		ES_ERROR("transactional write failed");
		return 0;
	}

	ioc.run();

	return 0;
}