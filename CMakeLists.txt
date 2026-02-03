cmake_minimum_required(VERSION 3.21)
project(DecentBridge VERSION 0.1.0 LANGUAGES CXX)

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
)

# Android requires Gui for the platform plugin, even for non-GUI apps
if(ANDROID)
    find_package(Qt6 REQUIRED COMPONENTS Gui)
endif()

# Core sources
set(SOURCES
    src/main.cpp
    src/core/bridge.cpp
    src/core/settings.cpp
)

set(HEADERS
    src/core/bridge.h
    src/core/settings.h
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

# Network (HTTP + WebSocket servers)
list(APPEND SOURCES
    src/network/httpserver.cpp
    src/network/websocketserver.cpp
)

list(APPEND HEADERS
    src/network/httpserver.h
    src/network/websocketserver.h
)

# Create executable (qt_add_executable handles Android APK generation)
qt_add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

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

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Android specific
if(ANDROID)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
        QT_ANDROID_SIGN_APK ON
        QT_ANDROID_SIGN_KEY_STORE_PATH "C:/CODE/Android APK keystore.jks"
        QT_ANDROID_SIGN_KEY_STORE_ALIAS "de1-key"
        QT_ANDROID_SIGN_KEY_STORE_PASSWORD "$ENV{ANDROID_KEYSTORE_PASSWORD}"
        QT_ANDROID_SIGN_KEY_PASSWORD "$ENV{ANDROID_KEYSTORE_PASSWORD}"
    )
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Bluetooth
    Qt6::Network
    Qt6::WebSockets
)

# Android requires Gui for the platform plugin
if(ANDROID)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Gui)
endif()

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
