cmake_minimum_required(VERSION 3.16)

project(tatr
    VERSION 0.1.0
    DESCRIPTION "Tiny terminal issue tracker"
    LANGUAGES C
)

# ---- Global settings ----

set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- Options ----

option(TATR_ENABLE_WARNINGS "Enable strict warnings" ON)
option(TATR_ENABLE_ASAN     "Enable AddressSanitizer" OFF)
option(TATR_ENABLE_UBSAN    "Enable UBSan" OFF)

# ---- Sources ----

set(TATR_SRC
    src/main.c
    src/issue.c
    src/ui.c
    src/editor.c
    src/tatrlog.c
)

file(GLOB TATR_EXPORT_SRC
    src/cmd/export/*.c
)

set(TATR_CMD_SRC
    src/cmd/basic.c
    src/cmd/init.c
    src/cmd/new.c
    src/cmd/list.c
    src/cmd/show.c
    src/cmd/edit.c
    src/cmd/close.c
    src/cmd/delete.c
    src/cmd/comment.c
    src/cmd/search.c
    src/cmd/reopen.c
    src/cmd/attach.c
    src/cmd/attachls.c
    src/cmd/detach.c
    src/cmd/tag.c
    src/cmd/status.c
    src/cmd/log.c
    ${TATR_EXPORT_SRC}
)

set(TATR_LIB_SRC
    libs/fs.c
    libs/astring.c
    libs/temp.c
)

# ---- Library ----

add_library(tatrlib ${TATR_LIB_SRC})

target_include_directories(tatrlib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/libs
)

# ---- Executable ----

add_executable(tatr
    ${TATR_SRC}
    ${TATR_CMD_SRC}
)

target_include_directories(tatr
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/cmd
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
    target_link_options(tatr PRIVATE ${TATR_SANITIZERS})
endif()

# ---- Build type flags ----

target_compile_options(tatr PRIVATE
    $<$<CONFIG:Debug>:-O0 -g>
    $<$<CONFIG:Release>:-O3 -DNDEBUG>
)

# ---- Platform-specific config ----

# Windows: link bcrypt for BCryptGenRandom
if(WIN32)
    target_link_libraries(tatrlib PUBLIC bcrypt)
endif()

# Linux: enable getrandom()
if(UNIX AND NOT APPLE)
    target_compile_definitions(tatrlib PRIVATE _GNU_SOURCE)
endif()

# ---- Installation ----

include(GNUInstallDirs)

install(TARGETS tatr
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# ---- Status output ----

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Warnings:   ${TATR_ENABLE_WARNINGS}")
message(STATUS "ASAN:       ${TATR_ENABLE_ASAN}")
message(STATUS "UBSAN:      ${TATR_ENABLE_UBSAN}")
message(STATUS "System:     ${CMAKE_SYSTEM_NAME}")
message(STATUS "Compiler:   ${CMAKE_C_COMPILER_ID}")

enable_testing()
add_subdirectory(tests)
