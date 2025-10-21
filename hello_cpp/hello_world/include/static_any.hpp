#pragma once

#include <array>
#include <stdint.h>
#include <memory>
#include <cstring>
#include <type_traits>
#include <typeinfo>
#include <typeindex>
#include <cassert>
#include <sstream>
#include <string>

namespace detail { namespace static_any {

struct move_tag {};
struct copy_tag {};

enum class operation_t { query_type, query_size, copy, move, destroy };

using function_ptr_t = void(*)(operation_t operation, void* this_ptr, void* other_ptr);

}}

template <std::size_t N>
class static_any
{
public:
	template <typename T>
	struct is_static_any : public std::false_type {};

	template <std::size_t M>
	struct is_static_any<static_any<M>> : public std::true_type {};

	template <class T>
	static constexpr bool is_static_any_v = is_static_any<T>::value;

	using size_type = std::size_t;

	static_any();
	~static_any();

	template <class T,
			  class = std::enable_if_t<!is_static_any_v<std::decay_t<T>>>>
	static_any(T&&);

	static_any(const static_any&);

	template <std::size_t M, class = std::enable_if_t<M <= N>>
	static_any(const static_any<M>&);

	template <std::size_t M, class = std::enable_if_t<M <= N>>
	static_any(static_any<M>&&);

	template <class T,
			  class = std::enable_if_t<!is_static_any_v<std::decay_t<T>>>>
	static_any& operator=(T&& t);

	static_any& operator=(const static_any& any)
	{
		assign_from_any(any);
		return *this;
	}

	template <std::size_t M, class = std::enable_if_t<M <= N>>
	static_any& operator=(const static_any<M>& any)
	{
		assign_from_any(any);
		return *this;
	}

	template <std::size_t M, class = std::enable_if_t<M <= N>>
	static_any& operator=(static_any<M>&& any)
	{
		assign_from_any(std::move(any));
		return *this;
	}

	void reset();

	template <class T>
	const T& get() const;

	template <class T>
	T& get();

	template <class T>
	bool has() const;

	const std::type_info& type() const;

	bool empty() const;

	size_type size() const;

	static constexpr size_type capacity();

	template <class T, class... Args>
	void emplace(Args&&... args);

private:
	using operation_t = detail::static_any::operation_t;
	using function_ptr_t = detail::static_any::function_ptr_t;

	template <class T>
	void copy_or_move(T&& t);

	template <class T>
	void assign_from_any(T&&);

	template <std::size_t M, class CopyOrMoveTag>
	void assign_from_any(const static_any<M>&, CopyOrMoveTag);

	const std::type_info& query_type() const;

	size_type query_size() const;

	void destroy();

	template <class T>
	const T* as() const;

	template <class T>
	T* as();

	template <class _RefT>
	void call_copy_or_move(void* this_void_ptr, void* other_void_ptr);

	void call_operation(const function_ptr_t& function, void* this_void_ptr, void* other_void_ptr, detail::static_any::move_tag);

	void call_operation(const function_ptr_t& function, void* this_void_ptr, void* other_void_ptr, detail::static_any::copy_tag);

	template <class T>
	void copy_or_move_from_another(T&&);

	std::array<char, N> __buff;
	function_ptr_t __function{};

	template <std::size_t S>
	friend class static_any;

	template <class _ValueT, std::size_t S>
	friend _ValueT* any_cast(static_any<S>*);

	template <class _ValueT, std::size_t S>
	friend _ValueT& any_cast(static_any<S>&);
};

namespace detail { namespace static_any {

template <class T>
static void operation(operation_t operation, void* ptr1, void* ptr2)
{
	T* this_ptr = reinterpret_cast<T*>(ptr1);

	switch(operation)
	{
	case operation_t::query_type:
	{
		*reinterpret_cast<const std::type_info**>(ptr1) = &typeid(T);
		break;
	}
	case operation_t::query_size:
	{
		*reinterpret_cast<std::size_t*>(ptr1) = sizeof(T);
		break;
	}
	case operation_t::copy:
	{
		T* other_ptr = reinterpret_cast<T*>(ptr2);
		assert(this_ptr);
		assert(other_ptr);
		new(this_ptr)T(*other_ptr);
		break;
	}
	case operation_t::move:
	{
		T* other_ptr = reinterpret_cast<T*>(ptr2);
		assert(this_ptr);
		assert(other_ptr);
		new(this_ptr)T(std::move(*other_ptr));
		break;
	}
	case operation_t::destroy:
	{
		assert(this_ptr);
		this_ptr->~T();
		break;
	}
	}
}

template <class T>
static function_ptr_t get_function_for_type()
{
	return &static_any::operation<std::remove_cv_t<std::remove_reference_t<T>>>;
}

}}


template <std::size_t N>
constexpr typename static_any<N>::size_type static_any<N>::capacity()
{
	return N;
}
class bad_any_cast : public std::bad_cast
{
public:
	explicit bad_any_cast(const std::type_info& from,
						  const std::type_info& to);
	virtual ~bad_any_cast();

	const std::type_info& stored_type() const { return __from; }
	const std::type_info& target_type() const { return __to; }

	const char* what() const noexcept override
	{
		return __reason.c_str();
	}

private:
	const std::type_info& __from;
	const std::type_info& __to;
	std::string __reason;
};

template <std::size_t N>
class static_any_t
{
public:
	using size_type = std::size_t;

	static constexpr size_type capacity() { return N; }

	static_any_t() = default;
	static_any_t(const static_any_t&) = default;

	template <class _ValueT>
	static_any_t(_ValueT&& t)
	{
		copy(std::forward<_ValueT>(t));
	}

	template <class _ValueT>
	static_any_t& operator=(_ValueT&& t)
	{
		copy(std::forward<_ValueT>(t));
		return *this;
	}

	template <class _ValueT>
	_ValueT& get() { return *reinterpret_cast<_ValueT*>(__buff.data()); }

	template <class _ValueT>
	const _ValueT& get() const { return *reinterpret_cast<const _ValueT*>(__buff.data()); }

private:
	template <class _ValueT>
	void copy(_ValueT&& t)
	{
		using NonConstT = std::remove_cv_t<std::remove_reference_t<_ValueT>>;

#if __GNUG__ && __GNUC__ < 5
		static_assert(std::has_trivial_copy_constructor<NonConstT>::value, "_ValueT is not trivially copyable");
#else
		static_assert(std::is_trivially_copyable<NonConstT>::value, "_ValueT is not trivially copyable");
#endif

		static_assert(capacity() >= sizeof(_ValueT), "_ValueT is too big to be copied to static_any");

		std::memcpy(__buff.data(), reinterpret_cast<char*>(&t), sizeof(_ValueT));
	}

	std::array<char, N> __buff;
};