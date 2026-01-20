# ============================================================================
# AppImage Packaging Script
# Called by CPack External generator for Linux AppImage creation
# ============================================================================

message(STATUS "Building AppImage package...")

# Check for linuxdeployqt or linuxdeploy
find_program(LINUXDEPLOY_EXECUTABLE NAMES linuxdeploy linuxdeploy-x86_64.AppImage)
find_program(LINUXDEPLOYQT_EXECUTABLE NAMES linuxdeployqt)

if(NOT LINUXDEPLOY_EXECUTABLE AND NOT LINUXDEPLOYQT_EXECUTABLE)
    message(WARNING "Neither linuxdeploy nor linuxdeployqt found. "
                    "AppImage will not be created. "
                    "Install from: https://github.com/linuxdeploy/linuxdeploy/releases")
    return()
endif()

# Create AppDir structure
set(APPDIR "${CPACK_TEMPORARY_DIRECTORY}/AppDir")
file(MAKE_DIRECTORY "${APPDIR}/usr/bin")
file(MAKE_DIRECTORY "${APPDIR}/usr/lib")
file(MAKE_DIRECTORY "${APPDIR}/usr/share/applications")
file(MAKE_DIRECTORY "${APPDIR}/usr/share/icons/hicolor/256x256/apps")

# Copy executable
file(COPY "${CPACK_PACKAGE_DIRECTORY}/bin/lumen-gui"
     DESTINATION "${APPDIR}/usr/bin")

# Copy desktop file
file(COPY "${CPACK_PACKAGE_DIRECTORY}/share/applications/lumen.desktop"
     DESTINATION "${APPDIR}/usr/share/applications")

# Copy icon
file(COPY "${CPACK_PACKAGE_DIRECTORY}/share/icons/hicolor/256x256/apps/lumen.png"
     DESTINATION "${APPDIR}/usr/share/icons/hicolor/256x256/apps")

# Create AppRun symlink
execute_process(
    COMMAND ${CMAKE_COMMAND} -E create_symlink usr/bin/lumen-gui AppRun
    WORKING_DIRECTORY "${APPDIR}"
)

# Create icon symlink at root
execute_process(
    COMMAND ${CMAKE_COMMAND} -E create_symlink usr/share/icons/hicolor/256x256/apps/lumen.png lumen.png
    WORKING_DIRECTORY "${APPDIR}"
)

# Create .desktop symlink at root
execute_process(
    COMMAND ${CMAKE_COMMAND} -E create_symlink usr/share/applications/lumen.desktop lumen.desktop
    WORKING_DIRECTORY "${APPDIR}"
)

# Run linuxdeploy with Qt plugin
if(LINUXDEPLOY_EXECUTABLE)
    # Find Qt plugin for linuxdeploy
    find_program(LINUXDEPLOY_QT_PLUGIN NAMES linuxdeploy-plugin-qt-x86_64.AppImage)

    execute_process(
        COMMAND ${LINUXDEPLOY_EXECUTABLE}
            --appdir "${APPDIR}"
            --output appimage
            $<$<BOOL:${LINUXDEPLOY_QT_PLUGIN}>:--plugin qt>
        WORKING_DIRECTORY "${CPACK_PACKAGE_DIRECTORY}"
        RESULT_VARIABLE LINUXDEPLOY_RESULT
    )

    if(NOT LINUXDEPLOY_RESULT EQUAL 0)
        message(WARNING "linuxdeploy failed with code ${LINUXDEPLOY_RESULT}")
    endif()

elseif(LINUXDEPLOYQT_EXECUTABLE)
    # Use linuxdeployqt (older tool, still works)
    execute_process(
        COMMAND ${LINUXDEPLOYQT_EXECUTABLE}
            "${APPDIR}/usr/share/applications/lumen.desktop"
            -appimage
            -qmake=${QT_QMAKE_EXECUTABLE}
        WORKING_DIRECTORY "${CPACK_PACKAGE_DIRECTORY}"
        RESULT_VARIABLE LINUXDEPLOYQT_RESULT
    )

    if(NOT LINUXDEPLOYQT_RESULT EQUAL 0)
        message(WARNING "linuxdeployqt failed with code ${LINUXDEPLOYQT_RESULT}")
    endif()
endif()

# Rename the output AppImage
file(GLOB APPIMAGE_FILES "${CPACK_PACKAGE_DIRECTORY}/*.AppImage")
foreach(APPIMAGE_FILE ${APPIMAGE_FILES})
    get_filename_component(APPIMAGE_NAME ${APPIMAGE_FILE} NAME)
    file(RENAME ${APPIMAGE_FILE}
         "${CPACK_PACKAGE_DIRECTORY}/${CPACK_PACKAGE_FILE_NAME}.AppImage")
endforeach()

message(STATUS "AppImage created: ${CPACK_PACKAGE_FILE_NAME}.AppImage")
