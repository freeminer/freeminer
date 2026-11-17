#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <functional>
#include <iterator>
#include <mutex>
#include <memory>
#include <optional>
#include <ratio>
#include <variant>
#include <ostream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <thread>
#include <typeinfo>

// DUMP(expr, ...): expressions are evaluated once, in argument order.
// Deep pointer traversal is opt-in: DUMP(NDebugDump::Deep(pointer)).
#ifndef DUMP_TYPE
#define DUMP_TYPE 0
#endif
#ifndef DUMP_MAX_ELEMENTS
#define DUMP_MAX_ELEMENTS 64
#endif
#ifndef DUMP_MAX_STRING
#define DUMP_MAX_STRING 1024
#endif
#ifndef DUMP_MAX_DEPTH
#define DUMP_MAX_DEPTH 8
#endif

namespace NDebugDump
{

struct TContext
{
	std::size_t Depth = 0;
	bool DeepPointers = false;
};

template <class T>
struct TDeep
{
	const T &Value;
};

template <class T>
TDeep<T> Deep(const T &value)
{
	return {value};
}

template <class T>
inline constexpr bool IsDeep = false;
template <class T>
inline constexpr bool IsDeep<TDeep<T>> = true;

template <class T>
concept TupleLike = requires
{
	std::tuple_size<std::remove_cvref_t<T>>::value;
};

template <class T>
concept PointerLike = requires(const T &x)
{
	*x;
	bool(x);
}
&&!std::is_function_v<std::remove_reference_t<decltype(*std::declval<const T &>())>>;

// Match wrappers explicitly: dereferenceability alone does not imply ownership.
template <class T>
inline constexpr bool IsOptional = false;
template <class T>
inline constexpr bool IsOptional<std::optional<T>> = true;
template <class T>
inline constexpr bool IsVariant = false;
template <class... T>
inline constexpr bool IsVariant<std::variant<T...>> = true;
template <class T>
inline constexpr bool IsWeakPtr = false;
template <class T>
inline constexpr bool IsWeakPtr<std::weak_ptr<T>> = true;
template <class T>
inline constexpr bool IsReference = false;
template <class T>
inline constexpr bool IsReference<std::reference_wrapper<T>> = true;
template <class T>
inline constexpr bool IsDuration = false;
template <class R, class P>
inline constexpr bool IsDuration<std::chrono::duration<R, P>> = true;
template <class T>
inline constexpr bool IsTimePoint = false;
template <class C, class D>
inline constexpr bool IsTimePoint<std::chrono::time_point<C, D>> = true;

template <class Out>
void Quote(Out &out, std::string_view value)
{
	constexpr char Hex[] = "0123456789abcdef";
	out << '"';
	const auto count = std::min(value.size(), std::size_t(DUMP_MAX_STRING));
	for (std::size_t i = 0; i < count; ++i) {
		const auto c = static_cast<unsigned char>(value[i]);
		switch (c) {
		case '"':
			out << "\\\"";
			break;
		case '\\':
			out << "\\\\";
			break;
		case '\n':
			out << "\\n";
			break;
		case '\r':
			out << "\\r";
			break;
		case '\t':
			out << "\\t";
			break;
		default:
			if (c < 32 || c == 127) {
				out << "\\x" << Hex[c >> 4] << Hex[c & 15];
			} else {
				out << static_cast<char>(c);
			}
		}
	}
	out << '"';
	if (count < value.size()) {
		out << "...<" << value.size() << " bytes>";
	}
}

template <class Out, class T>
Out &dumpValue(Out &out, T &&x, TContext context = {})
{
	using U = std::remove_cvref_t<T>;
	if constexpr (IsDeep<U>) {
		context.DeepPointers = true;
		return dumpValue(out, x.Value, context);
	} else {
		if (context.Depth >= std::size_t(DUMP_MAX_DEPTH)) {
			out << "<max depth>";
			return out;
		}
		++context.Depth;
		if constexpr (std::is_same_v<U, std::nullptr_t>) {
			out << "nullptr";
		} else if constexpr (std::is_same_v<std::decay_t<T>, char *> ||
							 std::is_same_v<std::decay_t<T>, const char *>) {
			if constexpr (std::is_pointer_v<U>) {
				if (!x) {
					out << "nullptr";
					return out;
				}
			}
			// Bounded scan also handles non-terminated char arrays.
			std::size_t limit = std::size_t(DUMP_MAX_STRING);
			if constexpr (std::is_array_v<U>) {
				limit = std::min(limit, std::extent_v<U>);
			}
			std::size_t length = 0;
			while (length < limit && x[length]) {
				++length;
			}
			Quote(out, std::string_view(x, length));
			if (length == limit) {
				out << "...<limit>";
			}
		} else if constexpr (std::is_same_v<U, std::string> ||
							 std::is_same_v<U, std::string_view>) {
			Quote(out, std::string_view(x.data(), x.size()));
		} else if constexpr (std::is_same_v<U, unsigned char> ||
							 std::is_same_v<U, signed char>) {
			out << static_cast<int>(x);
		} else if constexpr (requires { x.PrintJSON(out); }) {
			x.PrintJSON(out);
		} else if constexpr (std::is_same_v<U, bool>) {
			out << (x ? "true" : "false");
		} else if constexpr (std::is_same_v<U, std::byte>) {
			out << "byte(" << std::to_integer<unsigned int>(x) << ')';
		} else if constexpr (IsOptional<U>) {
			if (x) {
				out << "some(";
				dumpValue(out, *x, context);
				out << ')';
			} else {
				out << "none";
			}
		} else if constexpr (std::is_same_v<U, std::nullopt_t>) {
			out << "none";
		} else if constexpr (std::is_same_v<U, std::monostate>) {
			out << "monostate";
		} else if constexpr (IsVariant<U>) {
			if (x.valueless_by_exception()) {
				out << "variant(valueless)";
			} else {
				out << "variant[" << x.index() << "](";
				std::visit([&](const auto &value) { dumpValue(out, value, context); }, x);
				out << ')';
			}
		} else if constexpr (IsReference<U>) {
			out << "ref(";
			dumpValue(out, x.get(), context);
			out << ')';
		} else if constexpr (IsWeakPtr<U>) {
			// Keep the object alive throughout formatting, without changing x.
			if (const auto locked = x.lock()) {
				out << "weak(";
				dumpValue(out, locked, context);
				out << ')';
			} else {
				out << "weak(expired)";
			}
		} else if constexpr (IsDuration<U>) {
			dumpValue(out, x.count(), context);
			using P = typename U::period;
			if constexpr (std::ratio_equal_v<P, std::nano>) {
				out << "ns";
			} else if constexpr (std::ratio_equal_v<P, std::micro>) {
				out << "us";
			} else if constexpr (std::ratio_equal_v<P, std::milli>) {
				out << "ms";
			} else if constexpr (std::ratio_equal_v<P, std::ratio<1>>) {
				out << "s";
			} else if constexpr (std::ratio_equal_v<P, std::ratio<60>>) {
				out << "min";
			} else if constexpr (std::ratio_equal_v<P, std::ratio<3600>>) {
				out << "h";
			} else {
				out << "*[" << P::num << '/' << P::den << "]s";
			}
		} else if constexpr (IsTimePoint<U>) {
			if constexpr (std::is_same_v<typename U::clock, std::chrono::system_clock>) {
				out << "system_time(";
			} else if constexpr (std::is_same_v<typename U::clock,
										 std::chrono::steady_clock>) {
				out << "steady_time(";
			} else {
				out << "time_point(";
			}
			dumpValue(out, x.time_since_epoch(), context);
			out << " since epoch)";
		} else if constexpr (TupleLike<T>) {
			out << '{';
			[&]<std::size_t... I>(std::index_sequence<I...>) {
				((void)(I < std::size_t(DUMP_MAX_ELEMENTS)
								 ? (void)(out << (I ? ", " : ""),
										   dumpValue(out, std::get<I>(x), context))
								 : (void)0),
						...);
			}(std::make_index_sequence<std::tuple_size_v<U>>{});
			if constexpr (std::tuple_size_v<U> > std::size_t(DUMP_MAX_ELEMENTS)) {
				out << (DUMP_MAX_ELEMENTS ? ", ..." : "...");
			}
			out << '}';
		} else if constexpr (requires {
								 std::begin(x);
								 std::end(x);
							 }) {
			if constexpr (requires { std::size(x); }) {
				if (const auto size = std::size(x); size > 3) {
					out << '<' << size << '>';
				}
			}
			out << '{';
			std::size_t count = 0;
			for (auto it = std::begin(x); it != std::end(x); ++it) {
				if (count) {
					out << ", ";
				}
				if (count == std::size_t(DUMP_MAX_ELEMENTS)) {
					out << "...";
					break;
				}
				if constexpr (requires {
								  typename U::key_type;
								  typename U::mapped_type;
							  }) {
					dumpValue(out, it->first, context);
					out << ": ";
					dumpValue(out, it->second, context);
				} else {
					dumpValue(out, *it, context);
				}
				++count;
			}
			out << '}';
		} else if constexpr (std::is_pointer_v<U> || PointerLike<T>) {
			if (!x) {
				out << "nullptr";
			} else if constexpr (std::is_pointer_v<U> &&
								 std::is_function_v<std::remove_pointer_t<U>>) {
				out << "<function pointer>";
			} else {
				if constexpr (std::is_pointer_v<U>) {
					out << const_cast<const void *>(
							static_cast<const volatile void *>(x));
				} else if constexpr (requires { static_cast<const void *>(x.get()); }) {
					out << static_cast<const void *>(x.get());
				} else {
					out << "<pointer-like>";
				}
				if constexpr (PointerLike<T>) {
					if (context.DeepPointers) {
						out << " -> ";
						dumpValue(out, *x, context);
					}
				}
			}
		} else if constexpr (requires { out << x; }) {
			out << x;
		} else if constexpr (requires { out << ToString(x); }) {
			out << ToString(x);
		} else if constexpr (std::is_enum_v<U>) {
			// Preserve custom stream/ToString names when available.
			dumpValue(out, static_cast<std::underlying_type_t<U>>(x), context);
		} else {
			out << "{...}";
		}
		return out;
	}
}

template <class Out, class T>
Out &dump(Out &out, const char *name, T &&x)
{
#if DUMP_TYPE
	// The standard library provides implementation-defined type names.
	out << typeid(std::remove_cvref_t<T>).name() << ' ';
#endif
	out << name << " = ";
	dumpValue(out, x);
	out << "; ";
	return out;
}

inline std::mutex &OutputMutex()
{
	static std::mutex mutex;
	return mutex;
}

} // namespace NDebugDump

