cmake_minimum_required(VERSION 3.21)
project(HamMixer VERSION 1.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui SerialPort WebEngineWidgets)

# Windows-specific settings
if(WIN32)
    set(CMAKE_WIN32_EXECUTABLE ON)
    add_definitions(-DUNICODE -D_UNICODE)
    add_definitions(-DNOMINMAX)
    add_definitions(-D_USE_MATH_DEFINES)
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Audio library sources
set(AUDIO_SOURCES
    src/audio/RingBuffer.cpp
    src/audio/DelayBuffer.cpp
    src/audio/MixerCore.cpp
    src/audio/AudioSync.cpp
    src/audio/Recorder.cpp
    src/audio/WasapiDevice.cpp
    src/audio/AudioManager.cpp
)

set(AUDIO_HEADERS
    src/audio/RingBuffer.h
    src/audio/DelayBuffer.h
    src/audio/MixerCore.h
    src/audio/AudioSync.h
    src/audio/Recorder.h
    src/audio/WasapiDevice.h
    src/audio/AudioManager.h
    src/audio/DeviceInfo.h
)

# UI library sources
set(UI_SOURCES
    src/ui/LevelMeter.cpp
    src/ui/SMeter.cpp
    src/ui/SMeterWindow.cpp
    src/ui/ChannelStrip.cpp
    src/ui/MasterStrip.cpp
    src/ui/Crossfader.cpp
    src/ui/DevicePanel.cpp
    src/ui/TransportControls.cpp
    src/ui/VBCableWizard.cpp
    src/ui/RadioControlPanel.cpp
    src/ui/WebSdrManagerDialog.cpp
    src/ui/AudioDevicesDialog.cpp
    src/ui/FrequencyLCD.cpp
    src/ui/RadioControlWindow.cpp
    src/ui/MainWindow.cpp
)

set(UI_HEADERS
    src/ui/Styles.h
    src/ui/LevelMeter.h
    src/ui/SMeter.h
    src/ui/SMeterWindow.h
    src/ui/ChannelStrip.h
    src/ui/MasterStrip.h
    src/ui/Crossfader.h
    src/ui/DevicePanel.h
    src/ui/TransportControls.h
    src/ui/VBCableWizard.h
    src/ui/RadioControlPanel.h
    src/ui/WebSdrManagerDialog.h
    src/ui/AudioDevicesDialog.h
    src/ui/FrequencyLCD.h
    src/ui/RadioControlWindow.h
    src/ui/MainWindow.h
)

# Config library sources
set(CONFIG_SOURCES
    src/config/Settings.cpp
)

set(CONFIG_HEADERS
    src/config/Settings.h
)

# Serial/Radio control library sources
set(SERIAL_SOURCES
    src/serial/CIVProtocol.cpp
    src/serial/CIVController.cpp
    src/serial/RadioController.cpp
    src/serial/KenwoodController.cpp
)

set(SERIAL_HEADERS
    src/serial/CIVProtocol.h
    src/serial/CIVController.h
    src/serial/RadioController.h
    src/serial/KenwoodController.h
)

# WebSDR/KiwiSDR control sources
set(WEBSDR_SOURCES
    src/websdr/WebSdrController.cpp
    src/websdr/KiwiSdrController.cpp
    src/websdr/WebSdrManager.cpp
)

set(WEBSDR_HEADERS
    src/websdr/WebSdrSite.h
    src/websdr/WebSdrController.h
    src/websdr/KiwiSdrController.h
    src/websdr/WebSdrManager.h
)

# Version header
set(VERSION_HEADERS
    include/HamMixer/Version.h
)

# Windows resource file
if(WIN32)
    set(WIN_RESOURCES resources/hammixer.rc)
endif()

# Qt resource file
set(QT_RESOURCES resources/resources.qrc)

# Main executable
add_executable(HamMixer WIN32
    src/main.cpp
    ${AUDIO_SOURCES}
    ${AUDIO_HEADERS}
    ${UI_SOURCES}
    ${UI_HEADERS}
    ${CONFIG_SOURCES}
    ${CONFIG_HEADERS}
    ${SERIAL_SOURCES}
    ${SERIAL_HEADERS}
    ${WEBSDR_SOURCES}
    ${WEBSDR_HEADERS}
    ${VERSION_HEADERS}
    ${WIN_RESOURCES}
    ${QT_RESOURCES}
)

# Link libraries
target_link_libraries(HamMixer PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
    Qt6::SerialPort
    Qt6::WebEngineWidgets
)

# Windows-specific libraries for WASAPI
if(WIN32)
    target_link_libraries(HamMixer PRIVATE
        ole32
        uuid
        winmm
        ksuser
        mfplat
        mfuuid
        avrt
        propsys
    )
endif()

# Set output directory
set_target_properties(HamMixer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
)

# Installation
install(TARGETS HamMixer
    RUNTIME DESTINATION bin
)

# Post-build: run windeployqt to copy Qt DLLs automatically
if(WIN32)
    # Find windeployqt executable
    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")

    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET HamMixer POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo "Running windeployqt..."
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --$<IF:$<CONFIG:Debug>,debug,release>
                --no-translations
                "$<TARGET_FILE:HamMixer>"
            COMMAND ${CMAKE_COMMAND} -E echo "Build complete: $<TARGET_FILE:HamMixer>"
            COMMENT "Deploying Qt dependencies..."
        )
    else()
        message(WARNING "windeployqt not found, Qt DLLs will not be deployed automatically")
        add_custom_command(TARGET HamMixer POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo "Build complete: $<TARGET_FILE:HamMixer>"
        )
    endif()
endif()
