cmake_minimum_required(VERSION 4.0)
include(FetchContent)
project(ProgramowanieObiektoweProjekt VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- C/C++ Settings --------------------------------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Dependencies ----------------------------------------------------------------------------------------------------

find_package(Vulkan REQUIRED)
find_package(SDL3 REQUIRED)

FetchContent_Declare(
        VMAHpp
        URL      https://github.com/YaaZ/VulkanMemoryAllocator-Hpp/releases/download/v3.3.0+3/VulkanMemoryAllocator-Hpp-3.3.0.tar.gz
        URL_HASH SHA256=8c2a0573babe1f86f3241d12b4c9df40bb944649b7f4239c1c611797f8af6cd5
        SYSTEM
)
FetchContent_MakeAvailable(VMAHpp)

FetchContent_Declare(
        xxhash
        GIT_REPOSITORY https://github.com/Cyan4973/xxHash.git
        GIT_TAG v0.8.3
        SYSTEM
)
FetchContent_MakeAvailable(xxhash)

FetchContent_Declare(
        ktx
        GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software.git
        GIT_TAG        v4.4.2
        SYSTEM
)

set(KTX_FEATURE_STATIC_LIBRARY ON CACHE BOOL "" FORCE)
set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_TOOLS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(ktx)

# ---- Slang Setup -----------------------------------------------------------------------------------------------------

# TODO: command to compile shaders..

# ---- Sources Setup ---------------------------------------------------------------------------------------------------

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp)

# ---- Build Setup -----------------------------------------------------------------------------------------------------

add_executable(ProgramowanieObiektoweProjekt ${SOURCES})
target_include_directories(ProgramowanieObiektoweProjekt PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(
        ProgramowanieObiektoweProjekt PRIVATE
        Vulkan::Headers
        SDL3::SDL3
        ktx
        VulkanMemoryAllocator-Hpp::VulkanMemoryAllocator-Hpp
        xxhash
)


