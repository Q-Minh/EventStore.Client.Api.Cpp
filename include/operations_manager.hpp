#pragma once

#ifndef ES_OPERATIONS_MANAGER_HPP
#define ES_OPERATIONS_MANAGER_HPP

#include <memory>
#include <unordered_map>
#include <utility>

#include <asio/error_code.hpp>

#include "guid.hpp"
#include "tcp/tcp_package.hpp"

namespace es {

// type erased operation
template <class Allocator = std::allocator<char>>
class operation
{
public:
	using package_view_type = detail::tcp::tcp_package_view;

	template <typename CompletionHandler>
	operation(CompletionHandler&& token, Allocator& alloc = Allocator())
		: op_(nullptr), alloc_(alloc)
	{
		static_assert(
			std::is_invocable_r_v<void, CompletionHandler, asio::error_code, package_view_type>,
			"CompletionToken must have signature void(asio::error_code, es::internal::tcp::tcp_package_view)"
		);

		op_ = (op_base*)alloc_.allocate(sizeof(op<CompletionHandler>));
		new (op_) op<CompletionHandler>(std::move(token));
	}

	operation(operation<Allocator> const& other)
	{
		using std::swap;
		swap(op_, other.op_);
		swap(alloc_, other.alloc_);
	}

	operation(operation<Allocator>&& other) noexcept
	{
		op_ = std::move(other.op_);
		other.op_ = nullptr;
		alloc_ = std::move(other.alloc_);
	}

	operation<Allocator>& operator=(operation<Allocator>&& other) noexcept
	{
		using std::swap;
		if (op_ != nullptr)
		{
			alloc_.deallocate(op_, op_->size());
		}

		op_ = std::move(other.op_);
		other.op_ = nullptr;
		swap(alloc_, other.alloc_);
	}

	void operator()(asio::error_code ec, package_view_type package)
	{
		op_->handle(ec, package);
	}

	~operation() 
	{ 
		if (op_ != nullptr)
			alloc_.deallocate(reinterpret_cast<char*>(op_), op_->size()); 
	}

private:
	struct op_base
	{
		virtual void handle(asio::error_code ec, package_view_type package) = 0;
		virtual std::size_t size() const = 0;
		virtual ~op_base() {}
	};

	template <class CompletionHandler>
	struct op : public op_base {
		explicit op(CompletionHandler&& handler) : handler_(std::move(handler)) {}
		virtual void handle(asio::error_code ec, package_view_type package) override { handler_(ec, package); }
		virtual std::size_t size() const override { return sizeof(op<CompletionHandler>); }
		CompletionHandler handler_;
	};

	op_base* op_;
	Allocator alloc_;
};

template <class Operation, class Allocator = std::allocator<std::pair<const es::guid_type, Operation>>>
class operations_manager
{
public:
	using operation_type = Operation;

	explicit operations_manager(
		Allocator const& alloc = Allocator(),
		std::size_t initial_capacity = 0
	) : operations_(initial_capacity, alloc)
	{}

	void register_op(es::guid_type const& guid, Operation&& op)
	{
		operations_.emplace(std::make_pair(guid, std::move(op)));
	}

	bool contains(es::guid_type const& key) { return operations_.find(key) != operations_.cend(); }
	Operation& operator[](es::guid_type const& key) { return operations_.at(key); }
	Operation const& operator[](es::guid_type const& key) const { return operations_.at(key); }
	void erase(es::guid_type const& key) { operations_.erase(key); }

private:
	using map_type = std::unordered_map<
		es::guid_type, // key type
		Operation, // operation type
		std::hash<es::guid_type>, // hasher type
		std::equal_to<es::guid_type>, // equality type
		Allocator // allocator type for key/value pairs
	>;

	map_type operations_;
};

} // es

#endif // ES_OPERATIONS_MANAGER_HPP