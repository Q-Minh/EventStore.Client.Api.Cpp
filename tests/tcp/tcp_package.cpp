#include <catch2/catch.hpp>

#include "guid.hpp"
#include "version.hpp"

#include "message/messages.pb.h"
#include "tcp/tcp_package.hpp"

TEST_CASE("tcp_package is consistent with tcp_package_view", "[tcp_package]") 
{
	using es::detail::tcp::tcp_package;
	using es::detail::tcp::tcp_package_view;
	using es::detail::tcp::tcp_command;
	using es::detail::tcp::tcp_flags;

	SECTION("tcp_package_view accessors with unauthenticated tcp_package return correct values")
	{
		es::message::IdentifyClient identify_client_message;
		auto connection_name_uuid = es::guid();
		auto connection_name = std::string("ES-") + es::to_string(connection_name_uuid);
		identify_client_message.set_connection_name(connection_name);
		identify_client_message.set_version(ES_CLIENT_VERSION);
		auto message = identify_client_message.SerializeAsString();

		auto guid = es::guid();

		tcp_package pkg(
			tcp_command::identify_client,
			tcp_flags::none,
			guid,
			(std::byte*)message.data(),
			message.size()
		);

		REQUIRE(pkg.is_valid());
		REQUIRE(pkg.is_header_valid());
		REQUIRE(pkg.size() == 18 + message.size() + 4);

		auto pkg_view = static_cast<tcp_package_view>(pkg);

		REQUIRE(pkg_view.is_authenticated() == false);
		REQUIRE(pkg_view.is_valid());
		REQUIRE(pkg_view.command() == tcp_command::identify_client);
		bool result = (char)pkg_view.flags() == 0x00;
		REQUIRE(result);
		result = ((char)pkg_view.flags() & (char)tcp_flags::authenticated) == 0x01 ? true : false;
		REQUIRE(!result);
		auto view_corr_id = pkg_view.correlation_id();
		auto pkg_corr_id = std::string_view((char*)guid.data, guid.size());
		REQUIRE(view_corr_id == pkg_corr_id);
		REQUIRE(pkg_view.message_size() == message.size());
		REQUIRE(pkg_view.size() == 18 + message.size() + 4);
		REQUIRE(pkg_view.message_size() == pkg_view.size() - pkg_view.message_offset());
		REQUIRE(std::string_view(pkg.data(), pkg.size()) == std::string_view(pkg_view.data(), pkg_view.size()));
	}
	SECTION("tcp_package_view accessors with authenticated tcp_package return correct values")
	{
		auto message = std::string("");

		auto guid = es::guid();

		std::string login = "admin";
		std::string password = "changeit";

		tcp_package pkg(
			tcp_command::authenticate,
			tcp_flags::authenticated,
			guid,
			login,
			password,
			(std::byte*)message.data(),
			message.size()
		);

		REQUIRE(pkg.is_valid());
		REQUIRE(pkg.is_header_valid());
		REQUIRE(pkg.size() == 4 + 18 + 1 + login.size() + 1 + password.size() + message.size());

		auto pkg_view = static_cast<tcp_package_view>(pkg);

		REQUIRE(pkg_view.is_authenticated());
		REQUIRE(pkg_view.is_valid());
		REQUIRE(pkg_view.command() == tcp_command::authenticate);
		bool result = (char)pkg_view.flags() == 0x00;
		REQUIRE(!result);
		result = ((char)pkg_view.flags() & (char)tcp_flags::authenticated) == 0x01 ? true : false;
		REQUIRE(result);
		auto view_corr_id = pkg_view.correlation_id();
		auto pkg_corr_id = std::string_view((char*)guid.data, guid.size());
		REQUIRE(view_corr_id == pkg_corr_id);
		REQUIRE(pkg_view.message_size() == message.size());
		REQUIRE(pkg_view.login() == "admin");
		REQUIRE(pkg_view.password() == "changeit");
		REQUIRE(pkg_view.size() == 4 + 18 + message.size() + 1 + login.size() + 1 + password.size());
		REQUIRE(pkg_view.message_offset() == pkg_view.size() - pkg_view.message_size());
		auto pkg_data = std::string_view(pkg.data(), pkg.size());
		auto pkg_view_data = std::string_view(pkg_view.data(), pkg_view.size());
		REQUIRE(pkg_data == pkg_view_data);
	}
}