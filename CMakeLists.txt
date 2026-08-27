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
    "${CLIPCUTTER_SOURCE_DIR}/Utility.cpp"
    "${CLIPCUTTER_SOURCE_DIR}/Utility.h"
    "${CLIPCUTTER_SOURCE_DIR}/App/Models/ClipQueueModel.cpp"
    "${CLIPCUTTER_SOURCE_DIR}/App/Models/ClipQueueModel.h"
    "${CLIPCUTTER_SOURCE_DIR}/Core/Clip/Clip.h"
    "${CLIPCUTTER_SOURCE_DIR}/Core/Clip/Segment.cpp"
    "${CLIPCUTTER_SOURCE_DIR}/Core/Clip/Segment.h"
    "${CLIPCUTTER_SOURCE_DIR}/Core/Clip/TimeRange.cpp"
    "${CLIPCUTTER_SOURCE_DIR}/Core/Clip/TimeRange.h"
    "${CLIPCUTTER_SOURCE_DIR}/Core/Export/ExportJob.h"
    "${CLIPCUTTER_SOURCE_DIR}/Core/Export/ExportQueueController.cpp"
    "${CLIPCUTTER_SOURCE_DIR}/Core/Export/ExportQueueController.h"
    "${CLIPCUTTER_SOURCE_DIR}/Core/Export/ExportResult.h"
    "${CLIPCUTTER_SOURCE_DIR}/Core/Export/ExportState.cpp"
    "${CLIPCUTTER_SOURCE_DIR}/Core/Export/ExportState.h"
    "${CLIPCUTTER_SOURCE_DIR}/Core/Export/FfmpegCommand.h"
    "${CLIPCUTTER_SOURCE_DIR}/Core/Export/FfmpegCommandBuilder.cpp"
    "${CLIPCUTTER_SOURCE_DIR}/Core/Export/FfmpegCommandBuilder.h"
    "${CLIPCUTTER_SOURCE_DIR}/Core/Export/FfmpegProgress.h"
    "${CLIPCUTTER_SOURCE_DIR}/Core/Export/FfmpegProgressParser.cpp"
    "${CLIPCUTTER_SOURCE_DIR}/Core/Export/FfmpegProgressParser.h"
    "${CLIPCUTTER_SOURCE_DIR}/Core/Export/MetadataService.cpp"
    "${CLIPCUTTER_SOURCE_DIR}/Core/Export/MetadataService.h"
    "${CLIPCUTTER_SOURCE_DIR}/Core/Export/OutputPathPlanner.cpp"
    "${CLIPCUTTER_SOURCE_DIR}/Core/Export/OutputPathPlanner.h"
    "${CLIPCUTTER_SOURCE_DIR}/Core/Export/OutputProfile.cpp"
    "${CLIPCUTTER_SOURCE_DIR}/Core/Export/OutputProfile.h"
    "${CLIPCUTTER_SOURCE_DIR}/Core/Export/OutputVerifier.cpp"
    "${CLIPCUTTER_SOURCE_DIR}/Core/Export/OutputVerifier.h"
    "${CLIPCUTTER_SOURCE_DIR}/Core/Export/ProcessRunner.cpp"
    "${CLIPCUTTER_SOURCE_DIR}/Core/Export/ProcessRunner.h"
    "${CLIPCUTTER_SOURCE_DIR}/Core/Import/ClipImporter.cpp"
    "${CLIPCUTTER_SOURCE_DIR}/Core/Import/ClipImporter.h"
    "${CLIPCUTTER_SOURCE_DIR}/Core/Media/MediaInfo.h"
    "${CLIPCUTTER_SOURCE_DIR}/Core/Media/MediaProbe.cpp"
    "${CLIPCUTTER_SOURCE_DIR}/Core/Media/MediaProbe.h"
    "${CLIPCUTTER_SOURCE_DIR}/Core/Diagnostics/StartupDiagnostics.cpp"
    "${CLIPCUTTER_SOURCE_DIR}/Core/Diagnostics/StartupDiagnostics.h"
)
target_include_directories(clipcutter_core PUBLIC
    "$<BUILD_INTERFACE:${CLIPCUTTER_SOURCE_DIR}>"
)
target_link_libraries(clipcutter_core PUBLIC
    Qt6::Core
)
target_compile_features(clipcutter_core PUBLIC cxx_std_17)
clipcutter_configure_first_party_target(clipcutter_core)

qt_add_executable(ClipCutter WIN32)
target_sources(ClipCutter PRIVATE
    "${CLIPCUTTER_SOURCE_DIR}/Main.cpp"
    "${CLIPCUTTER_SOURCE_DIR}/MainWindow.cpp"
    "${CLIPCUTTER_SOURCE_DIR}/MainWindow.h"
    "${CLIPCUTTER_SOURCE_DIR}/MainWindow.ui"
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

    qt_add_executable(ClipCutterExportTests)
    target_sources(ClipCutterExportTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/ExportEngineTests.cpp"
    )
    target_include_directories(ClipCutterExportTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
    )
    target_link_libraries(ClipCutterExportTests PRIVATE
        clipcutter_core
        Qt6::Core
        Qt6::Test
    )
    target_compile_features(ClipCutterExportTests PRIVATE cxx_std_17)
    clipcutter_configure_first_party_target(ClipCutterExportTests)

    add_test(NAME ClipCutterExportTests COMMAND ClipCutterExportTests)

    qt_add_executable(ClipCutterMediaCorrectnessTests)
    target_sources(ClipCutterMediaCorrectnessTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/MediaCorrectnessTests.cpp"
    )
    target_include_directories(ClipCutterMediaCorrectnessTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_link_libraries(ClipCutterMediaCorrectnessTests PRIVATE clipcutter_core Qt6::Core Qt6::Test)
    target_compile_features(ClipCutterMediaCorrectnessTests PRIVATE cxx_std_17)
    clipcutter_configure_first_party_target(ClipCutterMediaCorrectnessTests)
    add_test(NAME ClipCutterMediaCorrectnessTests COMMAND ClipCutterMediaCorrectnessTests)
    if(WIN32)
        set_tests_properties(ClipCutterTests PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        )
        set_tests_properties(ClipCutterExportTests PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        )
        set_tests_properties(ClipCutterMediaCorrectnessTests PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        )
    endif()
endif()
