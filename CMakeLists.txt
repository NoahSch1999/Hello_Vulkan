cmake_minimum_required(VERSION 3.20 FATAL_ERROR)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
include(FetchContent)
set(NAME HowToVulkan)
project(${NAME})

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include(FetchContent)

# Vulkan
FetchContent_Declare(VulkanHeaders
    GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
    GIT_TAG "vulkan-sdk-1.4.350"
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(VulkanHeaders)
#

# Volk
FetchContent_Declare(Volk
    GIT_REPOSITORY https://github.com/zeux/volk.git
    GIT_TAG "vulkan-sdk-1.4.350"
    GIT_SHALLOW TRUE
)
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

target_compile_definitions(${NAME} PRIVATE
    VK_NO_PROTOTYPES
    VK_USE_PLATFORM_WIN32_KHR
    NOMINMAX
    _USE_MATH_DEFINES
    _CRT_SECURE_NO_WARNINGS
)

target_link_libraries(${NAME} 
    PUBLIC
    volk::volk
    GPUOpen::VulkanMemoryAllocator
    ktx
    fastgltf
    aZeroWindow
    glm::glm
    meshoptimizer
    slang
)