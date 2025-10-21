#include <static_any.hpp>

template <std::size_t N>
static_any<N>::static_any()
{}

template <std::size_t N>
static_any<N>::~static_any()
{
	destroy();
}

template <std::size_t N>
template <class T, class>
static_any<N>::static_any(T&& v)
{
	copy_or_move(std::forward<T>(v));
}

template <std::size_t N>
static_any<N>::static_any(const static_any<N>& another)
{
	copy_or_move_from_another(another);
}

template <std::size_t N>
template <std::size_t M, class>
static_any<N>::static_any(const static_any<M>& another)
{
	copy_or_move_from_another(another);
}

template <std::size_t N>
template <std::size_t M, class>
static_any<N>::static_any(static_any<M>&& another)
{
	copy_or_move_from_another(std::move(another));
}

template <std::size_t N>
template <class T, class>
static_any<N>& static_any<N>::operator=(T&& t)
{
	static_assert(capacity() >= sizeof(T), "T is too big to be copied to static_any");

	using NonConstT = std::remove_cv_t<std::remove_reference_t<T>>;
	NonConstT* non_const_t = const_cast<NonConstT*>(&t);

	static_any temp = std::move_if_noexcept(*this);

	try
	{
		destroy();
		assert(__function == nullptr);

		call_copy_or_move<T&&>(__buff.data(), non_const_t);
	}
	catch(...)
	{
		*this = std::move(temp);
		throw;
	}

	__function = detail::static_any::get_function_for_type<T>();
	return *this;
}

template <std::size_t N>
void static_any<N>::reset() { destroy(); }

template <std::size_t N>
template <class T>
bool static_any<N>::has() const
{
	if (__function == detail::static_any::get_function_for_type<T>())
	{
		return true;
	}
	else if (__function)
	{
		// need to try another, possibly more costly way, as we may compare types across DLL boundaries
		return std::type_index(typeid(T)) == std::type_index(query_type());
	}
	return false;
}

template <std::size_t N>
const std::type_info& static_any<N>::type() const
{
	if (empty())
		return typeid(void);
	else
		return query_type();
}

template <std::size_t N>
bool static_any<N>::empty() const { return __function == nullptr; }

template <std::size_t N>
typename static_any<N>::size_type static_any<N>::size() const
{
	if (empty())
		return 0;
	else
		return query_size();
}


template <std::size_t N>
template <class T, class... Args>
void static_any<N>::emplace(Args&&... args)
{
	destroy();
	new(__buff.data()) T(std::forward<Args>(args)...);
	__function = detail::static_any::get_function_for_type<T>();
}

template <std::size_t N>
template <class T>
void static_any<N>::copy_or_move(T&& t)
{
	static_assert(capacity() >= sizeof(T), "T is too big to be copied to static_any");
	assert(__function == nullptr);

	using NonConstT = std::remove_cv_t<std::remove_reference_t<T>>;
	NonConstT* non_const_t = const_cast<NonConstT*>(&t);

	try {
		call_copy_or_move<T&&>(__buff.data(), non_const_t);
	}
	catch(...) {
		throw;
	}

	__function = detail::static_any::get_function_for_type<T>();
}

template <std::size_t N>
template <class T>
void static_any<N>::assign_from_any(T&& t)
{
	using CopyOrMoveTag = typename std::conditional<
		std::is_rvalue_reference<T&&>::value,
			detail::static_any::move_tag,
			detail::static_any::copy_tag
		>::type;

	assign_from_any(std::forward<T>(t), CopyOrMoveTag{});
}

template <std::size_t N>
template <std::size_t M, class CopyOrMoveTag>
void static_any<N>::assign_from_any(const static_any<M>& another, CopyOrMoveTag)
{
	if (another.__function == nullptr)
		return;

	static_any temp = std::move_if_noexcept(*this);
	void* other_data = reinterpret_cast<void*>(const_cast<char*>(another.__buff.data()));

	try {
		destroy();
		assert(__function == nullptr);

		call_operation(another.__function, __buff.data(), other_data, CopyOrMoveTag{});
	}
	catch(...) {
		*this = std::move(temp);
		throw;
	}

	__function= another.__function;
}

