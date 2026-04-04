cmake_minimum_required(VERSION 3.16)

project(tatr
    VERSION 0.1.0
    DESCRIPTION "Tiny terminal issue tracker"
    LANGUAGES C
)

set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(TATR_ENABLE_WARNINGS "Enable strict warnings" ON)
option(TATR_ENABLE_ASAN     "Enable AddressSanitizer" OFF)
option(TATR_ENABLE_UBSAN    "Enable UBSan" OFF)

set(TATR_WARNINGS
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wconversion
    -Wstrict-prototypes
    -Werror=implicit-function-declaration
)

set(TATR_SANITIZERS "")

if(TATR_ENABLE_ASAN)
    list(APPEND TATR_SANITIZERS -fsanitize=address)
endif()

if(TATR_ENABLE_UBSAN)
    list(APPEND TATR_SANITIZERS -fsanitize=undefined)
endif()

set(TATR_SRC
    src/main.c
    src/issue.c
    src/ui.c
)

set(TATR_CMD_SRC
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
)

set(TATR_LIB_SRC
    libs/fs.c
    libs/astring.c
    libs/temp.c
)

add_library(tatrlib
    ${TATR_LIB_SRC}
)

target_include_directories(tatrlib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/libs
)

if(TATR_ENABLE_WARNINGS)
    target_compile_options(tatrlib PRIVATE ${TATR_WARNINGS})
endif()

target_compile_options(tatrlib PRIVATE ${TATR_SANITIZERS})
target_link_options(tatrlib PRIVATE ${TATR_SANITIZERS})

add_executable(tatr
    ${TATR_SRC}
    ${TATR_CMD_SRC}
)

target_include_directories(tatr
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/cmd
)

target_link_libraries(tatr
    PRIVATE
        tatrlib
)

if(TATR_ENABLE_WARNINGS)
    target_compile_options(tatr PRIVATE ${TATR_WARNINGS})
endif()

target_compile_options(tatr PRIVATE ${TATR_SANITIZERS})
target_link_options(tatr PRIVATE ${TATR_SANITIZERS})

# Debug
target_compile_options(tatr PRIVATE
    $<$<CONFIG:Debug>:-O0 -ggdb>
)

# Release
target_compile_options(tatr PRIVATE
    $<$<CONFIG:Release>:-O2 -DNDEBUG>
)

include(GNUInstallDirs)

install(TARGETS tatr
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Warnings:   ${TATR_ENABLE_WARNINGS}")
message(STATUS "ASAN:       ${TATR_ENABLE_ASAN}")
message(STATUS "UBSAN:      ${TATR_ENABLE_UBSAN}")
