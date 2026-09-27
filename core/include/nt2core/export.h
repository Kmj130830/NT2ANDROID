#pragma once

#if defined(_WIN32) && defined(NT2CORE_BUILD_DLL)
    #define NT2CORE_API __declspec(dllexport)
#elif defined(_WIN32)
    #define NT2CORE_API __declspec(dllimport)
#elif defined(__GNUC__) || defined(__clang__)
    #define NT2CORE_API __attribute__((visibility("default")))
#else
    #define NT2CORE_API
#endif
