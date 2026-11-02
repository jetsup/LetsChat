cmake_minimum_required(VERSION 3.21)

project(LetsChat VERSION 0.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core Gui Network Qml Quick QuickControls2 Sql Bluetooth Multimedia
)
# Widgets is only needed for the desktop system-tray icon
if(NOT ANDROID)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
else()
    find_package(Qt6 REQUIRED COMPONENTS CorePrivate)   # QtAndroidPrivate::startActivity (camera)
endif()

qt_standard_project_setup(REQUIRES 6.5)
qt_policy(SET QTP0004 NEW)   # extra QML directories get qmldir files automatically

# Content of downloaded design from Figma to Qt or similar
if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/importedcontent/CMakeLists.txt")
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/importedcontent)
endif()

qt_add_executable(appLetsChat
    main.cpp
    src/core/AppConfig.h
    src/core/IdentityManager.h        src/core/IdentityManager.cpp
    src/core/ThemeManager.h           src/core/ThemeManager.cpp
    src/chat/ChatController.h         src/chat/ChatController.cpp
    src/network/DiscoveryService.h    src/network/DiscoveryService.cpp
    src/network/TransferService.h     src/network/TransferService.cpp
    src/network/BluetoothService.h    src/network/BluetoothService.cpp
    src/network/FrameParser.h         src/network/FrameParser.cpp
    src/network/PeerManager.h         src/network/PeerManager.cpp
    src/network/LanTracker.h          src/network/LanTracker.cpp
    src/security/Crypto.h             src/security/Crypto.cpp
    src/security/NoiseXX.h            src/security/NoiseXX.cpp
    src/security/SecureChannel.h      src/security/SecureChannel.cpp
    src/security/SecurityManager.h    src/security/SecurityManager.cpp
    src/security/SecretStore.h        src/security/SecretStore.cpp
    src/storage/DatabaseManager.h     src/storage/DatabaseManager.cpp
    src/storage/CryptoUtils.h         src/storage/CryptoUtils.cpp
    src/notifications/NotificationManager.h src/notifications/NotificationManager.cpp
    src/models/PeerListModel.h        src/models/PeerListModel.cpp
    src/models/MessageModel.h         src/models/MessageModel.cpp
    src/platform/BackgroundService.h  src/platform/BackgroundService.cpp
    src/platform/MediaService.h       src/platform/MediaService.cpp
    src/utils/DeviceInfo.h            src/utils/DeviceInfo.cpp
)

# Theme.qml is a QML singleton, available to every file through "import LetsChat"
set_source_files_properties(qml/themes/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(appLetsChat
    URI LetsChat
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/themes/Theme.qml
        qml/components/Icon.qml
        qml/components/Avatar.qml
        qml/components/IconButton.qml
        qml/components/PeerItem.qml
        qml/components/MessageBubble.qml
        qml/components/TransportBadge.qml
        qml/components/Toast.qml
        qml/components/VoiceNote.qml
        qml/components/MediaViewer.qml
        qml/dialogs/PhotoDialog.qml
        qml/dialogs/VoiceDialog.qml
        qml/views/OnboardingView.qml
        qml/views/ContactsView.qml
        qml/views/ChatView.qml
        qml/views/SettingsView.qml
        qml/views/BluetoothView.qml
        qml/views/ProfileView.qml
        qml/dialogs/AliasDialog.qml
)

qt_add_resources(appLetsChat appicons
    PREFIX "/icons"
    BASE resources/icons
    FILES resources/icons/app.png
)

target_include_directories(appLetsChat PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_target_properties(appLetsChat PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER com.smalnets.letschat
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
    QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
    QT_ANDROID_VERSION_CODE 2
    QT_ANDROID_VERSION_NAME "0.2"
)

target_link_libraries(appLetsChat PRIVATE
    Qt6::Core Qt6::Gui Qt6::Network Qt6::Qml Qt6::Quick Qt6::QuickControls2
    Qt6::Sql Qt6::Bluetooth Qt6::Multimedia
)
if(ANDROID)
    target_link_libraries(appLetsChat PRIVATE Qt6::CorePrivate)   # QtAndroidPrivate::startActivity
    # OpenSSL (libcrypto only) is linked statically from the Android SDK's android_openssl package, so no
    # extra shared libraries have to be shipped and no symbols clash with the system's own crypto library.
    if(NOT ANDROID_SDK_ROOT AND DEFINED ENV{ANDROID_SDK_ROOT})
        set(ANDROID_SDK_ROOT "$ENV{ANDROID_SDK_ROOT}")
    endif()
    set(LETSCHAT_OPENSSL_ANDROID "${ANDROID_SDK_ROOT}/android_openssl/ssl_3" CACHE PATH
        "android_openssl (OpenSSL 3) directory containing include/ and <abi>/libcrypto.a")
    if(NOT EXISTS "${LETSCHAT_OPENSSL_ANDROID}/${CMAKE_ANDROID_ARCH_ABI}/libcrypto.a")
        message(FATAL_ERROR "OpenSSL for Android not found. Install 'android_openssl' with the SDK manager or "
                            "set LETSCHAT_OPENSSL_ANDROID (looked in ${LETSCHAT_OPENSSL_ANDROID}).")
    endif()
    target_include_directories(appLetsChat PRIVATE "${LETSCHAT_OPENSSL_ANDROID}/include")
    target_link_libraries(appLetsChat PRIVATE "${LETSCHAT_OPENSSL_ANDROID}/${CMAKE_ANDROID_ARCH_ABI}/libcrypto.a")
    target_link_options(appLetsChat PRIVATE "-Wl,--exclude-libs,ALL")
else()
    find_package(OpenSSL 3.0 REQUIRED COMPONENTS Crypto)
    target_link_libraries(appLetsChat PRIVATE OpenSSL::Crypto)
endif()
if(NOT ANDROID)
    target_link_libraries(appLetsChat PRIVATE Qt6::Widgets)
endif()

install(TARGETS appLetsChat
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# ---------------------------------------------------------------------------------------------------------
# Security test-suite:  cmake -DLETSCHAT_BUILD_TESTS=ON ... && cmake --build ... && ctest --output-on-failure
option(LETSCHAT_BUILD_TESTS "Build the security unit tests" OFF)
if(LETSCHAT_BUILD_TESTS AND NOT ANDROID)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(tst_security
        tests/tst_security.cpp
        src/security/Crypto.h          src/security/Crypto.cpp
        src/security/NoiseXX.h         src/security/NoiseXX.cpp
        src/security/SecureChannel.h   src/security/SecureChannel.cpp
        src/security/SecurityManager.h src/security/SecurityManager.cpp
        src/security/SecretStore.h     src/security/SecretStore.cpp
        src/storage/DatabaseManager.h  src/storage/DatabaseManager.cpp
        src/storage/CryptoUtils.h      src/storage/CryptoUtils.cpp
    )
    target_include_directories(tst_security PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tst_security PRIVATE Qt6::Test Qt6::Core Qt6::Network Qt6::Sql OpenSSL::Crypto)
    add_test(NAME security COMMAND tst_security)
endif()
