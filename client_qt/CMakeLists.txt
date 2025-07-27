cmake_minimum_required(VERSION 3.16)

project(client_qt VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# OpenCV 경로 설정
set(OpenCV_DIR "C:/opencv/opencv/build_mingw/install")
find_package(OpenCV REQUIRED)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS
    Core
    Widgets
    Network
    Mqtt
    Charts
)
find_package(Qt6Multimedia REQUIRED)
find_package(Qt6MultimediaWidgets REQUIRED)

# config 폴더 파일 모음
file(GLOB CONFIG_FILES "config/*")
source_group("config" FILES ${CONFIG_FILES})

# 전체 프로젝트 파일 통합 관리
set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui

    streamer.cpp
    streamer.h

    home.cpp
    home.h
    home.ui

    conveyor.cpp
    conveyor.h
    conveyor.ui

    factory_mcp.cpp
    factory_mcp.h

    ai_command.cpp
    ai_command.h

    videoplayer.cpp
    videoplayer.h

    video_client_functions.hpp

    video_mqtt.cpp
    video_mqtt.h

    mcp_btn.cpp
    mcp_btn.h

    chatbot_widget.cpp
    chatbot_widget.h

    errorchartmanager.cpp
    errorchartmanager.h

    cardevent.cpp
    cardevent.h

    cardhovereffect.cpp
    cardhovereffect.h

    error_message_card.cpp
    error_message_card.h

    device_chart.cpp
    device_chart.h

    login_window.h
    login_window.cpp
    qr_code_dialog.h
    qr_code_dialog.cpp
    resources.qrc

    ${CONFIG_FILES}
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(client_qt
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    if(ANDROID)
        add_library(client_qt SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(client_qt
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_include_directories(client_qt PRIVATE ${OpenCV_INCLUDE_DIRS})

target_link_libraries(client_qt PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Mqtt
    Qt${QT_VERSION_MAJOR}::Charts
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    ${OpenCV_LIBS}
)

if(${QT_VERSION} VERSION_LESS 6.1.0)
    set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.client_qt)
endif()

set_target_properties(client_qt PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS client_qt
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(client_qt)
endif()
