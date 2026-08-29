cmake_minimum_required(VERSION 3.16)

project(classicsetup LANGUAGES C)

find_package(Curses REQUIRED)
find_package(PkgConfig QUIET)

option(CLASSICSETUP_ENABLE_GTK
    "Build the optional GTK4 Recommended frontend when GTK4 is available"
    ON)
option(CLASSICSETUP_ENABLE_DOWNLOAD
    "Build official Windows source discovery/download support"
    ON)
option(CLASSICSETUP_ENABLE_UUP
    "Build the pinned Microsoft UUP acquisition backend"
    ON)
set(CLASSICSETUP_UUP_TOOL_ROOT
    "/usr/lib/classicsetup/tools/uupmediacreator"
    CACHE PATH "Pinned bundled UUPMediaCreator installation directory")
set(CLASSICSETUP_UUP_CONVERTER_ROOT
    "${CLASSICSETUP_UUP_TOOL_ROOT}"
    CACHE PATH "Validated UUPMediaConverter runtime directory")
set(CLASSICSETUP_UUP_DOTNET_ROOT "" CACHE PATH
    "Optional explicit .NET runtime root for UUP apphosts")
set(CLASSICSETUP_WIMLIB_EXECUTABLE
    "/usr/bin/wimlib-imagex"
    CACHE FILEPATH "wimlib-imagex used to verify generated WIM images")
set(CLASSICSETUP_UUP_ISO_EXTRACTOR
    "/usr/bin/7z"
    CACHE FILEPATH "7-Zip executable used for unprivileged ISO/UDF extraction")
set(CLASSICSETUP_UUP_BUNDLE_DIR "" CACHE PATH
    "Pre-staged, hash-verified UUPMediaCreator bundle for packaging")

set(CLASSICSETUP_GTK_FRONTEND src/gui/gtk_stub.c)
set(CLASSICSETUP_NETWORK_BACKEND src/network/network_manager_stub.c)
set(CLASSICSETUP_GTK_ENABLED OFF)
if(CLASSICSETUP_ENABLE_GTK)
    if(PkgConfig_FOUND)
        pkg_check_modules(GTK4 QUIET IMPORTED_TARGET gtk4)
    endif()
    if(GTK4_FOUND)
        set(CLASSICSETUP_GTK_FRONTEND src/gui/gtk_frontend.c)
        set(CLASSICSETUP_NETWORK_BACKEND
            src/network/network_manager_gdbus.c)
        set(CLASSICSETUP_GTK_ENABLED ON)
        message(STATUS "ClassicSetup GTK4 frontend: enabled")
    else()
        message(STATUS "ClassicSetup GTK4 frontend: disabled (GTK4 not found)")
    endif()
else()
    message(STATUS "ClassicSetup GTK4 frontend: disabled by option")
endif()

set(CLASSICSETUP_SOURCE_BACKEND src/source/microsoft_source_stub.c)
set(CLASSICSETUP_DOWNLOAD_BACKEND src/source/download_stub.c)
set(CLASSICSETUP_DOWNLOAD_ENABLED OFF)
if(CLASSICSETUP_ENABLE_DOWNLOAD AND PkgConfig_FOUND)
    pkg_check_modules(LIBCURL QUIET IMPORTED_TARGET libcurl)
    find_package(OpenSSL QUIET)
    if(LIBCURL_FOUND AND OpenSSL_FOUND)
        set(CLASSICSETUP_SOURCE_BACKEND
            src/source/microsoft_source_curl.c)
        set(CLASSICSETUP_DOWNLOAD_BACKEND src/source/download_curl.c)
        set(CLASSICSETUP_DOWNLOAD_ENABLED ON)
        message(STATUS "ClassicSetup Windows download: enabled")
    else()
        message(STATUS "ClassicSetup Windows download: disabled (libcurl/OpenSSL not found)")
    endif()
else()
    message(STATUS "ClassicSetup Windows download: disabled by option")
