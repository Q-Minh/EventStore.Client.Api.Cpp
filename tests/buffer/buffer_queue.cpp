#include <catch2/catch.hpp>

#include <deque>

#include "buffer/buffer_queue.hpp"

static int default_ctor_count = 0;
static int copy_ctor_count = 0;
static int user_ctor_count = 0;

TEST_CASE("buffer_queue has consistent accessors/modifiers", "[buffer][buffer_queue]")
{
	struct dummy
	{
		dummy()
		{
			++default_ctor_count;
		}

		dummy(int dat1, char dat2) : data1(dat1), data2(dat2) 
		{
			++user_ctor_count;
		}

		dummy(dummy const& other)
		{
			++copy_ctor_count;
			data1 = other.data1;
			data2 = other.data2;
		}

		int data1;
		char data2;
	};

	es::buffer::buffer_queue<dummy, std::deque, 3, std::allocator<std::array<char, sizeof(dummy) * 3>>> queue_;

	default_ctor_count = 0;
	copy_ctor_count = 0;
	user_ctor_count = 0;

	SECTION("size, empty, push_back, front and pop_front are consistent when used in linear fashion (build up tear down)")
	{
		REQUIRE(queue_.empty());
		REQUIRE(queue_.size() == 0);
		dummy d1{ 3, 'c' };
		//REQUIRE(user_ctor_count == 1);
		queue_.push_back(d1);
		queue_.push_back(dummy{ 4, 'd' });
		queue_.push_back(dummy{ 5, 'e' });
		queue_.push_back(dummy{ 6, 'f' });
		queue_.push_back(dummy{ 7, 'g' });
		REQUIRE(queue_.front().data1 == 3);
		REQUIRE(queue_.front().data2 == 'c');
		REQUIRE(queue_.size() == 5);
		REQUIRE(!queue_.empty());
		//REQUIRE(user_ctor_count == 1);

		queue_.pop_front();
		REQUIRE(queue_.front().data1 == 4);
		REQUIRE(queue_.front().data2 == 'd');
		REQUIRE(queue_.size() == 4);
		REQUIRE(!queue_.empty());

		queue_.pop_front();
		REQUIRE(queue_.front().data1 == 5);
		REQUIRE(queue_.front().data2 == 'e');
		REQUIRE(queue_.size() == 3);
		REQUIRE(!queue_.empty());

		queue_.pop_front();
		REQUIRE(queue_.front().data1 == 6);
		REQUIRE(queue_.front().data2 == 'f');
		REQUIRE(queue_.size() == 2);
		REQUIRE(!queue_.empty());

		queue_.pop_front();
		REQUIRE(queue_.front().data1 == 7);
		REQUIRE(queue_.front().data2 == 'g');
		REQUIRE(queue_.size() == 1);
		REQUIRE(!queue_.empty());

		queue_.pop_front();
		REQUIRE(queue_.size() == 0);
		REQUIRE(queue_.empty());
		REQUIRE(copy_ctor_count == 5);
	}
	SECTION("size, empty, push_back, front and pop_front are consistent when used in unordered fashion (build, build, tear, build build build, tear)")
	{
		REQUIRE(queue_.empty());
		REQUIRE(queue_.size() == 0);
		queue_.push_back(dummy{ 1, 'a' });
		queue_.push_back(dummy{ 2, 'b' });
		queue_.push_back(dummy{ 3, 'c' });
		REQUIRE(queue_.size() == 3);
		REQUIRE(!queue_.empty());
		REQUIRE(queue_.front().data1 == 1);
		REQUIRE(queue_.front().data2 == 'a');
		queue_.pop_front();
		REQUIRE(queue_.size() == 2);
		REQUIRE(!queue_.empty());
		REQUIRE(queue_.front().data1 == 2);
		REQUIRE(queue_.front().data2 == 'b');
		queue_.push_back(dummy{ 4, 'd' });
		queue_.push_back(dummy{ 5, 'e' });
		queue_.push_back(dummy{ 6, 'f' });
		queue_.push_back(dummy{ 7, 'g' });
		REQUIRE(queue_.size() == 6);
		REQUIRE(!queue_.empty());
		REQUIRE(queue_.front().data1 == 2);
		REQUIRE(queue_.front().data2 == 'b');

		queue_.pop_front();
		REQUIRE(queue_.size() == 5);
		REQUIRE(!queue_.empty());
		REQUIRE(queue_.front().data1 == 3);
		REQUIRE(queue_.front().data2 == 'c');

		queue_.pop_front();
		REQUIRE(queue_.size() == 4);
		REQUIRE(!queue_.empty());
		REQUIRE(queue_.front().data1 == 4);
		REQUIRE(queue_.front().data2 == 'd');

		queue_.pop_front();
		REQUIRE(queue_.size() == 3);
		REQUIRE(!queue_.empty());
		REQUIRE(queue_.front().data1 == 5);
		REQUIRE(queue_.front().data2 == 'e');

		queue_.pop_front();
		REQUIRE(queue_.size() == 2);
		REQUIRE(!queue_.empty());
		REQUIRE(queue_.front().data1 == 6);
		REQUIRE(queue_.front().data2 == 'f');

		queue_.pop_front();
		REQUIRE(queue_.size() == 1);
		REQUIRE(!queue_.empty());
		REQUIRE(queue_.front().data1 == 7);
		REQUIRE(queue_.front().data2 == 'g');

		queue_.pop_front();
		REQUIRE(queue_.size() == 0);
		REQUIRE(queue_.empty());
		REQUIRE(copy_ctor_count == 7);
	}
	SECTION("size, empty, push_back, front and pop_front are consistent when used in unordered fashion with r-value (build, build, tear, build build build, tear)")
	{
		REQUIRE(queue_.empty());
		REQUIRE(queue_.size() == 0);
		queue_.push_back(std::move(dummy{ 1, 'a' }));
		queue_.push_back(std::move(dummy{ 2, 'b' }));
		queue_.push_back(std::move(dummy{ 3, 'c' }));
		REQUIRE(queue_.size() == 3);
		REQUIRE(!queue_.empty());
		REQUIRE(queue_.front().data1 == 1);
		REQUIRE(queue_.front().data2 == 'a');
		queue_.pop_front();
		REQUIRE(queue_.size() == 2);
		REQUIRE(!queue_.empty());
		REQUIRE(queue_.front().data1 == 2);
		REQUIRE(queue_.front().data2 == 'b');
		queue_.push_back(std::move(dummy{ 4, 'd' }));
		queue_.push_back(std::move(dummy{ 5, 'e' }));
		queue_.push_back(std::move(dummy{ 6, 'f' }));
		queue_.push_back(std::move(dummy{ 7, 'g' }));
		REQUIRE(queue_.size() == 6);
		REQUIRE(!queue_.empty());
		REQUIRE(queue_.front().data1 == 2);
		REQUIRE(queue_.front().data2 == 'b');

		queue_.pop_front();
		REQUIRE(queue_.size() == 5);
		REQUIRE(!queue_.empty());
		REQUIRE(queue_.front().data1 == 3);
		REQUIRE(queue_.front().data2 == 'c');

		queue_.pop_front();
		REQUIRE(queue_.size() == 4);
		REQUIRE(!queue_.empty());
		REQUIRE(queue_.front().data1 == 4);
		REQUIRE(queue_.front().data2 == 'd');

		queue_.pop_front();
		REQUIRE(queue_.size() == 3);
		REQUIRE(!queue_.empty());
		REQUIRE(queue_.front().data1 == 5);
		REQUIRE(queue_.front().data2 == 'e');

		queue_.pop_front();
		REQUIRE(queue_.size() == 2);
		REQUIRE(!queue_.empty());
		REQUIRE(queue_.front().data1 == 6);
		REQUIRE(queue_.front().data2 == 'f');

		queue_.pop_front();
		REQUIRE(queue_.size() == 1);
		REQUIRE(!queue_.empty());
		REQUIRE(queue_.front().data1 == 7);
		REQUIRE(queue_.front().data2 == 'g');

		queue_.pop_front();
		REQUIRE(queue_.size() == 0);
		REQUIRE(queue_.empty());
		REQUIRE(copy_ctor_count == 7);
	}
	SECTION("size, empty, push_back, front and pop_front are consistent when used in unordered fashion with emplace_back (build, build, tear, build build build, tear)")
	{
		REQUIRE(queue_.empty());
		REQUIRE(queue_.size() == 0);
		queue_.emplace_back<int, char>(1, 'a');
		queue_.emplace_back(2, 'b');
		queue_.emplace_back(3, 'c');
		REQUIRE(queue_.size() == 3);
		REQUIRE(!queue_.empty());
		REQUIRE(queue_.front().data1 == 1);
		REQUIRE(queue_.front().data2 == 'a');
		queue_.pop_front();
		REQUIRE(queue_.size() == 2);
		REQUIRE(!queue_.empty());
		REQUIRE(queue_.front().data1 == 2);
		REQUIRE(queue_.front().data2 == 'b');
		queue_.emplace_back(4, 'd');
		queue_.emplace_back(5, 'e');
		queue_.emplace_back(6, 'f');
		queue_.emplace_back(7, 'g');
		REQUIRE(queue_.size() == 6);
		REQUIRE(!queue_.empty());
		REQUIRE(queue_.front().data1 == 2);
		REQUIRE(queue_.front().data2 == 'b');

		queue_.pop_front();
		REQUIRE(queue_.size() == 5);
		REQUIRE(!queue_.empty());
		REQUIRE(queue_.front().data1 == 3);
		REQUIRE(queue_.front().data2 == 'c');

		queue_.pop_front();
		REQUIRE(queue_.size() == 4);
		REQUIRE(!queue_.empty());
		REQUIRE(queue_.front().data1 == 4);
		REQUIRE(queue_.front().data2 == 'd');

		queue_.pop_front();
		REQUIRE(queue_.size() == 3);
		REQUIRE(!queue_.empty());
		REQUIRE(queue_.front().data1 == 5);
		REQUIRE(queue_.front().data2 == 'e');

		queue_.pop_front();
		REQUIRE(queue_.size() == 2);
		REQUIRE(!queue_.empty());
		REQUIRE(queue_.front().data1 == 6);
		REQUIRE(queue_.front().data2 == 'f');

		queue_.pop_front();
		REQUIRE(queue_.size() == 1);
		REQUIRE(!queue_.empty());
		REQUIRE(queue_.front().data1 == 7);
		REQUIRE(queue_.front().data2 == 'g');

		queue_.pop_front();
		REQUIRE(queue_.size() == 0);
		REQUIRE(queue_.empty());
		REQUIRE(copy_ctor_count == 0);
	}
}