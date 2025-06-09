; QUIET NSIS Installer Script
; This script creates a professional Windows installer for QUIET

!define PRODUCT_NAME "QUIET"
!define PRODUCT_VERSION "1.0.0"
!define PRODUCT_PUBLISHER "QUIET Development Team"
!define PRODUCT_WEB_SITE "https://quietaudio.com"
!define PRODUCT_SUPPORT "https://quietaudio.com/support"
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\Quiet.exe"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"

SetCompressor lzma

; MUI Settings
!include "MUI2.nsh"
!include "x64.nsh"

!define MUI_ABORTWARNING
!define MUI_ICON "..\..\resources\icons\icon.ico"
!define MUI_UNICON "..\..\resources\icons\icon.ico"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "header.bmp"
!define MUI_WELCOMEFINISHPAGE_BITMAP "welcome.bmp"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "welcome.bmp"

; Welcome page
!insertmacro MUI_PAGE_WELCOME
; License page
!insertmacro MUI_PAGE_LICENSE "..\..\LICENSE"
; Components page
!insertmacro MUI_PAGE_COMPONENTS
; Directory page
!insertmacro MUI_PAGE_DIRECTORY
; Instfiles page
!insertmacro MUI_PAGE_INSTFILES
; Finish page
!define MUI_FINISHPAGE_RUN "$INSTDIR\Quiet.exe"
!define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\README.md"
!define MUI_FINISHPAGE_LINK "Visit QUIET website"
!define MUI_FINISHPAGE_LINK_LOCATION "${PRODUCT_WEB_SITE}"
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

; Language files
!insertmacro MUI_LANGUAGE "English"

; Product information
Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "QUIET-${PRODUCT_VERSION}-Windows-Setup.exe"
InstallDir "$PROGRAMFILES64\QUIET"
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""
ShowInstDetails show
ShowUnInstDetails show
RequestExecutionLevel admin

; Version information
VIProductVersion "${PRODUCT_VERSION}.0"
VIAddVersionKey "ProductName" "${PRODUCT_NAME}"
VIAddVersionKey "ProductVersion" "${PRODUCT_VERSION}"
VIAddVersionKey "CompanyName" "${PRODUCT_PUBLISHER}"
VIAddVersionKey "LegalCopyright" "Copyright (c) 2025 ${PRODUCT_PUBLISHER}"
VIAddVersionKey "FileDescription" "${PRODUCT_NAME} Installer"
VIAddVersionKey "FileVersion" "${PRODUCT_VERSION}"

; Check for 64-bit Windows
Function .onInit
  ${If} ${RunningX64}
    SetRegView 64
  ${Else}
    MessageBox MB_OK|MB_ICONSTOP "QUIET requires a 64-bit version of Windows."
    Abort
  ${EndIf}
  
  ; Check for Windows 10 or later
  ${If} ${AtLeastWin10}
    ; OK
  ${Else}
    MessageBox MB_OK|MB_ICONSTOP "QUIET requires Windows 10 or later."
    Abort
  ${EndIf}
FunctionEnd

Section "QUIET Core Application (required)" SEC_CORE
  SectionIn RO
  
  SetOutPath "$INSTDIR"
  SetOverwrite ifnewer
  
  ; Install main executable
  File "..\..\build\Release\Quiet.exe"
  
  ; Install dependencies
  File "..\..\build\Release\*.dll"
  
  ; Install resources
  SetOutPath "$INSTDIR\resources"
  File /r "..\..\resources\*.*"
  
  ; Install documentation
  SetOutPath "$INSTDIR"
  File "..\..\README.md"
  File "..\..\LICENSE"
  
  ; Store installation folder
  WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR\Quiet.exe"
  
  ; Create uninstaller
  WriteUninstaller "$INSTDIR\uninstall.exe"
  
  ; Write uninstall information
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName" "${PRODUCT_NAME}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninstall.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\Quiet.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "HelpLink" "${PRODUCT_SUPPORT}"
  
  ; Estimate size
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "EstimatedSize" "$0"
SectionEnd

Section "Start Menu Shortcuts" SEC_SHORTCUTS
  CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk" "$INSTDIR\Quiet.exe"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Uninstall ${PRODUCT_NAME}.lnk" "$INSTDIR\uninstall.exe"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME} Website.lnk" "${PRODUCT_WEB_SITE}"
SectionEnd

Section "Desktop Shortcut" SEC_DESKTOP
  CreateShortCut "$DESKTOP\${PRODUCT_NAME}.lnk" "$INSTDIR\Quiet.exe"
SectionEnd

Section "Install VB-Cable (recommended)" SEC_VBCABLE
  MessageBox MB_YESNO|MB_ICONQUESTION "QUIET requires VB-Audio Virtual Cable for virtual device functionality.$\r$\n$\r$\nWould you like to download and install it now?$\r$\n$\r$\n(This will open your web browser)" IDNO skip_vbcable
  ExecShell "open" "https://vb-audio.com/Cable/index.htm"
  
  MessageBox MB_OK|MB_ICONINFORMATION "Please install VB-Cable and then click OK to continue QUIET installation."
  skip_vbcable:
SectionEnd

; Component descriptions
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_CORE} "The main QUIET application files (required)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_SHORTCUTS} "Add shortcuts to your Start Menu"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_DESKTOP} "Add a shortcut to your Desktop"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_VBCABLE} "Download and install VB-Audio Virtual Cable driver"
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Section Uninstall
  ; Remove files
  Delete "$INSTDIR\Quiet.exe"
  Delete "$INSTDIR\*.dll"
  Delete "$INSTDIR\README.md"
  Delete "$INSTDIR\LICENSE"
  Delete "$INSTDIR\uninstall.exe"
  
  ; Remove directories
  RMDir /r "$INSTDIR\resources"
  RMDir "$INSTDIR"
  
  ; Remove shortcuts
  Delete "$SMPROGRAMS\${PRODUCT_NAME}\*.*"
  RMDir "$SMPROGRAMS\${PRODUCT_NAME}"
  Delete "$DESKTOP\${PRODUCT_NAME}.lnk"
  
  ; Remove registry entries
  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
  DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"
  
  ; Remove app data
  MessageBox MB_YESNO|MB_ICONQUESTION "Do you want to remove QUIET configuration and preferences?" IDNO skip_appdata
  RMDir /r "$APPDATA\QUIET"
  skip_appdata:
  
  SetAutoClose true
SectionEnd

; Installation success callback
Function .onInstSuccess
  MessageBox MB_YESNO|MB_ICONQUESTION "Installation complete!$\r$\n$\r$\nWould you like to run QUIET now?" IDNO +2
  Exec "$INSTDIR\Quiet.exe"
FunctionEnd