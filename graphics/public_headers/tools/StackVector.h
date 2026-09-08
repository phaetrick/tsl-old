#pragma once
#include <array>      // std::array
#include <cassert>    // assert
#include <utility>    // std::move, std::forward
#include <algorithm>  // std::move (range version)
#include <cstddef>    // std::size_t

namespace tsl {
	template<typename T, std::size_t MaxSize>
	class StackVector {
	public:
		StackVector() = default;

		StackVector(const StackVector&) = default;
		StackVector& operator=(const StackVector&) = default;
		StackVector(StackVector&&) = default;
		StackVector& operator=(StackVector&&) = default;

		void push_back(const T& val) {
			assert(size_ < MaxSize);
			data_[size_++] = val;
		}

		void push_back(T&& val) {
			assert(size_ < MaxSize);
			data_[size_++] = std::move(val);
		}

		template<typename... Args>
		T& emplace_back(Args&&... args) {
			assert(size_ < MaxSize);
			data_[size_] = T(std::forward<Args>(args)...);
			return data_[size_++];
		}

		void pop_back() { assert(size_ > 0); --size_; }
		void clear() { size_ = 0; }
		void resize(std::size_t n) { assert(n <= MaxSize); size_ = n; }

		T& operator[](std::size_t i) { assert(i < size_); return data_[i]; }
		const T& operator[](std::size_t i) const { assert(i < size_); return data_[i]; }

		T* data() { return data_.data(); }
		const T* data()  const { return data_.data(); }
		T* begin() { return data_.data(); }
		T* end() { return data_.data() + size_; }
		const T* begin() const { return data_.data(); }
		const T* end()   const { return data_.data() + size_; }

		std::size_t size()     const { return size_; }
		std::size_t capacity() const { return MaxSize; }
		bool        empty()    const { return size_ == 0; }
		bool        full()     const { return size_ == MaxSize; }

		T& front() { assert(size_ > 0); return data_[0]; }
		const T& front() const { assert(size_ > 0); return data_[0]; }
		T& back() { assert(size_ > 0); return data_[size_ - 1]; }
		const T& back()  const { assert(size_ > 0); return data_[size_ - 1]; }

		void erase(T* it) {
			assert(it >= begin() && it < end());
			std::move(it + 1, end(), it);
			--size_;
		}

	private:
		std::array<T, MaxSize> data_;
		std::size_t            size_{ 0 };
	};
}