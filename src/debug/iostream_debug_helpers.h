// https://github.com/ClickHouse/ClickHouse.git base/base/iostream_debug_helpers.h

#pragma once

#include "demangle.h"
#include "getThreadId.h"
#include "magic_enum.hpp"
#include <cstring>
#include <string_view>
#include <type_traits>
#include <tuple>
#include <iomanip>
#include <iostream>

/** Usage:
  *
  * DUMP(variable...)
  */

#if !defined(DUMP_DEMANGLE)
#define DUMP_DEMANGLE demangle
#endif

#if !defined(DUMP_TYPE)
#define DUMP_TYPE 0
#endif

template <typename Out, typename T>
Out & dumpValue(Out &, T &&);


/// Catch-all case.
template <int priority, typename Out, typename T>
std::enable_if_t<priority == -1, Out> & dumpImpl(Out & out, T &&)
{
    out << "{...}";
    return out;
}

/// An object, that could be output with operator <<.
template <int priority, typename Out, typename T>
std::enable_if_t<priority == 0, Out> & dumpImpl(Out & out, T && x, std::decay_t<decltype(std::declval<Out &>() << std::declval<T>())> * = nullptr)
{
    out << x;
    return out;
}

/// A pointer-like object.
template <int priority, typename Out, typename T>
std::enable_if_t<priority == 1
    /// Protect from the case when operator * do effectively nothing (function pointer).
    && !std::is_same_v<std::decay_t<T>, std::decay_t<decltype(*std::declval<T>())>>
    , Out> & dumpImpl(Out & out, T && x, std::decay_t<decltype(*std::declval<T>())> * = nullptr)
{
    if (!x) {
        out << "nullptr";
        return out;
    }
    if constexpr (std::is_pointer_v<T>) {
        out << "*" << (long long) x << " ";
    }
    return dumpValue(out, *x);
}

/// Container.
template <int priority, typename Out, typename T>
std::enable_if_t<priority == 2, Out> & dumpImpl(Out & out, T && x, std::decay_t<decltype(std::begin(std::declval<T>()))> * = nullptr)
{
    bool first = true;
    out << "{";
    for (const auto & elem : x)
    {
        if (first)
            first = false;
        else
            out << ", ";
        dumpValue(out, elem);
    }
    out << "}";
    return out;
}


template <int priority, typename Out, typename T>
std::enable_if_t<priority == 3 && std::is_enum_v<std::decay_t<T>>, Out> &
dumpImpl(Out & out, T && x)
{
    out << magic_enum::enum_name(x);
    return out;
}

/// string and const char * - output not as container or pointer.

template <int priority, typename Out, typename T>
std::enable_if_t<priority == 3 &&
						 (std::is_same_v<std::decay_t<T>, std::string> ||
								 std::is_same_v<std::decay_t<T>, std::string_view> ||
								 std::is_same_v<std::decay_t<T>, const char *>),
		Out> &
dumpImpl(Out &out, T &&x)
{
    out << std::quoted(x);
    return out;
}

/// UInt8 - output as number, not char.

template <int priority, typename Out, typename T>
std::enable_if_t<priority == 3 && std::is_same_v<std::decay_t<T>, unsigned char>, Out> &
dumpImpl(Out & out, T && x)
{
    out << int(x);
    return out;
}


/// Tuple, pair
template <size_t N, typename Out, typename T>
Out & dumpTupleImpl(Out & out, T && x)
{
    if constexpr (N == 0)
        out << "{";
    else
        out << ", ";

    dumpValue(out, std::get<N>(x));

    if constexpr (N + 1 == std::tuple_size_v<std::decay_t<T>>)
        out << "}";
    else
        dumpTupleImpl<N + 1>(out, x);

    return out;
}

template <int priority, typename Out, typename T>
std::enable_if_t<priority == 4, Out> & dumpImpl(Out & out, T && x, std::decay_t<decltype(std::get<0>(std::declval<T>()))> * = nullptr)
{
    return dumpTupleImpl<0>(out, x);
}


template <int priority, typename Out, typename T>
Out & dumpDispatchPriorities(Out & out, T && x, std::decay_t<decltype(dumpImpl<priority>(std::declval<Out &>(), std::declval<T>()))> *)
{
    return dumpImpl<priority>(out, x);
}

struct LowPriority { LowPriority(void *) {} };

