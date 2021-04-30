
#pragma once

#if defined(__clang__) && defined(__clang_minor__)
#define CCT_CLANG (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
#elif defined(__GNUC__) && defined(__GNUC_MINOR__) && defined(__GNUC_PATCHLEVEL__)
#define CCT_GCC (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#endif

#if defined(__GNUC__)
#define CCT_LIKELY(x) (__builtin_expect(!!(x), 1))
#define CCT_UNLIKELY(x) (__builtin_expect(!!(x), 0))
#else
#define CCT_LIKELY(x) (!!(x))
#define CCT_UNLIKELY(x) (!!(x))
#endif

#if defined(CCT_GCC) && CCT_GCC < 40600
#define CCT_PUSH_WARNING
#define CCT_POP_WARNING
#else
#define CCT_PUSH_WARNING _Pragma("GCC diagnostic push")
#define CCT_POP_WARNING _Pragma("GCC diagnostic pop")
#endif
#define CCT_DISABLE_WARNING_INTERNAL(warningName) #warningName
#define CCT_DISABLE_WARNING(warningName) _Pragma(CCT_DISABLE_WARNING_INTERNAL(GCC diagnostic ignored warningName))
#ifdef CCT_CLANG
#define CCT_CLANG_DISABLE_WARNING(warningName) CCT_DISABLE_WARNING(warningName)
#define CCT_GCC_DISABLE_WARNING(warningName)
#else
#define CCT_CLANG_DISABLE_WARNING(warningName)
#define CCT_GCC_DISABLE_WARNING(warningName) CCT_DISABLE_WARNING(warningName)
#endif

#define CCT_COLD __attribute__((__cold__))
#define CCT_HOT __attribute__((hot))
#define CCT_NOINLINE __attribute__((__noinline__))
#define CCT_ALWAYS_INLINE inline __attribute__((__always_inline__))
#define CCT_NORETURN __attribute__((__noreturn__))
#define CCT_PACKED __attribute__((__packed__))

#define CCT_STRINGIFY(x) #x
#define CCT_VER_STRING(major, minor, patch) CCT_STRINGIFY(major) "." CCT_STRINGIFY(minor) "." CCT_STRINGIFY(patch)

#if defined(__clang__)
#define CCT_COMPILER_NAME "clang"
#define CCT_COMPILER_VERSION \
  CCT_COMPILER_NAME " " CCT_VER_STRING(__clang_major__, __clang_minor__, __clang_patchlevel__)
#elif defined(__GNUC__)
#define CCT_COMPILER_NAME "g++"
#define CCT_COMPILER_VERSION CCT_COMPILER_NAME " " CCT_VER_STRING(__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__)
#else
#error "Unknown compiler"
#endif

#ifndef NDEBUG
#define CCT_DEBUG_MODE 1
#endif