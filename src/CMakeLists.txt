add_library(kdictation_core STATIC
    dictationcontroller.cpp
    dictationcontroller.h
)

target_include_directories(kdictation_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(kdictation_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::DBus
    KF6::CoreAddons
    KF6::I18n
    KF6::GlobalAccel
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
    LayerShellQt::Interface
)

install(TARGETS kdictation ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
