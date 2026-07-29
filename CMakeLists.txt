cmake_minimum_required(VERSION 3.21)

project(tatr
    VERSION 3.0.0
    DESCRIPTION "Tiny terminal issue tracker"
    LANGUAGES C
)

# ---- Global settings ----

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- Options ----

option(TATR_ENABLE_WARNINGS "Enable strict warnings" ON)
option(TATR_ENABLE_ASAN     "Enable AddressSanitizer" OFF)
option(TATR_ENABLE_UBSAN    "Enable UBSan" OFF)

# ---- Module options ----
# To add a new module (e.g. mcp, git_hook), see src/modules/README.md.

option(TATR_MODULE_EXPORT "Enable the export module (Markdown/JSON issue export)" ON)

# ---- Sources ----

function(tatr_read_sources out_var manifest)
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/sources/${manifest}.txt" _lines)
    set(_result "")
    foreach(_line ${_lines})
        string(REGEX REPLACE "#.*" "" _line "${_line}")
        string(STRIP "${_line}" _line)
        if(_line)
            list(APPEND _result "${_line}")
        endif()
    endforeach()
    set(${out_var} ${_result} PARENT_SCOPE)
endfunction()

tatr_read_sources(TATR_MAIN_SRC       main)
tatr_read_sources(TATR_CORE_SRC       core)
tatr_read_sources(TATR_API_SRC        api)
tatr_read_sources(TATR_STORAGE_SRC    storage)
tatr_read_sources(TATR_RENDER_SRC     render)
tatr_read_sources(TATR_COMMANDS_SRC   commands)
tatr_read_sources(TATR_THIRDPARTY_SRC thirdparty)
tatr_read_sources(TATR_MODULES_CORE_SRC modules)

if(TATR_MODULE_EXPORT)
    list(APPEND TATR_COMMANDS_SRC src/commands/export.c)
endif()

set(TATR_MODULE_SRC "${TATR_MODULES_CORE_SRC}")
set(TATR_MODULE_INCLUDE_DIRS "")
set(TATR_MODULE_DEFS "")

if(TATR_MODULE_EXPORT)
    file(GLOB TATR_EXPORT_SRC RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/modules/export/*.c)
    list(APPEND TATR_MODULE_SRC ${TATR_EXPORT_SRC})
    list(APPEND TATR_MODULE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/export)
    list(APPEND TATR_MODULE_DEFS TATR_MODULE_EXPORT)
endif()

# ---- Library ----
#
# tatrlib bundles src/core/ + src/modules/ + src/thirdparty/ so both the executable
# and the test binary link against a single, consistent build of everything below the CLI entry point.

add_library(tatrlib
    ${TATR_CORE_SRC}
    ${TATR_API_SRC}
    ${TATR_STORAGE_SRC}
    ${TATR_RENDER_SRC}
    ${TATR_MODULE_SRC}
    ${TATR_THIRDPARTY_SRC}
)

target_include_directories(tatrlib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core
        ${CMAKE_CURRENT_SOURCE_DIR}/src/thirdparty
        ${CMAKE_CURRENT_SOURCE_DIR}/src/api
        ${CMAKE_CURRENT_SOURCE_DIR}/src/storage
        ${CMAKE_CURRENT_SOURCE_DIR}/src/render
        ${TATR_MODULE_INCLUDE_DIRS}
)

target_compile_definitions(tatrlib PUBLIC ${TATR_MODULE_DEFS})

# ---- Executable ----

add_executable(tatr
    ${TATR_MAIN_SRC}
    ${TATR_COMMANDS_SRC}
)

target_include_directories(tatr
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/commands
)

target_link_libraries(tatr PRIVATE tatrlib)

# ---- Compiler warnings ----

if(TATR_ENABLE_WARNINGS)
    if(MSVC)
        target_compile_options(tatrlib PRIVATE /W4)
        target_compile_options(tatr    PRIVATE /W4)
    else()
        set(TATR_WARNINGS
            -Wall
            -Wextra
            -Wshadow
            -Wconversion
            -Wstrict-prototypes
            -Werror=implicit-function-declaration
        )

        target_compile_options(tatrlib PRIVATE ${TATR_WARNINGS})
        target_compile_options(tatr    PRIVATE ${TATR_WARNINGS})
    endif()
endif()

if(MSVC)
    target_compile_definitions(tatr      PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
    target_compile_definitions(tatrlib   PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
endif()

# ---- Sanitizers (GCC/Clang only) ----

set(TATR_SANITIZERS "")

if(TATR_ENABLE_ASAN)
    list(APPEND TATR_SANITIZERS -fsanitize=address)
endif()

if(TATR_ENABLE_UBSAN)
    list(APPEND TATR_SANITIZERS -fsanitize=undefined)
endif()

if(TATR_SANITIZERS AND NOT MSVC)
    target_compile_options(tatr PRIVATE ${TATR_SANITIZERS})
    target_link_options(tatr    PRIVATE ${TATR_SANITIZERS})
endif()

# ---- Build type flags ----

if(MSVC)
    target_compile_options(tatr PRIVATE
        $<$<CONFIG:Debug>:/Od>
        $<$<CONFIG:Release>:/O2 /DNDEBUG>
    )
else()
    target_compile_options(tatr PRIVATE
        $<$<CONFIG:Debug>:-O0 -g>
        $<$<CONFIG:Release>:-O2 -DNDEBUG>
    )
endif()

# ---- Platform-specific config ----

if(WIN32)
    target_link_libraries(tatrlib PUBLIC bcrypt)
endif()

if(UNIX AND NOT APPLE)
    target_compile_definitions(tatrlib PRIVATE _GNU_SOURCE)
endif()

# ---- Installation ----

include(GNUInstallDirs)

install(TARGETS tatr
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# ---- Testing ----
enable_testing()

# ---- Status output ----

message(STATUS "Build type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "Warnings:       ${TATR_ENABLE_WARNINGS}")
message(STATUS "ASAN:           ${TATR_ENABLE_ASAN}")
message(STATUS "UBSAN:          ${TATR_ENABLE_UBSAN}")
message(STATUS "Module export:  ${TATR_MODULE_EXPORT}")
message(STATUS "System:         ${CMAKE_SYSTEM_NAME}")
message(STATUS "Compiler:       ${CMAKE_C_COMPILER_ID}")

add_subdirectory(tests)

# ---- Build-drift check ----
#
# Writes the fully-resolved set of sources this CMakeLists.txt will
# compile to a file, one per line, sorted -- the CMake-side counterpart
# is `make print-sources`. See scripts/check-build-drift.sh.
set(TATR_ALL_SRC
    ${TATR_MAIN_SRC} ${TATR_CORE_SRC} ${TATR_API_SRC} ${TATR_STORAGE_SRC}
    ${TATR_RENDER_SRC} ${TATR_COMMANDS_SRC} ${TATR_MODULE_SRC} ${TATR_THIRDPARTY_SRC}
)
list(SORT TATR_ALL_SRC)
list(REMOVE_DUPLICATES TATR_ALL_SRC)

set(TATR_SOURCES_LIST_FILE "${CMAKE_BINARY_DIR}/cmake-sources.txt")
string(REPLACE ";" "\n" TATR_ALL_SRC_LINES "${TATR_ALL_SRC}")
file(WRITE "${TATR_SOURCES_LIST_FILE}" "${TATR_ALL_SRC_LINES}\n")