#if !defined(DUMP_STREAM)
#define DUMP_STREAM std::cerr
#endif

#if !defined(DUMP_FILE)
#define DUMP_FILE << __FILE__ << ':' << __LINE__ << " "
#endif

#if !defined(DUMP_FUNCTION)
#define DUMP_FUNCTION << __PRETTY_FUNCTION__ << " "
//#define DUMP_FUNCTION
#endif

#if !defined(DUMP_ENDL)
//#define DUMP_ENDL << '\n'
#define DUMP_ENDL << std::endl
#endif

#if !defined(DUMP_THREAD)
#define DUMP_THREAD << "[ " << std::this_thread::get_id() << " ] "
#endif

#ifndef DUMP_TIME
#define DUMP_TIME 0
#endif

#if DUMP_TIME == 1
#define DUMP_TIME_STR                                                                    \
	<< "( " << std::chrono::system_clock::now().time_since_epoch().count() << " ) "
#elif DUMP_TIME
#define DUMP_TIME_STR DUMP_TIME
#endif

#if !defined(DUMP_TIME_STR)
#define DUMP_TIME_STR
#endif

#define DUMP_DETAIL_DUMPVAR(VAR) ::NDebugDump::dump(debugDumpRecord, #VAR, (VAR));

#define DUMP_DETAIL_DUMPHEAD                                                             \
	auto &debugDumpSink = (DUMP_STREAM);                                                 \
	std::ostringstream debugDumpRecord;                                                  \
	debugDumpRecord DUMP_FILE DUMP_TIME_STR DUMP_THREAD DUMP_FUNCTION;

