#pragma once

#ifndef ES_buffer_queue_HPP
#define ES_buffer_queue_HPP

#include <array>
#include <utility>
#include <new>

namespace es {
namespace buffer {

template <class T, template <class, class> class Deque, unsigned int ChunkSize, class Allocator = std::allocator<std::array<char, sizeof(T) * ChunkSize>>>
class buffer_queue
{
public:
	using self_type = buffer_queue<T, Deque, ChunkSize, Allocator>;

	// let's use these member types for now per cppreference...
	using value_type = std::array<char, sizeof(T) * ChunkSize>;
	using allocator_type = Allocator;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using reference = value_type & ;
	using const_reference = const value_type & ;
	using pointer = typename std::allocator_traits<Allocator>::pointer;
	using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;
	using iterator = typename Deque<value_type, Allocator>::iterator;
	using const_iterator = iterator const;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	// ctors
	explicit buffer_queue(Allocator const& alloc)
		: deque_(alloc), front_chunk_idx_(0), back_chunk_idx_(-1)
	{}

	buffer_queue()
		: deque_(), front_chunk_idx_(0), back_chunk_idx_(-1)
	{}

	// we don't need copy ctor for now
	// buffer_queue(self_type const& other) = default;
	buffer_queue(self_type&& other) = default;
	// maybe enable this later
	// buffer_queue(self_type&& other, Allocator const& alloc);

	~buffer_queue() = default; // default destructor

	bool empty() const { return deque_.empty(); }
	// careful ! size doesn't return the number of chunks, but rather the number of T elements
	size_type size() const 
	{ 
		if (deque_.empty()) return 0;
		if (deque_.size() == 1)
		{
			return back_chunk_idx_ - front_chunk_idx_ + 1;
		}

		return (deque_.size() - 2) * ChunkSize + (back_chunk_idx_ + 1) + (ChunkSize - front_chunk_idx_); 
	}

	void clear()
	{
		deque_.clear();
		front_chunk_idx_ = 0;
		back_chunk_idx_ = -1;
	}

	// careful ! these are iterator to the chunks (might create our own custom iterator later)
	iterator begin() { return deque_.begin(); }
	const_iterator cbegin() const { return deque_.cbegin(); }
	iterator end() { return deque_.end(); }
	const_iterator cend() const { return deque_.cend(); }

	void swap(self_type& other) noexcept
	{
		using std::swap;

		swap(deque_, other.deque_);
		swap(front_chunk_idx_, other.front_chunk_idx_);
		swap(back_chunk_idx_, other.back_chunk_idx_);
	}

	// no bounds checking
	T& front()
	{
		return *reinterpret_cast<T*>(&deque_.front()[front_chunk_idx_ * sizeof(T)]);
	}

	// no bounds checking
	T const& front() const
	{
		return *reinterpret_cast<T*>(&deque_.front()[front_chunk_idx_ * sizeof(T)]);
	}

	// calling pop_front if buffer_deque is empty is undefined behavior...
	void pop_front()
	{
		// if (deque_.empty()) return;
		++front_chunk_idx_;

		if (deque_.size() == 1)
		{
			if (front_chunk_idx_ == back_chunk_idx_ + 1)
			{
				deque_.pop_front();
				front_chunk_idx_ = back_chunk_idx_ = 0;
				return;
			}
		}
		else
		{
			if (front_chunk_idx_ == ChunkSize)
			{
				// call destructors ?
				deque_.pop_front();
				front_chunk_idx_ = 0;
			}
		}
	}

	void push_back(T const& value)
	{
		if (deque_.empty()) deque_.emplace_back();
		++back_chunk_idx_;
		// check if we've finished the current back chunk
		if (back_chunk_idx_ == ChunkSize)
		{
			deque_.emplace_back();
			back_chunk_idx_ = 0;
		}
		
		// placement new
		new (&deque_.back()[back_chunk_idx_ * sizeof(T)]) T(value);
	}

	void push_back(T&& value)
	{
		if (deque_.empty()) deque_.emplace_back();
		++back_chunk_idx_;
		// check if we've finished the current back chunk
		if (back_chunk_idx_ == ChunkSize)
		{
			deque_.emplace_back();
			back_chunk_idx_ = 0;
		}
	
		// placement new
		new (&deque_.back()[back_chunk_idx_ * sizeof(T)]) T(std::move(value));
	}

	template <class... Args>
	void emplace_back(Args&&... args)
	{
		if (deque_.empty()) deque_.emplace_back();
		++back_chunk_idx_;
		// check if we've finished the current back chunk
		if (back_chunk_idx_ == ChunkSize)
		{
			deque_.emplace_back();
			back_chunk_idx_ = 0;
		}

		// placement new
		new (&deque_.back()[back_chunk_idx_ * sizeof(T)]) T(std::forward<Args>(args)...);
	}

private:
	Deque<std::array<char, sizeof(T) * ChunkSize>, Allocator> deque_;
	unsigned int front_chunk_idx_ = 0;
	unsigned int back_chunk_idx_ = -1;
};

}
}

#endif // ES_buffer_queue_HPP