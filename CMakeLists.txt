cmake_minimum_required(VERSION 3.21)
project(DecentBridge VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# Find Qt packages
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Bluetooth
    Network
    WebSockets
    Quick
    Qml
)

# Gui is needed for QML on all platforms
find_package(Qt6 REQUIRED COMPONENTS Gui)

# Core sources
set(SOURCES
    src/main.cpp
    src/core/bridge.cpp
    src/core/settings.cpp
    src/core/skinmanager.cpp
)

set(HEADERS
    src/core/bridge.h
    src/core/settings.h
    src/core/skinmanager.h
)

# BLE core
list(APPEND SOURCES
    src/ble/blemanager.cpp
    src/ble/de1device.cpp
    src/ble/scaledevice.cpp
    src/ble/sensordevice.cpp
    src/ble/protocol/binarycodec.cpp
)

list(APPEND HEADERS
    src/ble/blemanager.h
    src/ble/de1device.h
    src/ble/scaledevice.h
    src/ble/sensordevice.h
    src/ble/protocol/de1characteristics.h
    src/ble/protocol/binarycodec.h
)

# Scale implementations (14 scales from Decenza)
list(APPEND SOURCES
    src/ble/scales/scalefactory.cpp
    src/ble/scales/acaiascale.cpp
    src/ble/scales/atomhearteclairscale.cpp
    src/ble/scales/bookooscale.cpp
    src/ble/scales/decentscale.cpp
    src/ble/scales/difluidscale.cpp
    src/ble/scales/eurekaprecisascale.cpp
    src/ble/scales/felicitascale.cpp
    src/ble/scales/flowscale.cpp
    src/ble/scales/hiroiascale.cpp
    src/ble/scales/skalescale.cpp
    src/ble/scales/smartchefscale.cpp
    src/ble/scales/solobaristascale.cpp
    src/ble/scales/variaakuscale.cpp
)

list(APPEND HEADERS
    src/ble/scales/scalefactory.h
    src/ble/scales/acaiascale.h
    src/ble/scales/atomhearteclairscale.h
    src/ble/scales/bookooscale.h
    src/ble/scales/decentscale.h
    src/ble/scales/difluidscale.h
    src/ble/scales/eurekaprecisascale.h
    src/ble/scales/felicitascale.h
    src/ble/scales/flowscale.h
    src/ble/scales/hiroiascale.h
    src/ble/scales/skalescale.h
    src/ble/scales/smartchefscale.h
    src/ble/scales/solobaristascale.h
    src/ble/scales/variaakuscale.h
)

# Sensor implementations
list(APPEND SOURCES
    src/ble/sensors/sensorfactory.cpp
    src/ble/sensors/bookoomonitor.cpp
)

list(APPEND HEADERS
    src/ble/sensors/sensorfactory.h
    src/ble/sensors/bookoomonitor.h
)

# BLE transport layer (platform-specific)
list(APPEND HEADERS src/ble/transport/scalebletransport.h)

if(ANDROID)
    # Qt 6.10 BLE works on Android - no need for Nordic/Java fallback
    list(APPEND SOURCES src/ble/transport/qtscalebletransport.cpp)
    list(APPEND HEADERS src/ble/transport/qtscalebletransport.h)
elseif(IOS)
    enable_language(OBJCXX)
    list(APPEND SOURCES src/ble/transport/corebluetooth/corebluetoothscalebletransport.mm)
    list(APPEND HEADERS src/ble/transport/corebluetooth/corebluetoothscalebletransport.h)
else()
    # Desktop (Windows, macOS, Linux)
    list(APPEND SOURCES src/ble/transport/qtscalebletransport.cpp)
    list(APPEND HEADERS src/ble/transport/qtscalebletransport.h)
endif()

# Network (HTTP + WebSocket + Discovery)
list(APPEND SOURCES
    src/network/httpserver.cpp
    src/network/websocketserver.cpp
    src/network/discoveryservice.cpp
)