#define DUMP_DETAIL_DUMPTAIL                                                             \
	const std::lock_guard<std::mutex> debugDumpLock(::NDebugDump::OutputMutex());        \
	debugDumpSink << debugDumpRecord.str().c_str() DUMP_ENDL;

#define DUMP_DETAIL_DUMP0()                                                              \
	do {                                                                                 \
		DUMP_DETAIL_DUMPHEAD DUMP_DETAIL_DUMPVAR(__PRETTY_FUNCTION__)                    \
				DUMP_DETAIL_DUMPTAIL                                                     \
	} while (0)
#define DUMP_DETAIL_DUMP1(V1)                                                            \
	do {                                                                                 \
		DUMP_DETAIL_DUMPHEAD DUMP_DETAIL_DUMPVAR(V1) DUMP_DETAIL_DUMPTAIL                \
	} while (0)
#define DUMP_DETAIL_DUMP2(V1, V2)                                                        \
	do {                                                                                 \
		DUMP_DETAIL_DUMPHEAD DUMP_DETAIL_DUMPVAR(V1) DUMP_DETAIL_DUMPVAR(V2)             \
				DUMP_DETAIL_DUMPTAIL                                                     \
	} while (0)
#define DUMP_DETAIL_DUMP3(V1, V2, V3)                                                    \
	do {                                                                                 \
		DUMP_DETAIL_DUMPHEAD DUMP_DETAIL_DUMPVAR(V1) DUMP_DETAIL_DUMPVAR(V2)             \
				DUMP_DETAIL_DUMPVAR(V3) DUMP_DETAIL_DUMPTAIL                             \
	} while (0)