endif()

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
    src/gui/gui.c
    src/gui/session.c
    ${CLASSICSETUP_GTK_FRONTEND}
    src/network/network.c
    ${CLASSICSETUP_NETWORK_BACKEND}
    src/source/download_model.c
    ${CLASSICSETUP_DOWNLOAD_BACKEND}
    ${CLASSICSETUP_SOURCE_BACKEND}
    src/source/windows_source.c
    src/source/uup.c
    src/source/workspace.c
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
    src/tui/license_agreement.c
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

target_compile_definitions(classicsetup PRIVATE
    CLASSICSETUP_ENABLE_UUP=$<BOOL:${CLASSICSETUP_ENABLE_UUP}>
    CLASSICSETUP_UUP_TOOL_ROOT="${CLASSICSETUP_UUP_TOOL_ROOT}"
    CLASSICSETUP_UUP_CONVERTER_ROOT="${CLASSICSETUP_UUP_CONVERTER_ROOT}"
    CLASSICSETUP_UUP_DOTNET_ROOT="${CLASSICSETUP_UUP_DOTNET_ROOT}"
    CLASSICSETUP_WIMLIB_EXECUTABLE="${CLASSICSETUP_WIMLIB_EXECUTABLE}"
    CLASSICSETUP_UUP_ISO_EXTRACTOR="${CLASSICSETUP_UUP_ISO_EXTRACTOR}"
)
if(CLASSICSETUP_ENABLE_UUP)
    message(STATUS "ClassicSetup Microsoft UUP backend: enabled (bundled runtime required)")
    install(FILES packaging/uupmediacreator.manifest
        DESTINATION share/classicsetup)
    if(CLASSICSETUP_UUP_BUNDLE_DIR)
        install(DIRECTORY "${CLASSICSETUP_UUP_BUNDLE_DIR}/"
            DESTINATION lib/classicsetup/tools/uupmediacreator
            USE_SOURCE_PERMISSIONS)
    endif()
else()
    message(STATUS "ClassicSetup Microsoft UUP backend: disabled by option")
endif()

if(CLASSICSETUP_DOWNLOAD_ENABLED)
    target_link_libraries(classicsetup PRIVATE
        PkgConfig::LIBCURL
        OpenSSL::Crypto
    )
    target_compile_definitions(classicsetup PRIVATE
        CLASSICSETUP_DOWNLOAD_ENABLED=1)
else()
    target_compile_definitions(classicsetup PRIVATE
        CLASSICSETUP_DOWNLOAD_ENABLED=0)
endif()

if(CLASSICSETUP_GTK_ENABLED)
    target_compile_definitions(classicsetup PRIVATE
        CLASSICSETUP_ENABLE_GTK=1
        CLASSICSETUP_GUI_CSS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/src/gui/classicsetup.css"
    )
    target_link_libraries(classicsetup PRIVATE PkgConfig::GTK4)
else()
    target_compile_definitions(classicsetup PRIVATE CLASSICSETUP_ENABLE_GTK=0)
