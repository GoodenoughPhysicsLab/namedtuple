#pragma once

#if defined(_MSC_VER) && !defined(__clang__)
    #include <cstdlib>
#endif

namespace ctb::shutdown {

#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
[[noreturn]]
inline void terminate() noexcept {
    // https://llvm.org/doxygen/Compiler_8h_source.html
#if defined(__has_builtin)
    #if __has_builtin(__builtin_trap)
    __builtin_trap();
    #elif __has_builtin(__builtin_abort)
    __builtin_abort();
    #else
    ::std::abort();
    #endif
#else
    ::std::abort();
#endif
}

#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
[[noreturn]]
inline void unreachable() noexcept {
#if defined(_MSC_VER) && !defined(__clang__)  // MSVC
    __assume(false);
#else  // GCC, Clang
    __builtin_unreachable();
#endif
}

}  // namespace ctb::shutdown