#define DUMP_DETAIL_DUMP4(V1, V2, V3, V4)                                                \
	do {                                                                                 \
		DUMP_DETAIL_DUMPHEAD DUMP_DETAIL_DUMPVAR(V1) DUMP_DETAIL_DUMPVAR(V2)             \
				DUMP_DETAIL_DUMPVAR(V3) DUMP_DETAIL_DUMPVAR(V4) DUMP_DETAIL_DUMPTAIL     \
	} while (0)
#define DUMP_DETAIL_DUMP5(V1, V2, V3, V4, V5)                                            \
	do {                                                                                 \
		DUMP_DETAIL_DUMPHEAD DUMP_DETAIL_DUMPVAR(V1) DUMP_DETAIL_DUMPVAR(V2)             \
				DUMP_DETAIL_DUMPVAR(V3) DUMP_DETAIL_DUMPVAR(V4) DUMP_DETAIL_DUMPVAR(V5)  \
						DUMP_DETAIL_DUMPTAIL                                             \
	} while (0)
#define DUMP_DETAIL_DUMP6(V1, V2, V3, V4, V5, V6)                                        \
	do {                                                                                 \
		DUMP_DETAIL_DUMPHEAD DUMP_DETAIL_DUMPVAR(V1) DUMP_DETAIL_DUMPVAR(V2)             \
				DUMP_DETAIL_DUMPVAR(V3) DUMP_DETAIL_DUMPVAR(V4) DUMP_DETAIL_DUMPVAR(V5)  \
						DUMP_DETAIL_DUMPVAR(V6) DUMP_DETAIL_DUMPTAIL                     \
	} while (0)
#define DUMP_DETAIL_DUMP7(V1, V2, V3, V4, V5, V6, V7)                                    \
	do {                                                                                 \
		DUMP_DETAIL_DUMPHEAD DUMP_DETAIL_DUMPVAR(V1) DUMP_DETAIL_DUMPVAR(V2)             \
				DUMP_DETAIL_DUMPVAR(V3) DUMP_DETAIL_DUMPVAR(V4) DUMP_DETAIL_DUMPVAR(V5)  \
						DUMP_DETAIL_DUMPVAR(V6) DUMP_DETAIL_DUMPVAR(V7)                  \
								DUMP_DETAIL_DUMPTAIL                                     \
	} while (0)
#define DUMP_DETAIL_DUMP8(V1, V2, V3, V4, V5, V6, V7, V8)                                \
	do {                                                                                 \
		DUMP_DETAIL_DUMPHEAD DUMP_DETAIL_DUMPVAR(V1) DUMP_DETAIL_DUMPVAR(V2)             \
				DUMP_DETAIL_DUMPVAR(V3) DUMP_DETAIL_DUMPVAR(V4) DUMP_DETAIL_DUMPVAR(V5)  \
						DUMP_DETAIL_DUMPVAR(V6) DUMP_DETAIL_DUMPVAR(V7)                  \
								DUMP_DETAIL_DUMPVAR(V8) DUMP_DETAIL_DUMPTAIL             \
	} while (0)