template <std::size_t N>
const std::type_info& static_any<N>::query_type() const
{
	assert(__function != nullptr);
	const std::type_info* ti ;
	__function(operation_t::query_type, &ti, nullptr);
	return *ti;
}

template <std::size_t N>
typename static_any<N>::size_type static_any<N>::query_size() const
{
	assert(__function != nullptr);
	std::size_t size;
	__function(operation_t::query_size, &size, nullptr);
	return size;
}

template <std::size_t N>
void static_any<N>::destroy()
{
	if (__function)
	{
		void* not_used = nullptr;
		__function(operation_t::destroy, __buff.data(), not_used);
		__function = nullptr;
	}
}

template <std::size_t N>
template <class T>
const T* static_any<N>::as() const
{
	return reinterpret_cast<const T*>(__buff.data());
}

template <std::size_t N>
template <class T>
T* static_any<N>::as()
{
	return reinterpret_cast<T*>(__buff.data());
}

template <std::size_t N>
template <class _RefT>
void static_any<N>::call_copy_or_move(void* this_void_ptr, void* other_void_ptr)
{
	using Tag = typename std::conditional<std::is_rvalue_reference<_RefT&&>::value,
				detail::static_any::move_tag,
				detail::static_any::copy_tag>::type;

	auto function = detail::static_any::get_function_for_type<_RefT>();
	call_operation(function, this_void_ptr, other_void_ptr, Tag{});
}

template <std::size_t N>
void static_any<N>::call_operation(const function_ptr_t& function, void* this_void_ptr, void* other_void_ptr, detail::static_any::move_tag)
{
	function(operation_t::move, this_void_ptr, other_void_ptr);
}

template <std::size_t N>
void static_any<N>::call_operation(const function_ptr_t& function, void* this_void_ptr, void* other_void_ptr, detail::static_any::copy_tag)
{
	function(operation_t::copy, this_void_ptr, other_void_ptr);
}

template <std::size_t N>
template <class T>
void static_any<N>::copy_or_move_from_another(T&& another)
{
	assert(__function == nullptr);

	if (another.__function == nullptr)
	{
		return;
	}

	using Tag = typename std::conditional<std::is_rvalue_reference<T&&>::value,
				detail::static_any::move_tag,
				detail::static_any::copy_tag>::type;

	void* other_data = reinterpret_cast<void*>(const_cast<char*>(another.__buff.data()));

	try {
		call_operation(another.__function, __buff.data(), other_data, Tag{});
	}
	catch(...) {
		throw;
	}

	__function= another.__function;
}


bad_any_cast::bad_any_cast(const std::type_info& from,
    const std::type_info& to) :
__from(from),
__to(to)
{
    std::ostringstream oss;
    oss << "failed conversion using any_cast: stored type "
    << from.name()
    << ", trying to cast to "
    << to.name();
    __reason = oss.str();
}

bad_any_cast::~bad_any_cast() {}

template <class _ValueT,
std::size_t S>
inline _ValueT* any_cast(static_any<S>* a)
{
    if (!a->template has<_ValueT>())
    return nullptr;

    return a->template as<_ValueT>();
}

template <class _ValueT,
std::size_t S>
inline const _ValueT* any_cast(const static_any<S>* a)
{
    return any_cast<const _ValueT>(const_cast<static_any<S>*>(a));
}

template <class _ValueT,
std::size_t S>
inline _ValueT& any_cast(static_any<S>& a)
{
    if (!a.template has<_ValueT>())
    throw bad_any_cast(a.type(), typeid(_ValueT));

    return *a.template as<_ValueT>();
}

template <class _ValueT,
std::size_t S>
inline const _ValueT& any_cast(const static_any<S>& a)
{
    return any_cast<const _ValueT>(const_cast<static_any<S>&>(a));
}

template <std::size_t S>
template <class T>
const T& static_any<S>::get() const
{
    return any_cast<T>(*this);
}

template <std::size_t S>
template <class T>
T& static_any<S>::get()
{
    return any_cast<T>(*this);
}