template <int priority, typename Out, typename T>
Out & dumpDispatchPriorities(Out & out, T && x, LowPriority)
{
    return dumpDispatchPriorities<priority - 1>(out, x, nullptr);
}


template <typename Out, typename T>
Out & dumpValue(Out & out, T && x)
{
    return dumpDispatchPriorities<5>(out, x, nullptr);
}


template <typename Out, typename T>
Out & dump(Out & out, const char * name, T && x)
{
    // Dumping string literal, printing name and demangled type is irrelevant.
    if constexpr (std::is_same_v<const char *, std::decay_t<std::remove_reference_t<T>>>)
    {
         const auto name_len = strlen(name);
         const auto value_len = strlen(x);
         // `name` is the same as quoted `x`
         if (name_len > 2 && value_len > 0 && name[0] == '"' && name[name_len - 1] == '"'
                 && strncmp(name + 1, x, std::min(value_len, name_len) - 1) == 0) {
             out << x << "; ";
             return out;
         }
    }

    out 
#if DUMP_TYPE
    << DUMP_DEMANGLE(typeid(x).name()) << ' ' 
#endif
    << name << " = ";
    dumpValue(out, x) << "; ";
    return out;
}

#ifdef __clang__
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#endif

#if !defined(DUMP_STREAM)
    #define DUMP_STREAM std::cerr
#endif

#if !defined(DUMP_FILE)
    #define DUMP_FILE << __FILE__ << ':' << __LINE__ << ' '
#endif

#if !defined(DUMP_FUNCTION)
    //#define DUMP_FUNCTION <<  __PRETTY_FUNCTION__ << ' '
    #define DUMP_FUNCTION
#endif

#if !defined(DUMP_ENDL)
    #define DUMP_ENDL << '\n'
#endif

#if !defined(DUMP_THREAD)
    #define DUMP_THREAD << " [ " << getThreadId() << " ] "
#endif

#define DUMPVAR(VAR) ::dump(DUMP_STREAM, #VAR, (VAR));
#define DUMPHEAD DUMP_STREAM DUMP_FILE DUMP_THREAD DUMP_FUNCTION;

#define DUMPTAIL DUMP_STREAM DUMP_ENDL;

