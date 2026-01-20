; ============================================================================
; Lumen NSIS Installer Script
; Target: Windows 11+
; ============================================================================

!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "x64.nsh"

; ============================================================================
; Installer Attributes
; ============================================================================

!ifndef VERSION
    !define VERSION "1.0.0"
!endif

!ifndef INSTALL_DIR
    !define INSTALL_DIR "install"
!endif

Name "Lumen ${VERSION}"
OutFile "Lumen-${VERSION}-Setup.exe"
InstallDir "$PROGRAMFILES64\Lumen"
InstallDirRegKey HKLM "Software\OAQL\Lumen" "InstallDir"
RequestExecutionLevel admin

; ============================================================================
; Version Information
; ============================================================================

VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "Lumen"
VIAddVersionKey "CompanyName" "OAQL Labs"
VIAddVersionKey "LegalCopyright" "Copyright (C) 2024 OAQL Labs"
VIAddVersionKey "FileDescription" "Lumen Portfolio Optimization Installer"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "ProductVersion" "${VERSION}"

; ============================================================================
; Modern UI Configuration
; ============================================================================

!define MUI_ABORTWARNING
!define MUI_ICON "${INSTALL_DIR}\bin\lumen.ico"
!define MUI_UNICON "${INSTALL_DIR}\bin\lumen.ico"

; Welcome page
!define MUI_WELCOMEPAGE_TITLE "Welcome to Lumen Setup"
!define MUI_WELCOMEPAGE_TEXT "This wizard will guide you through the installation of Lumen ${VERSION}, a quantum-enhanced portfolio optimization tool.$\r$\n$\r$\nSystem Requirements:$\r$\n- Windows 11 or later$\r$\n- 4 GB RAM minimum$\r$\n- 500 MB disk space$\r$\n$\r$\nClick Next to continue."

; Finish page
!define MUI_FINISHPAGE_RUN "$INSTDIR\bin\lumen-gui.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch Lumen"
!define MUI_FINISHPAGE_LINK "Visit Lumen Website"
!define MUI_FINISHPAGE_LINK_LOCATION "https://github.com/oaqlabs/lumen"

; ============================================================================
; Pages
; ============================================================================

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${INSTALL_DIR}\..\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; ============================================================================
; Languages
; ============================================================================

!insertmacro MUI_LANGUAGE "English"

; ============================================================================
; Functions
; ============================================================================

Function .onInit
    ; Check for Windows 11 (build 22000+)
    ${If} ${RunningX64}
        SetRegView 64
    ${Else}
        MessageBox MB_OK|MB_ICONSTOP "Lumen requires a 64-bit version of Windows."
        Abort
    ${EndIf}

    ; Check Windows version
    ReadRegStr $0 HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion" "CurrentBuild"
    ${If} $0 < 22000
        MessageBox MB_OK|MB_ICONEXCLAMATION "Lumen is designed for Windows 11 or later.$\r$\n$\r$\nYour version may work but is not officially supported."
    ${EndIf}
FunctionEnd

; ============================================================================
; Installer Section
; ============================================================================

Section "Lumen Application" SecMain
    SectionIn RO

    SetOutPath "$INSTDIR\bin"

    ; Copy application files
    File /r "${INSTALL_DIR}\bin\*.*"

    ; Copy license
    SetOutPath "$INSTDIR"
    File "${INSTALL_DIR}\..\LICENSE"
    File "${INSTALL_DIR}\..\README.md"

    ; Create uninstaller
    WriteUninstaller "$INSTDIR\Uninstall.exe"

    ; Registry entries
    WriteRegStr HKLM "Software\OAQL\Lumen" "InstallDir" "$INSTDIR"
    WriteRegStr HKLM "Software\OAQL\Lumen" "Version" "${VERSION}"

    ; Add to Add/Remove Programs
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Lumen" \
        "DisplayName" "Lumen ${VERSION}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Lumen" \
        "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Lumen" \
        "DisplayIcon" "$INSTDIR\bin\lumen-gui.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Lumen" \
        "Publisher" "OAQL Labs"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Lumen" \
        "DisplayVersion" "${VERSION}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Lumen" \
        "URLInfoAbout" "https://github.com/oaqlabs/lumen"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Lumen" \
        "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Lumen" \
        "NoRepair" 1

    ; Calculate installed size
    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    IntFmt $0 "0x%08X" $0
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Lumen" \
        "EstimatedSize" "$0"

    ; File associations for .lumen files
    WriteRegStr HKCR ".lumen" "" "LumenPortfolio"
    WriteRegStr HKCR "LumenPortfolio" "" "Lumen Portfolio File"
    WriteRegStr HKCR "LumenPortfolio\DefaultIcon" "" "$INSTDIR\bin\lumen-gui.exe,0"
    WriteRegStr HKCR "LumenPortfolio\shell\open\command" "" '"$INSTDIR\bin\lumen-gui.exe" "%1"'
SectionEnd

Section "Start Menu Shortcuts" SecShortcuts
    CreateDirectory "$SMPROGRAMS\Lumen"
    CreateShortcut "$SMPROGRAMS\Lumen\Lumen.lnk" "$INSTDIR\bin\lumen-gui.exe"
    CreateShortcut "$SMPROGRAMS\Lumen\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
SectionEnd

Section "Desktop Shortcut" SecDesktop
    CreateShortcut "$DESKTOP\Lumen.lnk" "$INSTDIR\bin\lumen-gui.exe"
SectionEnd

; ============================================================================
; Uninstaller Section
; ============================================================================

Section "Uninstall"
    ; Remove files
    RMDir /r "$INSTDIR\bin"
    Delete "$INSTDIR\LICENSE"
    Delete "$INSTDIR\README.md"
    Delete "$INSTDIR\Uninstall.exe"
    RMDir "$INSTDIR"

    ; Remove shortcuts
    Delete "$SMPROGRAMS\Lumen\Lumen.lnk"
    Delete "$SMPROGRAMS\Lumen\Uninstall.lnk"
    RMDir "$SMPROGRAMS\Lumen"
    Delete "$DESKTOP\Lumen.lnk"

    ; Remove registry entries
    DeleteRegKey HKLM "Software\OAQL\Lumen"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Lumen"

    ; Remove file associations
    DeleteRegKey HKCR ".lumen"
    DeleteRegKey HKCR "LumenPortfolio"
SectionEnd

; ============================================================================
; Section Descriptions
; ============================================================================

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecMain} "Core application files (required)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecShortcuts} "Add shortcuts to the Start Menu"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecDesktop} "Add a shortcut to the Desktop"
!insertmacro MUI_FUNCTION_DESCRIPTION_END