#define DUMP_DETAIL_DUMP9(V1, V2, V3, V4, V5, V6, V7, V8, V9)                            \
	do {                                                                                 \
		DUMP_DETAIL_DUMPHEAD DUMP_DETAIL_DUMPVAR(V1) DUMP_DETAIL_DUMPVAR(V2)             \
				DUMP_DETAIL_DUMPVAR(V3) DUMP_DETAIL_DUMPVAR(V4) DUMP_DETAIL_DUMPVAR(V5)  \
						DUMP_DETAIL_DUMPVAR(V6) DUMP_DETAIL_DUMPVAR(V7)                  \
								DUMP_DETAIL_DUMPVAR(V8) DUMP_DETAIL_DUMPVAR(V9)          \
										DUMP_DETAIL_DUMPTAIL                             \
	} while (0)

#define DUMP_DETAIL_DUMP10(V1, V2, V3, V4, V5, V6, V7, V8, V9, V10)                      \
	do {                                                                                 \
		DUMP_DETAIL_DUMPHEAD DUMP_DETAIL_DUMPVAR(V1) DUMP_DETAIL_DUMPVAR(V2)             \
				DUMP_DETAIL_DUMPVAR(V3) DUMP_DETAIL_DUMPVAR(V4) DUMP_DETAIL_DUMPVAR(V5)  \
						DUMP_DETAIL_DUMPVAR(V6) DUMP_DETAIL_DUMPVAR(V7)                  \
								DUMP_DETAIL_DUMPVAR(V8) DUMP_DETAIL_DUMPVAR(V9)          \
										DUMP_DETAIL_DUMPVAR(V10) DUMP_DETAIL_DUMPTAIL    \
	} while (0)
#define DUMP_DETAIL_DUMP11(V1, V2, V3, V4, V5, V6, V7, V8, V9, V10, V11)                 \
	do {                                                                                 \
		DUMP_DETAIL_DUMPHEAD DUMP_DETAIL_DUMPVAR(V1) DUMP_DETAIL_DUMPVAR(V2)             \
				DUMP_DETAIL_DUMPVAR(V3) DUMP_DETAIL_DUMPVAR(V4) DUMP_DETAIL_DUMPVAR(V5)  \
						DUMP_DETAIL_DUMPVAR(V6) DUMP_DETAIL_DUMPVAR(V7)                  \
								DUMP_DETAIL_DUMPVAR(V8) DUMP_DETAIL_DUMPVAR(V9)          \
										DUMP_DETAIL_DUMPVAR(V10)                         \
												DUMP_DETAIL_DUMPVAR(V11)                 \
														DUMP_DETAIL_DUMPTAIL             \
	} while (0)
#define DUMP_DETAIL_DUMP12(V1, V2, V3, V4, V5, V6, V7, V8, V9, V10, V11, V12)            \
	do {                                                                                 \
		DUMP_DETAIL_DUMPHEAD DUMP_DETAIL_DUMPVAR(V1) DUMP_DETAIL_DUMPVAR(V2)             \
				DUMP_DETAIL_DUMPVAR(V3) DUMP_DETAIL_DUMPVAR(V4) DUMP_DETAIL_DUMPVAR(     \
						V5) DUMP_DETAIL_DUMPVAR(V6) DUMP_DETAIL_DUMPVAR(V7)              \
						DUMP_DETAIL_DUMPVAR(V8) DUMP_DETAIL_DUMPVAR(V9)                  \
								DUMP_DETAIL_DUMPVAR(V10) DUMP_DETAIL_DUMPVAR(V11)        \
										DUMP_DETAIL_DUMPVAR(V12) DUMP_DETAIL_DUMPTAIL    \
	} while (0)
#define DUMP_DETAIL_DUMP13(V1, V2, V3, V4, V5, V6, V7, V8, V9, V10, V11, V12, V13)       \
	do {                                                                                 \
		DUMP_DETAIL_DUMPHEAD DUMP_DETAIL_DUMPVAR(V1) DUMP_DETAIL_DUMPVAR(V2)             \
				DUMP_DETAIL_DUMPVAR(V3) DUMP_DETAIL_DUMPVAR(V4) DUMP_DETAIL_DUMPVAR(     \
						V5) DUMP_DETAIL_DUMPVAR(V6) DUMP_DETAIL_DUMPVAR(V7)              \
						DUMP_DETAIL_DUMPVAR(V8) DUMP_DETAIL_DUMPVAR(V9)                  \
								DUMP_DETAIL_DUMPVAR(V10) DUMP_DETAIL_DUMPVAR(V11)        \
										DUMP_DETAIL_DUMPVAR(V12)                         \
												DUMP_DETAIL_DUMPVAR(V13)                 \
														DUMP_DETAIL_DUMPTAIL             \
	} while (0)