#define DUMP1(V1) do { DUMPHEAD DUMPVAR(V1) DUMPTAIL } while(0)
#define DUMP2(V1, V2) do { DUMPHEAD DUMPVAR(V1) DUMPVAR(V2) DUMPTAIL } while(0)
#define DUMP3(V1, V2, V3) do { DUMPHEAD DUMPVAR(V1) DUMPVAR(V2) DUMPVAR(V3) DUMPTAIL } while(0)
#define DUMP4(V1, V2, V3, V4) do { DUMPHEAD DUMPVAR(V1) DUMPVAR(V2) DUMPVAR(V3) DUMPVAR(V4) DUMPTAIL } while(0)
#define DUMP5(V1, V2, V3, V4, V5) do { DUMPHEAD DUMPVAR(V1) DUMPVAR(V2) DUMPVAR(V3) DUMPVAR(V4) DUMPVAR(V5) DUMPTAIL } while(0)
#define DUMP6(V1, V2, V3, V4, V5, V6) do { DUMPHEAD DUMPVAR(V1) DUMPVAR(V2) DUMPVAR(V3) DUMPVAR(V4) DUMPVAR(V5) DUMPVAR(V6) DUMPTAIL } while(0)
#define DUMP7(V1, V2, V3, V4, V5, V6, V7) do { DUMPHEAD DUMPVAR(V1) DUMPVAR(V2) DUMPVAR(V3) DUMPVAR(V4) DUMPVAR(V5) DUMPVAR(V6) DUMPVAR(V7) DUMPTAIL } while(0)
#define DUMP8(V1, V2, V3, V4, V5, V6, V7, V8) do { DUMPHEAD DUMPVAR(V1) DUMPVAR(V2) DUMPVAR(V3) DUMPVAR(V4) DUMPVAR(V5) DUMPVAR(V6) DUMPVAR(V7) DUMPVAR(V8) DUMPTAIL } while(0)
#define DUMP9(V1, V2, V3, V4, V5, V6, V7, V8, V9) do { DUMPHEAD DUMPVAR(V1) DUMPVAR(V2) DUMPVAR(V3) DUMPVAR(V4) DUMPVAR(V5) DUMPVAR(V6) DUMPVAR(V7) DUMPVAR(V8) DUMPVAR(V9) DUMPTAIL } while(0)
#define DUMP10(V1, V2, V3, V4, V5, V6, V7, V8, V9, V10) do { DUMPHEAD DUMPVAR(V1) DUMPVAR(V2) DUMPVAR(V3) DUMPVAR(V4) DUMPVAR(V5) DUMPVAR(V6) DUMPVAR(V7) DUMPVAR(V8) DUMPVAR(V9) DUMPVAR(V10) DUMPTAIL } while(0)
#define DUMP11(V1, V2, V3, V4, V5, V6, V7, V8, V9, V10, V11) do { DUMPHEAD DUMPVAR(V1) DUMPVAR(V2) DUMPVAR(V3) DUMPVAR(V4) DUMPVAR(V5) DUMPVAR(V6) DUMPVAR(V7) DUMPVAR(V8) DUMPVAR(V9) DUMPVAR(V10) DUMPVAR(V11) DUMPTAIL } while(0)
#define DUMP12(V1, V2, V3, V4, V5, V6, V7, V8, V9, V10, V11, V12) do { DUMPHEAD DUMPVAR(V1) DUMPVAR(V2) DUMPVAR(V3) DUMPVAR(V4) DUMPVAR(V5) DUMPVAR(V6) DUMPVAR(V7) DUMPVAR(V8) DUMPVAR(V9) DUMPVAR(V10) DUMPVAR(V11) DUMPVAR(V12) DUMPTAIL } while(0)
#define DUMP13(V1, V2, V3, V4, V5, V6, V7, V8, V9, V10, V11, V12, V13) do { DUMPHEAD DUMPVAR(V1) DUMPVAR(V2) DUMPVAR(V3) DUMPVAR(V4) DUMPVAR(V5) DUMPVAR(V6) DUMPVAR(V7) DUMPVAR(V8) DUMPVAR(V9) DUMPVAR(V10) DUMPVAR(V11) DUMPVAR(V12) DUMPVAR(V13) DUMPTAIL } while(0)
#define DUMP14(V1, V2, V3, V4, V5, V6, V7, V8, V9, V10, V11, V12, V13, V14) do { DUMPHEAD DUMPVAR(V1) DUMPVAR(V2) DUMPVAR(V3) DUMPVAR(V4) DUMPVAR(V5) DUMPVAR(V6) DUMPVAR(V7) DUMPVAR(V8) DUMPVAR(V9) DUMPVAR(V10) DUMPVAR(V11) DUMPVAR(V12) DUMPVAR(V13) DUMPVAR(V14) DUMPTAIL } while(0)
#define DUMP15(V1, V2, V3, V4, V5, V6, V7, V8, V9, V10, V11, V12, V13, V14, V15) do { DUMPHEAD DUMPVAR(V1) DUMPVAR(V2) DUMPVAR(V3) DUMPVAR(V4) DUMPVAR(V5) DUMPVAR(V6) DUMPVAR(V7) DUMPVAR(V8) DUMPVAR(V9) DUMPVAR(V10) DUMPVAR(V11) DUMPVAR(V12) DUMPVAR(V13) DUMPVAR(V14) DUMPVAR(V15) DUMPTAIL } while(0)

/// https://groups.google.com/forum/#!searchin/kona-dev/variadic$20macro%7Csort:date/kona-dev/XMA-lDOqtlI/GCzdfZsD41sJ

#define VA_NUM_ARGS_IMPL(x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, N, ...) N
#define VA_NUM_ARGS(...) VA_NUM_ARGS_IMPL(__VA_ARGS__, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1)

#define MAKE_VAR_MACRO_IMPL_CONCAT(PREFIX, NUM_ARGS) PREFIX ## NUM_ARGS
#define MAKE_VAR_MACRO_IMPL(PREFIX, NUM_ARGS) MAKE_VAR_MACRO_IMPL_CONCAT(PREFIX, NUM_ARGS)
#define MAKE_VAR_MACRO(PREFIX, ...) MAKE_VAR_MACRO_IMPL(PREFIX, VA_NUM_ARGS(__VA_ARGS__))

#define DUMP(...) MAKE_VAR_MACRO(DUMP, __VA_ARGS__)(__VA_ARGS__)
