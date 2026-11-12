find_package(Qt6 REQUIRED COMPONENTS Test Qml Quick)

qt_add_resources(TEST_QRC ../src/resources.qrc)

add_executable(test_kdictation
    test_kdictation.cpp
    ${TEST_QRC}
)

target_link_libraries(test_kdictation PRIVATE
    kdictation_core
    Qt6::Test
    Qt6::Qml
    Qt6::Quick
)

add_test(NAME test_kdictation COMMAND test_kdictation)

# The CachyOS.UI design system provides the shared layout auditor.
list(APPEND CMAKE_PREFIX_PATH "$ENV{HOME}/.local" "/usr/local")
find_package(CachyOSUITesting REQUIRED)

add_executable(test_layout_audit
    test_layout_audit.cpp
    ../src/dictationcontroller.cpp
    ../src/dictationcontroller.h
    ${TEST_QRC}
)

target_include_directories(test_layout_audit PRIVATE ../src)
target_link_libraries(test_layout_audit PRIVATE
    kdictation_core
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Test
    CachyOSUITesting::CachyOSUITesting
)

add_test(NAME test_layout_audit COMMAND test_layout_audit)

set_tests_properties(test_layout_audit PROPERTIES
    ENVIRONMENT "CACHYOS_UI_IMPORT_PATH=$ENV{HOME}/.local/lib/qt6/qml"
)
