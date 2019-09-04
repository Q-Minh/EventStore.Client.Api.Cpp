#include <catch2/catch.hpp>

#include "operations_map.hpp"
#include "guid.hpp"

static int op_func_test = 0;
void op_func1(asio::error_code ec, typename es::operation<>::package_view_type view) { ++op_func_test; }
void op_func2(asio::error_code ec, typename es::operation<>::package_view_type view) { ++op_func_test; }

void op_func_bind1(asio::error_code ec, typename es::operation<>::package_view_type view, int& op_func_bind_test) { ++op_func_bind_test; }
void op_func_bind2(asio::error_code ec, typename es::operation<>::package_view_type view, int& op_func_bind_test) { ++op_func_bind_test; }

TEST_CASE("operations_map has random access by guid to type erased operations", "[operations_map]")
{
	using operation_type = es::operation<>;
	using operations_map_type = es::operations_map<operation_type>;
	using package_view_type = typename operation_type::package_view_type;

	operations_map_type ops_manager;

	SECTION("operations_map can register and call lambdas")
	{
		auto key1 = es::guid();
		int test1 = 0;
		auto token1 = [&test1](asio::error_code, package_view_type) { test1 = 10; };
		operation_type op1(std::move(token1));
		ops_manager.register_op(key1, std::move(op1));
		auto key2 = es::guid();
		int test2 = 0;
		ops_manager.register_op(key2, operation_type([&test2](asio::error_code ec, package_view_type view) -> void { test2 = 6; }));

		ops_manager[key1](asio::error_code{}, package_view_type{ nullptr, 0 });
		ops_manager[key2](asio::error_code{}, package_view_type{ nullptr, 0 });

		REQUIRE(test1 == 10);
		REQUIRE(test2 == 6);
	}
	SECTION("operations_map can register and call functions")
	{
		auto key1 = es::guid();
		operation_type op1(&op_func1);
		ops_manager.register_op(key1, std::move(op1));
		auto key2 = es::guid();
		operation_type op2(&op_func2);
		ops_manager.register_op(key2, std::move(op2));

		ops_manager[key1](asio::error_code{}, package_view_type{ nullptr, 0 });
		REQUIRE(op_func_test == 1);
		ops_manager[key2](asio::error_code{}, package_view_type{ nullptr, 0 });
		REQUIRE(op_func_test == 2);
	}
	SECTION("operations_map can register and call std::bind function objects")
	{
		auto key1 = es::guid();
		int test1 = 0;
		operation_type op1(std::bind(&op_func_bind1, std::placeholders::_1, std::placeholders::_2, std::ref(test1)));
		ops_manager.register_op(key1, std::move(op1));
		auto key2 = es::guid();
		int test2 = 0;
		operation_type op2(std::bind(&op_func_bind2, std::placeholders::_1, std::placeholders::_2, std::ref(test2)));
		ops_manager.register_op(key2, std::move(op2));

		ops_manager[key1](asio::error_code{}, package_view_type{ nullptr, 0 });
		ops_manager[key2](asio::error_code{}, package_view_type{ nullptr, 0 });
		REQUIRE(test1 == 1);
		REQUIRE(test2 == 1);
	}
}