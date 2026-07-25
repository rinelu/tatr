# Unit tests

add_executable(run_unit_tests
    test_libtatr.c
)

target_link_libraries(run_unit_tests PRIVATE tatrlib)

if(TATR_ENABLE_WARNINGS AND NOT MSVC)
    target_compile_options(run_unit_tests PRIVATE
        -Wall -Wextra -Wshadow -Wconversion -Wstrict-prototypes
    )
endif()

add_test(NAME tatr_unit_tests
    COMMAND $<TARGET_FILE:run_unit_tests>
)
