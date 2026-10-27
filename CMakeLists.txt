cmake_minimum_required(VERSION 3.16)

project(LetsChat VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Network
    Qml
    Quick
    Sql
    Concurrent
    Widgets
)

qt_standard_project_setup()

# Content of downloaded design from Figma to Qt or similar
if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/importedcontent/CMakeLists.txt")
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/importedcontent)
endif()

set(LETSCHAT_CORE_SOURCES
    src/core/AppConfig.h
    src/core/AppConfig.cpp
    src/core/IdentityManager.h
    src/core/IdentityManager.cpp
    src/core/ThemeManager.h
    src/core/ThemeManager.cpp
    src/core/SettingsManager.h
    src/core/SettingsManager.cpp
)

set(LETSCHAT_NETWORK_SOURCES
    src/network/DiscoveryService.h
    src/network/DiscoveryService.cpp
    src/network/PeerManager.h
    src/network/PeerManager.cpp
    src/network/TransferService.h
    src/network/TransferService.cpp
    src/network/LanTracker.h
    src/network/LanTracker.cpp
)

set(LETSCHAT_STORAGE_SOURCES
    src/storage/DatabaseManager.h
    src/storage/DatabaseManager.cpp
    src/storage/CryptoUtils.h
    src/storage/CryptoUtils.cpp
)

set(LETSCHAT_NOTIFICATIONS_SOURCES
    src/notifications/NotificationManager.h
    src/notifications/NotificationManager.cpp
)

set(LETSCHAT_MODELS_SOURCES
    src/models/PeerListModel.h
    src/models/PeerListModel.cpp
    src/models/MessageModel.h
    src/models/MessageModel.cpp
)

set(LETSCHAT_PLATFORM_SOURCES
    src/platform/BackgroundService.h
    src/platform/BackgroundService.cpp
)

set(LETSCHAT_UTILS_SOURCES
    src/utils/DeviceInfo.h
    src/utils/DeviceInfo.cpp
)

qt_add_executable(appLetsChat
    main.cpp
    ${LETSCHAT_CORE_SOURCES}
    ${LETSCHAT_NETWORK_SOURCES}
    ${LETSCHAT_STORAGE_SOURCES}
    ${LETSCHAT_NOTIFICATIONS_SOURCES}
    ${LETSCHAT_MODELS_SOURCES}
    ${LETSCHAT_PLATFORM_SOURCES}
    ${LETSCHAT_UTILS_SOURCES}
)

qt_add_qml_module(appLetsChat
    URI LetsChat
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/views/ChatView.qml
        qml/views/ContactsView.qml
        qml/views/SettingsView.qml
        qml/views/OnboardingView.qml
        qml/components/PeerItem.qml
        qml/components/MessageBubble.qml
        qml/components/StatusIndicator.qml
        qml/dialogs/AliasDialog.qml
        qml/themes/Theme.qml
    SOURCES
        ${LETSCHAT_CORE_SOURCES}
        ${LETSCHAT_NETWORK_SOURCES}
        ${LETSCHAT_STORAGE_SOURCES}
        ${LETSCHAT_NOTIFICATIONS_SOURCES}
        ${LETSCHAT_MODELS_SOURCES}
        ${LETSCHAT_PLATFORM_SOURCES}
        ${LETSCHAT_UTILS_SOURCES}
    RESOURCES
        resources/icons.qrc
)

# Add UI files for Qt Designer
set(LETSCHAT_UI_FILES
    forms/AliasDialog.ui
    forms/SettingsDialog.ui
    forms/AboutDialog.ui
)

qt_wrap_ui(LETSCHAT_UI_HEADERS ${LETSCHAT_UI_FILES})

target_sources(appLetsChat PRIVATE
    ${LETSCHAT_UI_HEADERS}
)

target_include_directories(appLetsChat PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
set_target_properties(appLetsChat PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER com.smalnets.letschat
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
    QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
    QT_ANDROID_PACKAGE_NAME "com.smalnets.letschat"
)

target_link_libraries(appLetsChat
    PRIVATE
    Qt6::Quick
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Qml
    Qt6::Sql
    Qt6::Concurrent
    Qt6::Widgets
)

install(TARGETS appLetsChat
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