list(APPEND HEADERS
    src/network/httpserver.h
    src/network/websocketserver.h
    src/network/discoveryservice.h
)

# Create executable (qt_add_executable handles Android APK generation)
qt_add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Add QML UI as resource
qt_add_resources(${PROJECT_NAME} "qml"
    PREFIX "/"
    BASE src/qml
    FILES
        src/qml/Main.qml
)

# Add resources (API documentation)
qt_add_resources(${PROJECT_NAME} "api_docs"
    PREFIX "/"
    FILES
        assets/api/index.html
        assets/api/rest_v1.yml
        assets/api/websocket_v1.yml
        assets/api/vendor/swagger-ui.css
        assets/api/vendor/swagger-ui-bundle.js
        assets/api/vendor/swagger-ui-standalone-preset.js
        assets/api/vendor/react.production.min.js
        assets/api/vendor/react-dom.production.min.js
        assets/api/vendor/asyncapi-standalone.js
        assets/api/vendor/asyncapi.css
        assets/api/vendor/js-yaml.min.js
        assets/api/favicon.png
)

# Bundled skin (fallback for first launch without internet)
qt_add_resources(${PROJECT_NAME} "skin"
    PREFIX "/"
    FILES assets/skin.zip
)

# Add bundled default profiles
file(GLOB PROFILE_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "assets/profiles/*.json")
qt_add_resources(${PROJECT_NAME} "profiles"
    PREFIX "/"
    FILES ${PROFILE_FILES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
)

# mDNS service advertisement (pure Qt, no platform dependencies)
include(FetchContent)
FetchContent_Declare(
    qmdnsengine
    URL https://github.com/nitroshare/qmdnsengine/archive/refs/heads/master.zip
)
FetchContent_MakeAvailable(qmdnsengine)

# miniz for zip extraction (single-file, public domain)
# Suppress CMake policy warning for miniz's old cmake_minimum_required
set(_saved_cmake_policy_min "${CMAKE_POLICY_VERSION_MINIMUM}")
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_Declare(
    miniz
    URL https://github.com/richgel999/miniz/archive/refs/tags/3.0.2.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(miniz)
set(CMAKE_POLICY_VERSION_MINIMUM "${_saved_cmake_policy_min}")
if(ANDROID)
    # Enable 64-bit file APIs on Android to suppress miniz largefile warning
    target_compile_definitions(miniz PRIVATE _LARGEFILE64_SOURCE)
endif()

# Android specific
if(ANDROID)
    # Fetch OpenSSL for Android (fixes "No functional TLS backend" error)
    FetchContent_Declare(
        android_openssl
        URL https://github.com/KDAB/android_openssl/archive/refs/heads/master.zip
    )
    FetchContent_MakeAvailable(android_openssl)
    include(${android_openssl_SOURCE_DIR}/android_openssl.cmake)

    set_target_properties(${PROJECT_NAME} PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
        QT_ANDROID_SIGN_APK ON
        QT_ANDROID_SIGN_KEY_STORE_PATH "C:/CODE/Android APK keystore.jks"
        QT_ANDROID_SIGN_KEY_STORE_ALIAS "de1-key"
        QT_ANDROID_SIGN_KEY_STORE_PASSWORD "$ENV{ANDROID_KEYSTORE_PASSWORD}"
        QT_ANDROID_SIGN_KEY_PASSWORD "$ENV{ANDROID_KEYSTORE_PASSWORD}"
    )

    add_android_openssl_libraries(${PROJECT_NAME})
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Bluetooth
    Qt6::Network
    Qt6::WebSockets
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    qmdnsengine
    miniz
)

# iOS specific
if(IOS)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "-framework CoreBluetooth"
    )
endif()

# Install
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Print build info
message(STATUS "")
message(STATUS "DecentBridge v${PROJECT_VERSION}")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "  Scales: Acaia, Atomheart, Bookoo, Decent, DiFluid, Eureka, Felicita, Flow, Hiroia, Skale, SmartChef, SoloBarista, Varia")
message(STATUS "")
