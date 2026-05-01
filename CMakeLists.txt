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
find_package(glm REQUIRED)

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

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.7
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

target_include_directories(imgui PUBLIC 
    ${imgui_SOURCE_DIR} 
    ${imgui_SOURCE_DIR}/backends
)

target_compile_definitions(imgui PRIVATE IMGUI_IMPL_VULKAN_NO_PROTOTYPES)
target_link_libraries(imgui PRIVATE SDL3::SDL3 Vulkan::Headers)

# ---- Slang Setup -----------------------------------------------------------------------------------------------------

find_program(SLANGC_EXECUTABLE slangc
        HINTS $ENV{VULKAN_SDK}/bin
        REQUIRED
)

message(STATUS "Found slangc: ${SLANGC_EXECUTABLE}")

function(add_slang_shaders TARGET)
    cmake_parse_arguments(SLANG "" "TARGET_DIR" "SOURCES" ${ARGN})

    if(NOT SLANG_TARGET_DIR)
        set(SLANG_TARGET_DIR ${CMAKE_CURRENT_BINARY_DIR}/spirv)
    endif()

    file(MAKE_DIRECTORY ${SLANG_TARGET_DIR})
    set(OUTPUT_FILES "")

    set(SLANGC_FLAGS
            -target spirv
            -profile spirv_1_6
            -fvk-use-scalar-layout
    )

    foreach(SHADER ${SLANG_SOURCES})
        get_filename_component(NAME_WE ${SHADER} NAME_WE)
        set(OUTPUT_BINARY ${SLANG_TARGET_DIR}/${NAME_WE}.spv)
        set(CMD
                ${SLANGC_EXECUTABLE}
                ${SHADER}
                ${SLANGC_FLAGS}
                -o
                ${OUTPUT_BINARY}
        )

        add_custom_command(
                OUTPUT ${OUTPUT_BINARY}
                COMMAND ${CMD}
                DEPENDS ${SHADER}
                COMMENT "Compiling Slang shader ${SHADER} (${OUTPUT_BINARY})"
                VERBATIM
        )

        list(APPEND OUTPUT_FILES ${OUTPUT_BINARY})
    endforeach()

    add_custom_target(${TARGET} DEPENDS ${OUTPUT_FILES})
endfunction()

# ---- Sources Setup ---------------------------------------------------------------------------------------------------

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp)
file(GLOB_RECURSE SHADER_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/shaders/bin/*.slang)

# ---- Build Setup -----------------------------------------------------------------------------------------------------

add_executable(ProgramowanieObiektoweProjekt ${SOURCES})
add_slang_shaders(ProgramowanieObiektoweProjekt_spv SOURCES ${SHADER_SOURCES} TARGET_DIR ${CMAKE_CURRENT_BINARY_DIR}/spirv)
add_dependencies(ProgramowanieObiektoweProjekt ProgramowanieObiektoweProjekt_spv)

add_custom_command(TARGET ProgramowanieObiektoweProjekt POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/persistent.settings"
    "$<TARGET_FILE_DIR:ProgramowanieObiektoweProjekt>/persistent.settings"
)

target_include_directories(ProgramowanieObiektoweProjekt PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(
        ProgramowanieObiektoweProjekt PRIVATE
        Vulkan::Headers
        SDL3::SDL3
        ktx
        VulkanMemoryAllocator-Hpp::VulkanMemoryAllocator-Hpp
        xxhash
        imgui
        glm::glm
)

target_compile_definitions(
        ProgramowanieObiektoweProjekt PRIVATE
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_FORCE_LEFT_HANDED
        GLM_FORCE_RADIANS
)

target_compile_options(
        ProgramowanieObiektoweProjekt PRIVATE
        -Werror=return-type
)

