///
/// @file  intrin_wrapper.hpp
/// @brief Some intrinsic functions used by the project.
///

#ifndef SYNC_CELL_INTRIN_WRAPPER_HPP
#define SYNC_CELL_INTRIN_WRAPPER_HPP

// Compiler Extend Attribute
#if defined(_MSC_VER)
#define SC_ATTR_ALWAYS_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define SC_ATTR_ALWAYS_INLINE __attribute__((always_inline))
#else
#define SC_ATTR_ALWAYS_INLINE
#endif

// Architecture Macro Define
#if defined(__x86_64__) || defined(_M_X64)
#define _SC_X86_64_
#endif
#if defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
#define _SC_X86_32_
#endif
#if defined(__aarch64__) || defined(_M_ARM64)
#define _SC_ARM64_
#endif
#if defined(__arm__) || defined(_M_ARM)
#define _SC_ARM32_
#endif
#if defined(mips) || defined(__mips__) || defined(__mips)
#define _SC_MIPS_
#endif

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#if defined(__GNUC__) || defined(__clang__)
#if defined(_SC_X86_64_) || defined(__SC_X86_32_)
#include <immintrin.h>
#endif
#endif


inline void SC_ATTR_ALWAYS_INLINE spin_loop_hint() noexcept
{
#if defined(_SC_X86_64_) || defined(_SC_X86_32_)
#if defined(_MSC_VER)
#pragma intrinsic(_mm_pause)
#endif
    _mm_pause();
#endif
}

#endif //SYNC_CELL_INTRIN_WRAPPER_HPP
