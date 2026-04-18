# ---- Test executable ----

set(TEST_SOURCES
    main.c
    test_init_new.c
    test_list_show_edit.c
    test_lifecycle_org.c
    test_attachments.c
    test_status_meta.c
    test_export.c
)

add_executable(run_tests
    ${TEST_SOURCES}
)

# ---- Includes ----

target_include_directories(run_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/libs
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(run_tests PRIVATE tatrlib)

# ---- Define TATR_BIN ----

target_compile_definitions(run_tests
    PRIVATE
        TATR_BIN="$<TARGET_FILE:tatr>"
)

# ---- Warnings ----

if(TATR_ENABLE_WARNINGS)
    if(MSVC)
        target_compile_options(run_tests PRIVATE /W4)
    else()
        target_compile_options(run_tests PRIVATE
            -Wall -Wextra -Wshadow -Wconversion -Wstrict-prototypes
        )
    endif()
endif()
