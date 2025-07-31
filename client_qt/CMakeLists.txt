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
    resources.qrc

    # UI 관련 파일들
    ui/mainwindow.cpp
    ui/mainwindow.h
    ui/mainwindow.ui
    ui/home.cpp
    ui/home.h
    ui/home.ui
    ui/login_window.h
    ui/login_window.cpp
    ui/qr_code_dialog.h
    ui/qr_code_dialog.cpp
    ui/conveyor.cpp
    ui/conveyor.h
    ui/conveyor.ui

    # 위젯 관련 파일들
    widgets/chartcardwidget.h
    widgets/chartcardwidget.cpp
    widgets/sectionboxwidget.h
    widgets/sectionboxwidget.cpp
    widgets/error_message_card.cpp
    widgets/error_message_card.h
    widgets/cardevent.cpp
    widgets/cardevent.h
    widgets/cardhovereffect.cpp
    widgets/cardhovereffect.h

    # 차트 관련 파일들
    charts/device_chart.cpp
    charts/device_chart.h
    charts/device_speed_chart.cpp
    charts/device_speed_chart.h
    charts/errorchartmanager.cpp
    charts/errorchartmanager.h

    # 비디오 관련 파일들
    video/videoplayer.cpp
    video/videoplayer.h
    video/video_client_functions.hpp
    video/video_mqtt.cpp
    video/video_mqtt.h
    video/streamer.cpp
    video/streamer.h

    # MCP 관련 파일들
    mcp/factory_mcp.cpp
    mcp/factory_mcp.h
    mcp/MCPAgentClient.cpp
    mcp/MCPAgentClient.h
    mcp/DataStructures.h
    mcp/ToolExamples.cpp
    mcp/ToolExamples.h
    mcp/PromptGenerators.cpp
    mcp/PromptGenerators.h
    mcp/DataFormatter.cpp
    mcp/DataFormatter.h
    mcp/mcp_btn.cpp
    mcp/mcp_btn.h
    mcp/chatbot_widget.cpp
    mcp/chatbot_widget.h

    # 유틸리티 파일들
    utils/ai_command.cpp
    utils/ai_command.h
    utils/font_manager.h
    utils/font_manager.cpp

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
