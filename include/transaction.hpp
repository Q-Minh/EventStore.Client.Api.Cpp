#pragma once

#ifndef ES_TRANSACTION_HPP
#define ES_TRANSACTION_HPP

#include <boost/asio/post.hpp>

#include "commit_transaction.hpp"
#include "transactional_write.hpp"
#include "user/user_credentials.hpp"

namespace es {

template <class ConnectionType>
class transaction
{
public:
	explicit transaction(
		std::int64_t transaction_id,
		std::shared_ptr<ConnectionType> const& connection
	) : transaction_id_(transaction_id),
		is_rolled_back_(false),
		is_committed_(false),
		connection_(connection)
	{}

	transaction(transaction<ConnectionType>&& other) = default;
	transaction<ConnectionType>& operator=(transaction<ConnectionType>&& other) = default;

	template <class WriteResultHandler>
	void async_commit(WriteResultHandler&& handler)
	{
		boost::system::error_code ec;
		if (is_rolled_back_) ec = make_error_code(stream_errors::transaction_rolled_back);
		if (is_committed_) ec = make_error_code(stream_errors::transaction_committed);
		
		if (ec)
		{
			// call the handler with the error
			boost::asio::post(
				connection_->get_io_context(),
				[ec = ec, handler = std::move(handler)]() { handler(ec, {}); }
			);

			return;
		}
		else
		{
			is_committed_ = true;

			es::async_commit_transaction(
				connection_,
				transaction_id_,
				std::forward<WriteResultHandler>(handler)
			);

			return;
		}
	}

	template <class NewEventsSequence, class TransactionWriteHandler>
	void async_write(NewEventsSequence&& events, TransactionWriteHandler&& handler)
	{
		boost::system::error_code ec;
		if (is_rolled_back_) ec = make_error_code(stream_errors::transaction_rolled_back);
		if (is_committed_) ec = make_error_code(stream_errors::transaction_committed);

		if (ec)
		{
			// call the handler with the error
			boost::asio::post(
				connection_->get_io_context(),
				[ec = ec, handler = std::move(handler)]() { handler(ec); }
			);

			return;
		}
		else
		{
			es::async_transactional_write(
				connection_,
				transaction_id_,
				std::move(events),
				std::move(handler)
			);
			return;
		}
	}

	// returns false if rollback failed (transaction is already committed)
	// returns true if rollback successful
	boost::system::error_code rollback()
	{
		boost::system::error_code ec;
		if (is_committed_)
		{
			ec = make_error_code(stream_errors::transaction_committed);
		}
		else
		{
			is_rolled_back_ = true;
		}

		return ec;
	}

private:
	std::int64_t transaction_id_;
	bool is_rolled_back_;
	bool is_committed_;
	std::shared_ptr<ConnectionType> connection_;
};

}

#endif // ES_TRANSACTION_HPP