endif()

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

    add_executable(classicsetup_gui_test
        tests/gui_test.c
        src/gui/gui.c
        src/gui/gtk_stub.c
        src/core/process.c
        src/network/network.c
        src/source/download_model.c
        src/source/windows_source.c
        src/source/uup.c
        src/source/workspace.c
    )

    set_target_properties(classicsetup_gui_test PROPERTIES
        C_STANDARD 17
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO
    )

    target_include_directories(classicsetup_gui_test PRIVATE include)
    target_compile_definitions(classicsetup_gui_test PRIVATE
        CLASSICSETUP_ENABLE_UUP=$<BOOL:${CLASSICSETUP_ENABLE_UUP}>)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(classicsetup_gui_test PRIVATE
            -Wall -Wextra -Wpedantic
        )
    endif()
    add_test(NAME gui_navigation COMMAND classicsetup_gui_test)

    add_executable(classicsetup_source_test
        tests/source_test.c
        src/source/download_model.c
        ${CLASSICSETUP_DOWNLOAD_BACKEND}
        src/source/windows_source.c
        src/source/workspace.c
    )
    set_target_properties(classicsetup_source_test PROPERTIES
        C_STANDARD 17
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO
    )
    target_include_directories(classicsetup_source_test PRIVATE include)
    target_compile_definitions(classicsetup_source_test PRIVATE
        CLASSICSETUP_DOWNLOAD_ENABLED=$<BOOL:${CLASSICSETUP_DOWNLOAD_ENABLED}>)
    if(CLASSICSETUP_DOWNLOAD_ENABLED)
        target_link_libraries(classicsetup_source_test PRIVATE
            PkgConfig::LIBCURL
            OpenSSL::Crypto
        )
    endif()
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(classicsetup_source_test PRIVATE
            -Wall -Wextra -Wpedantic
        )
    endif()
    add_test(NAME windows_source_pipeline COMMAND classicsetup_source_test)

    add_executable(classicsetup_uup_test
        tests/uup_test.c
        src/core/process.c
        src/source/uup.c
        src/source/windows_source.c
        src/source/workspace.c
    )
    set_target_properties(classicsetup_uup_test PROPERTIES
        C_STANDARD 17
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO
    )
    target_include_directories(classicsetup_uup_test PRIVATE include)
    target_compile_definitions(classicsetup_uup_test PRIVATE
        CLASSICSETUP_ENABLE_UUP=$<BOOL:${CLASSICSETUP_ENABLE_UUP}>
        CLASSICSETUP_UUP_TOOL_ROOT="${CLASSICSETUP_UUP_TOOL_ROOT}"
        CLASSICSETUP_UUP_CONVERTER_ROOT="${CLASSICSETUP_UUP_CONVERTER_ROOT}"
        CLASSICSETUP_UUP_DOTNET_ROOT="${CLASSICSETUP_UUP_DOTNET_ROOT}"
        CLASSICSETUP_WIMLIB_EXECUTABLE="${CLASSICSETUP_WIMLIB_EXECUTABLE}"
        CLASSICSETUP_UUP_ISO_EXTRACTOR="${CLASSICSETUP_UUP_ISO_EXTRACTOR}"
    )
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(classicsetup_uup_test PRIVATE
            -Wall -Wextra -Wpedantic
        )
    endif()
    add_test(NAME uup_backend_model COMMAND classicsetup_uup_test)

    add_executable(classicsetup_network_test
        tests/network_test.c
        src/network/network.c
    )

    set_target_properties(classicsetup_network_test PROPERTIES
        C_STANDARD 17
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO
    )

    target_include_directories(classicsetup_network_test PRIVATE include)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(classicsetup_network_test PRIVATE
            -Wall -Wextra -Wpedantic
        )
    endif()
    add_test(NAME network_model COMMAND classicsetup_network_test)

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
        src/tui/tui.c
    )

    set_target_properties(classicsetup_recommended_test PROPERTIES
        C_STANDARD 17
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO
    )

    target_include_directories(classicsetup_recommended_test PRIVATE
        include
        ${CURSES_INCLUDE_DIRS}
    )
    target_link_libraries(classicsetup_recommended_test PRIVATE
        ${CURSES_LIBRARIES}
    )
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(classicsetup_recommended_test PRIVATE
            -Wall -Wextra -Wpedantic
        )
    endif()

    add_test(NAME recommended_policy COMMAND classicsetup_recommended_test)

    add_executable(classicsetup_tui_layout_test
        tests/tui_layout_test.c
        src/tui/keymap.c
        src/tui/license_agreement.c
        src/tui/tui.c
    )

    set_target_properties(classicsetup_tui_layout_test PROPERTIES
        C_STANDARD 17
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO
    )

    target_include_directories(classicsetup_tui_layout_test PRIVATE
        include
        ${CURSES_INCLUDE_DIRS}
    )
    target_link_libraries(classicsetup_tui_layout_test PRIVATE
        ${CURSES_LIBRARIES}
    )
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(classicsetup_tui_layout_test PRIVATE
            -Wall -Wextra -Wpedantic
        )
    endif()

    add_test(NAME tui_compact_layout COMMAND classicsetup_tui_layout_test)

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
    add_test(
        NAME tui_back_key_policy
        COMMAND ${CMAKE_COMMAND}
            -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_back_key_policy.cmake
    )
endif()
