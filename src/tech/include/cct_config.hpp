#pragma once

#if defined(__clang__) && defined(__clang_minor__)
#define CCT_CLANG (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
#elif defined(__GNUC__) && defined(__GNUC_MINOR__) && defined(__GNUC_PATCHLEVEL__)
#define CCT_GCC (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#elif defined(_MSC_FULL_VER)
#define CCT_MSVC _MSC_FULL_VER
#endif

#if defined(__GNUC__)
#define CCT_LIKELY(x) (__builtin_expect(!!(x), 1))
#define CCT_UNLIKELY(x) (__builtin_expect(!!(x), 0))
#else
#define CCT_LIKELY(x) (!!(x))
#define CCT_UNLIKELY(x) (!!(x))
#endif

#define CCT_PUSH_WARNING _Pragma("GCC diagnostic push")
#define CCT_POP_WARNING _Pragma("GCC diagnostic pop")

#define CCT_DISABLE_WARNING_INTERNAL(warningName) #warningName
#define CCT_DISABLE_WARNING(warningName) _Pragma(CCT_DISABLE_WARNING_INTERNAL(GCC diagnostic ignored warningName))
#ifdef CCT_CLANG
#define CCT_CLANG_DISABLE_WARNING(warningName) CCT_DISABLE_WARNING(warningName)
#define CCT_GCC_DISABLE_WARNING(warningName)
#else
#define CCT_CLANG_DISABLE_WARNING(warningName)
#define CCT_GCC_DISABLE_WARNING(warningName) CCT_DISABLE_WARNING(warningName)
#endif

#ifdef CCT_MSVC
#define CCT_ALWAYS_INLINE __forceinline
#define CCT_NOINLINE __declspec(noinline)
#elif defined(__GNUC__)
#define CCT_ALWAYS_INLINE inline __attribute__((__always_inline__))
#define CCT_NOINLINE __attribute__((__noinline__))
#else
#define CCT_ALWAYS_INLINE inline
#define CCT_NOINLINE
#endif

#define CCT_STRINGIFY(x) #x
#define CCT_VER_STRING(major, minor, patch) CCT_STRINGIFY(major) "." CCT_STRINGIFY(minor) "." CCT_STRINGIFY(patch)

#ifdef CCT_CLANG
#define CCT_COMPILER_NAME "clang"
#define CCT_COMPILER_VERSION \
  CCT_COMPILER_NAME " " CCT_VER_STRING(__clang_major__, __clang_minor__, __clang_patchlevel__)
#elif defined(__GNUC__)
#define CCT_COMPILER_NAME "g++"
#define CCT_COMPILER_VERSION CCT_COMPILER_NAME " " CCT_VER_STRING(__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__)
#elif defined(_MSC_VER)
#define CCT_COMPILER_NAME "MSVC"
#define CCT_COMPILER_VERSION CCT_COMPILER_NAME " " CCT_STRINGIFY(_MSC_FULL_VER)
#else
#error "Unknown compiler. Only clang, gcc and MSVC are supported."
#endif

#ifndef CCT_CLANG
// Clang does not support 'Allow initializing aggregates from a parenthesized list of values' (nor CTAD) yet.
// Although it's a C++20 project, it's nice to support clang so let's keep this switch for now, until clang finally
// implements it. More information here:
// https://stackoverflow.com/questions/70260994/automatic-template-deduction-c20-with-aggregate-type
#define CCT_AGGR_INIT_CXX20
#endif