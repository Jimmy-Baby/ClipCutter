cmake_minimum_required(VERSION 3.22)

project(ClipCutter VERSION 2.1.1 LANGUAGES CXX)

option(CLIPCUTTER_WARNINGS_AS_ERRORS "Treat first-party compiler warnings as errors" OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Multimedia
    MultimediaWidgets
)

qt_standard_project_setup(REQUIRES 6.5)
set(CMAKE_AUTORCC ON)

set(CLIPCUTTER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/ClipCutter")

function(clipcutter_configure_first_party_target target)
    target_compile_definitions(${target} PRIVATE
        $<$<PLATFORM_ID:Windows>:NOMINMAX>
        $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
    )

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /Zc:__cplusplus /utf-8)
        if(CLIPCUTTER_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(CLIPCUTTER_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

add_library(clipcutter_core STATIC)
target_sources(clipcutter_core PRIVATE
    "${CLIPCUTTER_SOURCE_DIR}/ClipLogic.cpp"
    "${CLIPCUTTER_SOURCE_DIR}/ClipLogic.h"
    "${CLIPCUTTER_SOURCE_DIR}/FFmpeg.cpp"
    "${CLIPCUTTER_SOURCE_DIR}/FFmpeg.h"
    "${CLIPCUTTER_SOURCE_DIR}/QueueItem.cpp"
    "${CLIPCUTTER_SOURCE_DIR}/QueueItem.h"
    "${CLIPCUTTER_SOURCE_DIR}/Utility.cpp"
    "${CLIPCUTTER_SOURCE_DIR}/Utility.h"
)
target_include_directories(clipcutter_core PUBLIC
    "$<BUILD_INTERFACE:${CLIPCUTTER_SOURCE_DIR}>"
)
target_link_libraries(clipcutter_core PUBLIC
    Qt6::Core
    Qt6::Widgets
)
target_compile_features(clipcutter_core PUBLIC cxx_std_17)
clipcutter_configure_first_party_target(clipcutter_core)

qt_add_executable(ClipCutter WIN32)
target_sources(ClipCutter PRIVATE
    "${CLIPCUTTER_SOURCE_DIR}/Main.cpp"
    "${CLIPCUTTER_SOURCE_DIR}/CCMainWindow.cpp"
    "${CLIPCUTTER_SOURCE_DIR}/CCMainWindow.h"
    "${CLIPCUTTER_SOURCE_DIR}/CCMainWindow.ui"
    "${CLIPCUTTER_SOURCE_DIR}/ClipCutterResources.qrc"
)
target_include_directories(ClipCutter PRIVATE
    "${CLIPCUTTER_SOURCE_DIR}"
)
target_link_libraries(ClipCutter PRIVATE
    clipcutter_core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
)
target_compile_features(ClipCutter PRIVATE cxx_std_17)
clipcutter_configure_first_party_target(ClipCutter)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

    qt_add_executable(ClipCutterTests)
    target_sources(ClipCutterTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/RegressionTests.cpp"
    )
    target_include_directories(ClipCutterTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
    )
    target_link_libraries(ClipCutterTests PRIVATE
        clipcutter_core
        Qt6::Core
        Qt6::Test
    )
    target_compile_features(ClipCutterTests PRIVATE cxx_std_17)
    clipcutter_configure_first_party_target(ClipCutterTests)

    add_test(NAME ClipCutterTests COMMAND ClipCutterTests)
    if(WIN32)
        set_tests_properties(ClipCutterTests PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        )
    endif()
endif()
