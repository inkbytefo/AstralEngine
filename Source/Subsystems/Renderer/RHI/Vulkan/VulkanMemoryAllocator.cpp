#define VMA_IMPLEMENTATION
// Suppress warnings for VMA implementation
#if defined(_MSC_VER)
    #pragma warning(push, 0)
    #pragma warning(disable: 4189) // local variable is initialized but not referenced
    #pragma warning(disable: 4100) // unreferenced formal parameter
    #pragma warning(disable: 4127) // conditional expression is constant
    #pragma warning(disable: 4324) // structure was padded due to alignment specifier
    #pragma warning(disable: 4505) // unreferenced local function has been removed
#elif defined(__clang__) || defined(__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-variable"
    #pragma GCC diagnostic ignored "-Wunused-parameter"
    #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

#include "vk_mem_alloc.h"

#if defined(_MSC_VER)
    #pragma warning(pop)
#elif defined(__clang__) || defined(__GNUC__)
    #pragma GCC diagnostic pop
#endif
