# ============================================================================
# Lumen Packaging Configuration
# Supports: Windows (NSIS), Linux (AppImage/DEB)
# Target OS: Windows 11+, Ubuntu 20.04+
# ============================================================================

set(CPACK_PACKAGE_NAME "Lumen")
set(CPACK_PACKAGE_VENDOR "OAQL Labs")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Quantum-Enhanced Portfolio Optimization")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Lumen")
set(CPACK_PACKAGE_CONTACT "support@oaql.com")

# ============================================================================
# Windows NSIS Installer Configuration
# ============================================================================
if(WIN32)
    set(CPACK_GENERATOR "NSIS")

    # NSIS specific settings
    set(CPACK_NSIS_DISPLAY_NAME "Lumen - Portfolio Optimization")
    set(CPACK_NSIS_PACKAGE_NAME "Lumen")
    set(CPACK_NSIS_HELP_LINK "https://github.com/oaqlabs/lumen")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/oaqlabs/lumen")
    set(CPACK_NSIS_CONTACT "support@oaql.com")
    set(CPACK_NSIS_MODIFY_PATH OFF)

    # Start menu and desktop shortcuts
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Lumen.lnk' '$INSTDIR\\\\bin\\\\lumen-gui.exe'"
    )
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Lumen.lnk'"
    )

    # Install locations
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")

    # Windows 11 minimum requirement notice
    set(CPACK_NSIS_WELCOME_TITLE "Welcome to Lumen Setup")
    set(CPACK_NSIS_WELCOME_TITLE_3LINES ON)

    # Installer icon
    if(EXISTS "${CMAKE_SOURCE_DIR}/apps/gui/resources/icons/lumen.ico")
        set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/apps/gui/resources/icons/lumen.ico")
        set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/apps/gui/resources/icons/lumen.ico")
    endif()

    # Add uninstaller to Add/Remove Programs
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)

    # File associations (optional - for .lumen portfolio files)
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
        WriteRegStr HKCR '.lumen' '' 'LumenPortfolio'
        WriteRegStr HKCR 'LumenPortfolio' '' 'Lumen Portfolio File'
        WriteRegStr HKCR 'LumenPortfolio\\\\shell\\\\open\\\\command' '' '\\\"$INSTDIR\\\\bin\\\\lumen-gui.exe\\\" \\\"%1\\\"'
    ")
    set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "
        DeleteRegKey HKCR '.lumen'
        DeleteRegKey HKCR 'LumenPortfolio'
    ")
endif()

# ============================================================================
# Linux Packaging Configuration
# ============================================================================
if(UNIX AND NOT APPLE)
    # Generate both DEB and AppImage
    set(CPACK_GENERATOR "DEB;External")

    # ========================================================================
    # DEB Package Configuration (Ubuntu 20.04+)
    # ========================================================================
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "OAQL Labs <support@oaql.com>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "finance")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/oaqlabs/lumen")

    # Dependencies for Ubuntu 20.04+
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "libqt6widgets6 (>= 6.2), libqt6charts6 (>= 6.2), libsqlite3-0 (>= 3.31)"
    )

    # Package architecture
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "arm64")
    endif()

    # ========================================================================
    # AppImage Configuration
    # ========================================================================
    # AppImage is built using a separate script (see scripts/build-appimage.sh)
    # CPack External generator triggers the script

    set(CPACK_EXTERNAL_PACKAGE_SCRIPT "${CMAKE_SOURCE_DIR}/cmake/appimage-package.cmake")
    set(CPACK_EXTERNAL_ENABLE_STAGING ON)

    # Desktop entry for Linux
    configure_file(
        "${CMAKE_SOURCE_DIR}/apps/gui/resources/lumen.desktop.in"
        "${CMAKE_BINARY_DIR}/lumen.desktop"
        @ONLY
    )

    install(FILES "${CMAKE_BINARY_DIR}/lumen.desktop"
        DESTINATION share/applications
    )

    # Install icons for Linux
    install(FILES "${CMAKE_SOURCE_DIR}/apps/gui/resources/icons/lumen.png"
        DESTINATION share/icons/hicolor/256x256/apps
        RENAME lumen.png
    )
endif()

# ============================================================================
# Common CPack Settings
# ============================================================================

# Components (for selective installation)
set(CPACK_COMPONENTS_ALL applications libraries headers)

set(CPACK_COMPONENT_APPLICATIONS_DISPLAY_NAME "Applications")
set(CPACK_COMPONENT_APPLICATIONS_DESCRIPTION "Lumen GUI and CLI applications")
set(CPACK_COMPONENT_APPLICATIONS_REQUIRED ON)

set(CPACK_COMPONENT_LIBRARIES_DISPLAY_NAME "Libraries")
set(CPACK_COMPONENT_LIBRARIES_DESCRIPTION "Lumen core library")

set(CPACK_COMPONENT_HEADERS_DISPLAY_NAME "Development Headers")
set(CPACK_COMPONENT_HEADERS_DESCRIPTION "Headers for development")
set(CPACK_COMPONENT_HEADERS_DEPENDS libraries)

# Package naming
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")

include(CPack)
