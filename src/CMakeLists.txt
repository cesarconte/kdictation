add_library(kdictation_core STATIC
    dictationcontroller.cpp
    dictationcontroller.h
    voicepostprocessor.cpp
    voicepostprocessor.h
    dictationaiformatter.cpp
    dictationaiformatter.h
    dictationhistorymanager.cpp
    dictationhistorymanager.h
    pipewirerecorder.cpp
    pipewirerecorder.h
    whisperengine.cpp
    whisperengine.h
    textinjector.cpp
    textinjector.h
)

ecm_add_wayland_client_protocol(kdictation_core
    PROTOCOL protocols/fake-input.xml
    BASENAME fake-input
)

target_include_directories(kdictation_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${PIPEWIRE_INCLUDE_DIRS}
    ${WHISPER_INCLUDE_DIRS}
    ${WAYLAND_CLIENT_INCLUDE_DIRS}
)

target_link_libraries(kdictation_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::DBus
    KF6::CoreAddons
    KF6::I18n
    KF6::GlobalAccel
    ${PIPEWIRE_LIBRARIES}
    ${WHISPER_LIBRARIES}
    ${WAYLAND_CLIENT_LIBRARIES}
)

qt_add_resources(KDICTATION_QRC resources.qrc)

add_executable(kdictation
    main.cpp
    ${KDICTATION_QRC}
)

target_link_libraries(kdictation PRIVATE
    kdictation_core
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)

install(TARGETS kdictation ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
