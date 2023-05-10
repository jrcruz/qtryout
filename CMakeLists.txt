cmake_minimum_required(VERSION 3.14)

project(qtryout VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Quick Network Positioning)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Quick Network)

set(PROJECT_SOURCES
        weather_api.cpp
        main.cpp
        qml.qrc
)

set(ANDROID_EXTRA_LIBS
    /media/internal/android2/android_openssl/ssl_3/x86_64/libcrypto_3.so
    /media/internal/android2/android_openssl/ssl_3/x86_64/libssl_3.so
CACHE INTERNAL "")

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(qtryout
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET qtryout APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(qtryout SHARED
            ${PROJECT_SOURCES} ${ANDROID_EXTRA_LIBS}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(qtryout
          ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(qtryout
  PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Quick Qt${QT_VERSION_MAJOR}::Network)

set_target_properties(qtryout PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
    QT_ANDROID_EXTRA_LIBS "${ANDROID_EXTRA_LIBS}"
)

install(TARGETS qtryout
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

if(QT_VERSION_MAJOR EQUAL 6)
    qt_import_qml_plugins(qtryout)
    qt_finalize_executable(qtryout)
endif()
