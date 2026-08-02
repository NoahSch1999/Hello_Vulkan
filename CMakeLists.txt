cmake_minimum_required(VERSION 3.20 FATAL_ERROR)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
set(NAME HowToVulkan)
project(${NAME})

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include(FetchContent)

# Vulkan
FetchContent_Declare(Vulkan_Headers
    GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
    GIT_TAG "vulkan-sdk-1.4.350.0"
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(Vulkan_Headers)
#

# Volk
FetchContent_Declare(Volk
    GIT_REPOSITORY https://github.com/zeux/volk.git
    GIT_TAG "1.4.350"
    GIT_SHALLOW TRUE
)
set(VULKAN_HEADERS_INSTALL_DIR "${vulkan_headers_SOURCE_DIR}" CACHE PATH "Where to get the Vulkan headers" FORCE)
FetchContent_MakeAvailable(Volk)
#

# VMA
FetchContent_Declare(Vma
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG "v3.4.0"
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(Vma)
#

# Spir-v Reflection
FetchContent_Declare(SpirvReflection
    GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Reflect.git
    GIT_TAG "vulkan-sdk-1.4.350.0"
    GIT_SHALLOW TRUE
)
set(SPIRV_REFLECT_EXECUTABLE OFF CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_STATIC_LIB ON CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SpirvReflection)
#

# GLM
FetchContent_Declare(GLM
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG "1.0.3"
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(GLM)
#

# KTX 
FetchContent_Declare(
    ktx
    GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software.git
    GIT_TAG "v4.4.2"
    #SOURCE_SUBDIR "lib"
    GIT_SHALLOW TRUE)
set(KTX_FEATURE_TOOLS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_LOADTEST_APPS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_GL_UPLOAD OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_VK_UPLOAD OFF CACHE BOOL "" FORCE)

# https://github.com/KhronosGroup/KTX-Software/blob/main/lib/include/ktx.h 
# https://github.com/KhronosGroup/KTX-Software/blob/main/examples/vkload.cpp 
FetchContent_MakeAvailable(ktx)
#

# FastGltf
FetchContent_Declare(
    fastgltf
    GIT_REPOSITORY https://github.com/spnda/fastgltf.git
    GIT_TAG        "v0.9.0"
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(fastgltf)
#

# Meshoptimizer
FetchContent_Declare(
    meshoptimizer
    GIT_REPOSITORY https://github.com/zeux/meshoptimizer.git
    GIT_TAG        "v1.2"
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(meshoptimizer)
#

# Slang
FetchContent_Declare(
    slang
    GIT_REPOSITORY https://github.com/shader-slang/slang.git
    #GIT_TAG "vulkan-sdk-1.4.350.0"
    GIT_TAG "v2026.13"
    GIT_SHALLOW TRUE
)

set(SLANG_ENABLE_SLANGC ON CACHE BOOL "" FORCE)
set(SLANG_ENABLE_SLANGD OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_SLANGI OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_OPTIX OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_CUDA OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_DXIL OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_COVERAGE OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_TIME_TRACE OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_SLANG_GLSLANG OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_SLANG_PROXY OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_REPLAYER OFF CACHE BOOL "" FORCE)
set(SLANG_EXCLUDE_DAWN ON CACHE BOOL "" FORCE)
set(SLANG_LIB_TYPE STATIC CACHE STRING "" FORCE)

set(SLANG_ENABLE_MINIZ ON CACHE BOOL "" FORCE)
set(SLANG_USE_SYSTEM_MINIZ OFF CACHE BOOL "" FORCE)
set(SLANG_STATIC ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(slang)
#

# aZeroWindow
FetchContent_Declare(
    aZeroWindow
    GIT_REPOSITORY https://github.com/NoahSch1999/aZero-Window.git
    GIT_TAG "main"
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(aZeroWindow)
#

add_executable(${NAME} "main.cpp" "Loading_Assets/STBIHack.cpp")

target_compile_definitions(${NAME} PUBLIC
    VK_NO_PROTOTYPES
    VK_USE_PLATFORM_WIN32_KHR
    NOMINMAX
    _USE_MATH_DEFINES
    _CRT_SECURE_NO_WARNINGS
)

target_link_libraries(${NAME} 
    PUBLIC
    Vulkan::Headers
    volk::volk
    GPUOpen::VulkanMemoryAllocator
    ktx
    fastgltf
    aZeroWindow
    glm::glm
    meshoptimizer
    slang
    spirv-reflect-static
)