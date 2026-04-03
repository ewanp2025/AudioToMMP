cmake_minimum_required(VERSION 3.16)

project(HarmonicGridAnalyser VERSION 0.1 LANGUAGES CXX C)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MINGW)
    add_compile_options(-Wa,-mbig-obj)
endif()


find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets PrintSupport Multimedia Concurrent Xml)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets PrintSupport Multimedia Concurrent Xml)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h

        libs/qcustomplot.cpp
        libs/qcustomplot.h
        libs/kiss_fft.c
        libs/kiss_fft.h
        libs/_kiss_fft_guts.h
        libs/kiss_fft_log.h
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(HarmonicGridAnalyser
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )

else()
    if(ANDROID)
        add_library(HarmonicGridAnalyser SHARED
            ${PROJECT_SOURCES}
        )

    else()
        add_executable(HarmonicGridAnalyser
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(HarmonicGridAnalyser PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::PrintSupport
    Qt${QT_VERSION_MAJOR}::Multimedia
    Qt${QT_VERSION_MAJOR}::Concurrent
    Qt${QT_VERSION_MAJOR}::Xml
)

target_include_directories(HarmonicGridAnalyser PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/libs
)


if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.HarmonicGridAnalyser)
endif()

set_target_properties(HarmonicGridAnalyser PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS HarmonicGridAnalyser
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(HarmonicGridAnalyser)
endif()
