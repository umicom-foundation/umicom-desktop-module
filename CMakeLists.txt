#-----------------------------------------------------------------------------
# Umicom Desktop Module
# File: CMakeLists.txt
#
# PURPOSE:
#   Build the thin Umicom Desk product composition against public Framework
#   targets supplied by the parent superproject or an installed Framework SDK.
#
# AUTHOR AND ORGANISATION:
#   Sammy Hegab
#   Umicom Foundation
#
# LICENCE:
#   MIT
#-----------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.24)

project(
    UmicomDesktopModule
    VERSION 0.1.0
    DESCRIPTION "Thin Umicom Desk application composition"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(UMICOM_DESKTOP_BUILD_GTK
       "Build the GTK4 Umicom Desk executable"
       ON)
option(UMICOM_DESKTOP_GUI
       "Build Umicom Desk as a Windows GUI-subsystem executable"
       ON)
option(UMICOM_DESKTOP_COMPOSE_STUDIO
       "Expose the composed Studio executable in Umicom Desk"
       ON)
option(UMICOM_DESKTOP_COMPOSE_OS
       "Expose the composed OS Control Centre in Umicom Desk"
       ON)

include(CTest)
include(GNUInstallDirs)

if(NOT TARGET Umicom::Framework)
    find_package(UmicomFramework CONFIG REQUIRED)
endif()

if(NOT COMMAND umicom_apply_warnings)
    function(umicom_apply_warnings target)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4)
        else()
            target_compile_options(${target} PRIVATE
                -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
        endif()
    endfunction()
endif()

if(NOT COMMAND umicom_apply_sanitizers)
    function(umicom_apply_sanitizers target)
    endfunction()
endif()

add_library(umicom_desktop_module STATIC
    src/desktop_module.c
    src/context_link_centre.c
)
add_library(Umicom::desktop_module ALIAS umicom_desktop_module)

target_include_directories(umicom_desktop_module PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(umicom_desktop_module PUBLIC
    Umicom::Framework
)
target_compile_definitions(umicom_desktop_module PUBLIC
    UMICOM_DESKTOP_COMPOSE_STUDIO=$<BOOL:${UMICOM_DESKTOP_COMPOSE_STUDIO}>
    UMICOM_DESKTOP_COMPOSE_OS=$<BOOL:${UMICOM_DESKTOP_COMPOSE_OS}>
)
umicom_apply_warnings(umicom_desktop_module)
umicom_apply_sanitizers(umicom_desktop_module)

add_executable(umicom-desk-console
    src/console/main.c
)
target_link_libraries(umicom-desk-console PRIVATE
    Umicom::desktop_module
)
umicom_apply_warnings(umicom-desk-console)
umicom_apply_sanitizers(umicom-desk-console)

if(UMICOM_DESKTOP_BUILD_GTK)
    if(NOT TARGET Umicom::ui_gtk4)
        message(FATAL_ERROR
            "UMICOM_DESKTOP_BUILD_GTK is ON but the Framework GTK4 adapter "
            "target Umicom::ui_gtk4 is unavailable.")
    endif()

    if(WIN32 AND UMICOM_DESKTOP_GUI)
        add_executable(umicom-desk WIN32
            src/gtk/main.c
        )
    else()
        add_executable(umicom-desk
            src/gtk/main.c
        )
    endif()
    target_link_libraries(umicom-desk PRIVATE
        Umicom::desktop_module
        Umicom::ui_gtk4
    )
    umicom_apply_warnings(umicom-desk)
    umicom_apply_sanitizers(umicom-desk)
endif()

if(BUILD_TESTING)
    add_executable(umicom-desktop-module-test
        tests/test_desktop_module.c
    )
    target_link_libraries(umicom-desktop-module-test PRIVATE
        Umicom::desktop_module
    )
    umicom_apply_warnings(umicom-desktop-module-test)
    umicom_apply_sanitizers(umicom-desktop-module-test)
    add_test(
        NAME desktop.module
        COMMAND umicom-desktop-module-test
    )
endif()

install(TARGETS
    umicom_desktop_module
    umicom-desk-console
    EXPORT UmicomDesktopModuleTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
if(TARGET umicom-desk)
    install(TARGETS umicom-desk
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY resources/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/umicom/applications/desktop)
install(FILES application.umicom.yaml README.md LICENSE
    DESTINATION ${CMAKE_INSTALL_DATADIR}/umicom/applications/desktop)

message(STATUS "Umicom Desktop Module version: ${PROJECT_VERSION}")
message(STATUS "Umicom Desk GTK4 executable: ${UMICOM_DESKTOP_BUILD_GTK}")

include(
    "${CMAKE_CURRENT_SOURCE_DIR}/../../framework/cmake/UmicomApplicationProductisationContribution.cmake")
umicom_add_application_productisation_contribution(
    NAME "desktop"
    TARGET "umicom_desktop_productisation"
    SOURCE "src/productisation_contribution.c"
    TEST_SOURCE "tests/test_productisation_contribution.c")
