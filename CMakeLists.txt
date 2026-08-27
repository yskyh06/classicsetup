cmake_minimum_required(VERSION 3.16)

project(classicsetup LANGUAGES C)

find_package(Curses REQUIRED)

add_executable(classicsetup
    src/app.c
    src/config.c
    src/core/apply.c
    src/core/disk.c
    src/core/environment.c
    src/core/format.c
    src/core/format_apply.c
    src/core/install_mode.c
    src/core/partition.c
    src/core/partition_plan.c
    src/core/process.c
    src/core/recommended.c
    src/core/setup_mode.c
    src/core/system_disk.c
    src/main.c
    src/state.c
    src/tui/after_format.c
    src/tui/apply.c
    src/tui/disk_selection.c
    src/tui/format_selection.c
    src/tui/format_apply.c
    src/tui/install_mode_selection.c
    src/tui/keymap.c
    src/tui/keyboard.c
    src/tui/partition_selection.c
    src/tui/quit.c
    src/tui/recommended.c
    src/tui/setup_mode_selection.c
    src/tui/tui.c
    src/tui/welcome.c
)

set_target_properties(classicsetup PROPERTIES
    C_STANDARD 17
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
)

target_include_directories(classicsetup PRIVATE
    include
    ${CURSES_INCLUDE_DIRS}
)

target_link_libraries(classicsetup PRIVATE ${CURSES_LIBRARIES})

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(classicsetup PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)

if(BUILD_TESTING)
    add_executable(classicsetup_state_test
        tests/state_test.c
        src/state.c
    )

    set_target_properties(classicsetup_state_test PROPERTIES
        C_STANDARD 17
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO
    )

    target_include_directories(classicsetup_state_test PRIVATE include)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(classicsetup_state_test PRIVATE
            -Wall -Wextra -Wpedantic
        )
    endif()
    add_test(NAME state_transitions COMMAND classicsetup_state_test)

    add_executable(classicsetup_disk_test
        tests/disk_test.c
        src/core/disk.c
    )

    set_target_properties(classicsetup_disk_test PROPERTIES
        C_STANDARD 17
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO
    )

    target_include_directories(classicsetup_disk_test PRIVATE include)
    target_compile_definitions(classicsetup_disk_test PRIVATE
        CLASSICSETUP_TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
    )

    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(classicsetup_disk_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME disk_scan COMMAND classicsetup_disk_test)

    add_executable(classicsetup_partition_test
        tests/partition_test.c
        src/core/partition.c
    )

    set_target_properties(classicsetup_partition_test PROPERTIES
        C_STANDARD 17
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO
    )

    target_include_directories(classicsetup_partition_test PRIVATE include)
    target_compile_definitions(classicsetup_partition_test PRIVATE
        CLASSICSETUP_TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
    )

    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(classicsetup_partition_test PRIVATE
            -Wall -Wextra -Wpedantic
        )
    endif()

    add_test(NAME partition_scan COMMAND classicsetup_partition_test)

    add_executable(classicsetup_partition_plan_test
        tests/partition_plan_test.c
        src/config.c
        src/core/format.c
        src/core/install_mode.c
        src/core/partition_plan.c
    )

    set_target_properties(classicsetup_partition_plan_test PROPERTIES
        C_STANDARD 17
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO
    )

    target_include_directories(classicsetup_partition_plan_test PRIVATE include)

    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(classicsetup_partition_plan_test PRIVATE
            -Wall -Wextra -Wpedantic
        )
    endif()

    add_test(NAME partition_plan COMMAND classicsetup_partition_plan_test)

    add_executable(classicsetup_format_test
        tests/format_test.c
        src/config.c
        src/core/format.c
        src/core/partition_plan.c
    )

    set_target_properties(classicsetup_format_test PROPERTIES
        C_STANDARD 17
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO
    )

    target_include_directories(classicsetup_format_test PRIVATE include)

    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(classicsetup_format_test PRIVATE
            -Wall -Wextra -Wpedantic
        )
    endif()

    add_test(NAME format_plan COMMAND classicsetup_format_test)

    add_executable(classicsetup_apply_test
        tests/apply_test.c
        src/core/apply.c
        src/core/disk.c
        src/core/environment.c
        src/core/partition.c
        src/core/partition_plan.c
        src/core/process.c
        src/core/system_disk.c
    )

    set_target_properties(classicsetup_apply_test PROPERTIES
        C_STANDARD 17
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO
    )

    target_include_directories(classicsetup_apply_test PRIVATE include)
    target_compile_definitions(classicsetup_apply_test PRIVATE
        CLASSICSETUP_TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
    )

    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(classicsetup_apply_test PRIVATE
            -Wall -Wextra -Wpedantic
        )
    endif()

    add_test(NAME apply_safety_and_render COMMAND classicsetup_apply_test)

    add_executable(classicsetup_format_apply_test
        tests/format_apply_test.c
        src/core/apply.c
        src/core/disk.c
        src/core/environment.c
        src/core/format.c
        src/core/format_apply.c
        src/core/partition.c
        src/core/partition_plan.c
        src/core/process.c
        src/core/system_disk.c
    )

    set_target_properties(classicsetup_format_apply_test PROPERTIES
        C_STANDARD 17
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO
    )

    target_include_directories(classicsetup_format_apply_test PRIVATE include)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(classicsetup_format_apply_test PRIVATE
            -Wall -Wextra -Wpedantic
        )
    endif()

    add_test(NAME format_apply COMMAND classicsetup_format_apply_test)

    add_executable(classicsetup_recommended_test
        tests/recommended_test.c
        src/config.c
        src/core/apply.c
        src/core/disk.c
        src/core/environment.c
        src/core/format.c
        src/core/format_apply.c
        src/core/partition.c
        src/core/partition_plan.c
        src/core/process.c
        src/core/recommended.c
        src/core/setup_mode.c
        src/core/system_disk.c
    )

    set_target_properties(classicsetup_recommended_test PROPERTIES
        C_STANDARD 17
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO
    )

    target_include_directories(classicsetup_recommended_test PRIVATE include)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(classicsetup_recommended_test PRIVATE
            -Wall -Wextra -Wpedantic
        )
    endif()

    add_test(NAME recommended_policy COMMAND classicsetup_recommended_test)

    add_executable(classicsetup_key_mapping_test
        tests/key_mapping_test.c
        src/tui/keymap.c
    )

    set_target_properties(classicsetup_key_mapping_test PROPERTIES
        C_STANDARD 17
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO
    )

    target_include_directories(classicsetup_key_mapping_test PRIVATE
        include
        ${CURSES_INCLUDE_DIRS}
    )

    target_link_libraries(classicsetup_key_mapping_test PRIVATE
        ${CURSES_LIBRARIES}
    )

    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(classicsetup_key_mapping_test PRIVATE
            -Wall -Wextra -Wpedantic
        )
    endif()

    add_test(NAME key_mapping COMMAND classicsetup_key_mapping_test)
    add_test(
        NAME tui_no_function_keys
        COMMAND ${CMAKE_COMMAND}
            -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_no_function_keys.cmake
    )
endif()
