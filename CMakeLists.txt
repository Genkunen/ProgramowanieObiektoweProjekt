cmake_minimum_required(VERSION 4.0)

option(ENABLE_TESTS "Enable tests" ON)
if (${ENABLE_TESTS})
    enable_testing()
endif()

include(FetchContent)
project(ProgramowanieObiektoweProjekt VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fdiagnostics-color=always)
endif()

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
        SOURCE_SUBDIR cmake_unofficial
        SYSTEM
)
FetchContent_MakeAvailable(xxhash)

FetchContent_Declare(
        ktx
        GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software.git
        GIT_TAG        v4.4.2
        SYSTEM
)
set(BUILD_SHARED_LIBS off CACHE BOOL "" FORCE)
set(KTX_FEATURE_TESTS off CACHE BOOL "" FORCE)
set(KTX_FEATURE_TOOLS off CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(ktx)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.7
    SYSTEM
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

FetchContent_Declare(
    tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG        v3.0.0
    SYSTEM
)
set(TINYGLTF_BUILD_LOADER_EXAMPLES OFF CACHE BOOL "" FORCE)
set(TINYGLTF_HEADER_ONLY ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(tinygltf)

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
            -O3
            -g
    )

    foreach(SHADER ${SLANG_SOURCES})
        get_filename_component(NAME_WE ${SHADER} NAME_WE)
        set(OUTPUT_BINARY ${SLANG_TARGET_DIR}/${NAME_WE}.spv)
        set(OUTPUT_DEPFILE ${OUTPUT_BINARY}.d)
        set(CMD
                ${SLANGC_EXECUTABLE}
                ${SHADER}
                ${SLANGC_FLAGS}
                -o
                ${OUTPUT_BINARY}
                -depfile
                ${OUTPUT_DEPFILE}
        )

        add_custom_command(
                OUTPUT ${OUTPUT_BINARY}
                COMMAND ${CMD}
                DEPFILE ${OUTPUT_DEPFILE}
                DEPENDS ${SHADER}
                COMMENT "Compiling Slang shader ${SHADER} (${OUTPUT_BINARY})"
                VERBATIM
        )

        list(APPEND OUTPUT_FILES ${OUTPUT_BINARY})
    endforeach()

    add_custom_target(${TARGET} DEPENDS ${OUTPUT_FILES})
endfunction()

# ---- Sources Setup ---------------------------------------------------------------------------------------------------

file(GLOB_RECURSE LIBRARY_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp)
list(FILTER LIBRARY_SOURCES EXCLUDE REGEX ".*/(bin|tests)/.*")

file(GLOB_RECURSE SHADER_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/shaders/bin/*.slang)

# ---- Build Setup -----------------------------------------------------------------------------------------------------

add_library(ProgramowanieObiektoweProjekt_lib STATIC ${LIBRARY_SOURCES})
add_slang_shaders(ProgramowanieObiektoweProjekt_spv SOURCES ${SHADER_SOURCES} TARGET_DIR ${CMAKE_CURRENT_BINARY_DIR}/spirv)
add_dependencies(ProgramowanieObiektoweProjekt_lib ProgramowanieObiektoweProjekt_spv)

add_custom_command(TARGET ProgramowanieObiektoweProjekt_lib POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/persistent.settings"
    "$<TARGET_FILE_DIR:ProgramowanieObiektoweProjekt_lib>/persistent.settings"
)

target_include_directories(ProgramowanieObiektoweProjekt_lib PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(
        ProgramowanieObiektoweProjekt_lib PUBLIC
        Vulkan::Headers
        SDL3::SDL3
        ktx
        VulkanMemoryAllocator-Hpp::VulkanMemoryAllocator-Hpp
        xxhash
        imgui
        glm::glm
        tinygltf
)

target_compile_definitions(
        ProgramowanieObiektoweProjekt_lib PUBLIC
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_FORCE_LEFT_HANDED
        GLM_FORCE_RADIANS
)

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_options(
        ProgramowanieObiektoweProjekt_lib PRIVATE
        /W4
        /we4716 # Odpowiednik MSVC dla -Werror=return-type
    )
else()
    target_compile_options(
        ProgramowanieObiektoweProjekt_lib PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror=return-type
    )
endif()

# ---- Assets Copy -----------------------------------------------------------------------------------------------------

add_custom_command(TARGET ProgramowanieObiektoweProjekt_lib POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/Assets
        ${CMAKE_CURRENT_BINARY_DIR}/Assets
)

# ---- Executables -----------------------------------------------------------------------------------------------------

function(add_bin name)
    add_executable(${name} ${CMAKE_SOURCE_DIR}/src/bin/${name}.cpp)
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(${name} PRIVATE ProgramowanieObiektoweProjekt_lib)
endfunction()

add_bin(main)

# ---- Tests -----------------------------------------------------------------------------------------------------------

function(add_project_test name)
    add_executable(test_${name} src/tests/${name}.cpp)
    target_include_directories(test_${name} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_${name} PRIVATE ProgramowanieObiektoweProjekt_lib)
    add_test(NAME ${name} COMMAND test_${name})
endfunction()

add_project_test(sanity)
add_project_test(pass_compile)
add_project_test(radix_sort_correctness)

# ---- macOS Support ---------------------------------------------------------------------------------------------------

if(APPLE)
    set(USED_MVK_VERSION "1.4.1")

    FetchContent_Declare(
            moltenvk
            URL https://github.com/KhronosGroup/MoltenVK/releases/download/v${USED_MVK_VERSION}/MoltenVK-macos.tar
    )
    FetchContent_MakeAvailable(moltenvk)

    set(MVK_DYLIB_PATH      ${moltenvk_SOURCE_DIR}/MoltenVK/dynamic/dylib/macOS/libMoltenVK.dylib)
    set(MVK_VULKAN_ICD_PATH ${moltenvk_SOURCE_DIR}/MoltenVK/dynamic/dylib/macOS/MoltenVK_icd.json)

    add_custom_command(TARGET ProgramowanieObiektoweProjekt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${MVK_DYLIB_PATH} ${CMAKE_CURRENT_BINARY_DIR}/libMoltenVK.dylib
    )

    add_custom_command(TARGET ProgramowanieObiektoweProjekt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${MVK_VULKAN_ICD_PATH} ${CMAKE_CURRENT_BINARY_DIR}/MoltenVK_icd.json
    )

    set_target_properties(ProgramowanieObiektoweProjekt PROPERTIES
        BUILD_RPATH "@executable_path"
        INSTALL_RPATH "@executable_path"
    )
endif()

