#include <iostream>

#if __cpp_lib_filesystem || (defined(__cplusplus) && __cplusplus >= 201703L)
#include <filesystem>
#endif

int main(){

#if __cpp_lib_filesystem || (defined(__cplusplus) && __cplusplus >= 201703L)
char fs = std::filesystem::path::preferred_separator;
std::cout << "OK: C++17 filesystem enabled" << std::endl;
#endif

#if defined(_MSC_VER)
#if _HAS_CXX17
std::cout << "OK: MSVC has C++17 enabled" << std::endl;
return EXIT_SUCCESS;
#else
std::cerr << "ERROR: MSVC does not have C++17 enabled" << std::endl;
return EXIT_FAILURE;
#endif
#elif defined(__cplusplus) && __cplusplus >= 201703L
std::cout << "OK: C++17 enabled" << std::endl;
return EXIT_SUCCESS;
#else
std::cerr << "ERROR: C++17 not enabled" << std::endl;
return EXIT_FAILURE;
#endif
}
