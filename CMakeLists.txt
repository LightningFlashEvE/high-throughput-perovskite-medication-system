cmake_minimum_required(VERSION 3.19)
project(PhenoLabHT LANGUAGES CXX)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dialogs
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui_components
)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

qt_standard_project_setup()

qt_add_executable(PhenoLabHT
    WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/MainWindow.h src/MainWindow.cpp
    src/common/StringUtils.h src/common/StringUtils.cpp
    src/dialogs/CommuInfoDialog.h src/dialogs/CommuInfoDialog.cpp
    src/ui_components/ControlPannel.h src/ui_components/ControlPannel.cpp
)

target_link_libraries(PhenoLabHT
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt6::Network
)

include(GNUInstallDirs)

install(TARGETS PhenoLabHT
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET PhenoLabHT
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