#define DUMP_DETAIL_DUMP14(V1, V2, V3, V4, V5, V6, V7, V8, V9, V10, V11, V12, V13, V14)  \
	do {                                                                                 \
		DUMP_DETAIL_DUMPHEAD DUMP_DETAIL_DUMPVAR(V1) DUMP_DETAIL_DUMPVAR(                \
				V2) DUMP_DETAIL_DUMPVAR(V3) DUMP_DETAIL_DUMPVAR(V4)                      \
				DUMP_DETAIL_DUMPVAR(V5) DUMP_DETAIL_DUMPVAR(V6) DUMP_DETAIL_DUMPVAR(     \
						V7) DUMP_DETAIL_DUMPVAR(V8) DUMP_DETAIL_DUMPVAR(V9)              \
						DUMP_DETAIL_DUMPVAR(V10) DUMP_DETAIL_DUMPVAR(V11)                \
								DUMP_DETAIL_DUMPVAR(V12) DUMP_DETAIL_DUMPVAR(V13)        \
										DUMP_DETAIL_DUMPVAR(V14) DUMP_DETAIL_DUMPTAIL    \
	} while (0)
#define DUMP_DETAIL_DUMP15(                                                              \
		V1, V2, V3, V4, V5, V6, V7, V8, V9, V10, V11, V12, V13, V14, V15)                \
	do {                                                                                 \
		DUMP_DETAIL_DUMPHEAD DUMP_DETAIL_DUMPVAR(V1) DUMP_DETAIL_DUMPVAR(                \
				V2) DUMP_DETAIL_DUMPVAR(V3) DUMP_DETAIL_DUMPVAR(V4)                      \
				DUMP_DETAIL_DUMPVAR(V5) DUMP_DETAIL_DUMPVAR(V6) DUMP_DETAIL_DUMPVAR(     \
						V7) DUMP_DETAIL_DUMPVAR(V8) DUMP_DETAIL_DUMPVAR(V9)              \
						DUMP_DETAIL_DUMPVAR(V10) DUMP_DETAIL_DUMPVAR(V11)                \
								DUMP_DETAIL_DUMPVAR(V12) DUMP_DETAIL_DUMPVAR(V13)        \
										DUMP_DETAIL_DUMPVAR(V14)                         \
												DUMP_DETAIL_DUMPVAR(V15)                 \
														DUMP_DETAIL_DUMPTAIL             \
	} while (0)

/// https://groups.google.com/forum/#!searchin/kona-dev/variadic$20macro%7Csort:date/kona-dev/XMA-lDOqtlI/GCzdfZsD41sJ

#define DUMP_DETAIL_VA_NUM_ARGS_IMPL(                                                    \
		x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, N, ...)        \
	N
#define DUMP_DETAIL_VA_NUM_ARGS(...)                                                     \
	DUMP_DETAIL_VA_NUM_ARGS_IMPL(__VA_ARGS__ __VA_OPT__(, ) 15, 14, 13, 12, 11, 10, 9,   \
			8, 7, 6, 5, 4, 3, 2, 1, 0)

#define DUMP_DETAIL_MAKE_VAR_MACRO_IMPL_CONCAT(PREFIX, NUM_ARGS) PREFIX##NUM_ARGS
#define DUMP_DETAIL_MAKE_VAR_MACRO_IMPL(PREFIX, NUM_ARGS)                                \
	DUMP_DETAIL_MAKE_VAR_MACRO_IMPL_CONCAT(PREFIX, NUM_ARGS)
#define DUMP_DETAIL_MAKE_VAR_MACRO(PREFIX, ...)                                          \
	DUMP_DETAIL_MAKE_VAR_MACRO_IMPL(PREFIX, DUMP_DETAIL_VA_NUM_ARGS(__VA_ARGS__))

#define DUMP(...)                                                                        \
	DUMP_DETAIL_MAKE_VAR_MACRO(DUMP_DETAIL_DUMP, __VA_ARGS__)                            \
	(__VA_ARGS__)
//#define DUMP() do { DUMP(__PRETTY_FUNCTION__); } while(0)
