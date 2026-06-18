; NSIS installer for the TekkChild TC-670 Vari-Mu Compressor.
; Built by CI on Windows (makensis) once the VST3 has been compiled; the VST3
; folder containing the .vst3 bundle is passed in with /DVST3DIR=... (it falls
; back to the local build tree so the script can also be test-compiled).

Unicode true

!include "MUI2.nsh"

!ifndef VST3DIR
  !define VST3DIR "..\build\TekkChild670_artefacts\Release\VST3"
!endif
!ifndef VERSION
  !define VERSION "2.0.0"
!endif

!define PRODUCT   "TekkChild TC-670 Vari-Mu Compressor"
!define COMPANY   "Tekk Engineering Audio Labs"
!define VST3NAME  "TekkChild TC-670 Vari-Mu Compressor.vst3"
!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\TekkChildTC670"

Name "${PRODUCT}"
OutFile "TekkChild-TC-670-Installer.exe"
InstallDir "$PROGRAMFILES64\${COMPANY}\TC-670"
RequestExecutionLevel admin
BrandingText "${COMPANY}  -  TC-670"

VIProductVersion "2.0.0.0"
VIAddVersionKey "ProductName" "${PRODUCT}"
VIAddVersionKey "CompanyName" "${COMPANY}"
VIAddVersionKey "FileDescription" "${PRODUCT} installer"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "LegalCopyright" "(c) ${COMPANY}"

;-------------------------------------------------------------- branding (mascot)
!define MUI_ICON   "assets\mascot.ico"
!define MUI_UNICON "assets\mascot.ico"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "assets\header.bmp"
!define MUI_HEADERIMAGE_RIGHT
!define MUI_WELCOMEFINISHPAGE_BITMAP   "assets\welcome.bmp"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "assets\welcome.bmp"
!define MUI_ABORTWARNING

!define MUI_WELCOMEPAGE_TITLE "TekkChild TC-670 Vari-Mu Compressor"
!define MUI_WELCOMEPAGE_TEXT "This installs the TekkChild TC-670 Vari-Mu Compressor (VST3) from Tekk Engineering Audio Labs, plus the bundled TEKK Engineering audio track.$\r$\n$\r$\nPlease close your DAW before continuing."
!define MUI_FINISHPAGE_TEXT "The TC-670 is installed to your VST3 folder. Rescan plugins in your DAW to find it.$\r$\n$\r$\nThe TEKK Engineering track was placed in your Documents."

;-------------------------------------------------------------- pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "LICENSE.txt"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

;-------------------------------------------------------------- sections
Section "TC-670 Plugin (VST3)" SecVST3
  SectionIn RO
  SetRegView 64

  ; the VST3 bundle goes to the shared VST3 location
  SetOutPath "$COMMONFILES64\VST3"
  File /r "${VST3DIR}\${VST3NAME}"

  ; uninstaller + Add/Remove Programs entry
  SetOutPath "$INSTDIR"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr   HKLM "${UNINST_KEY}" "DisplayName"     "${PRODUCT}"
  WriteRegStr   HKLM "${UNINST_KEY}" "Publisher"       "${COMPANY}"
  WriteRegStr   HKLM "${UNINST_KEY}" "DisplayVersion"  "${VERSION}"
  WriteRegStr   HKLM "${UNINST_KEY}" "DisplayIcon"     "$INSTDIR\mascot.ico"
  WriteRegStr   HKLM "${UNINST_KEY}" "UninstallString" "$\"$INSTDIR\Uninstall.exe$\""
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoRepair" 1
  File "assets\mascot.ico"
SectionEnd

Section "TEKK Engineering audio track" SecAudio
  SetOutPath "$DOCUMENTS\${COMPANY}\TC-670"
  File "assets\TEKK_Engineering_TC-670.mp3"
SectionEnd

;-------------------------------------------------------------- component blurbs
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecVST3}  "The TC-670 Vari-Mu Compressor plugin (VST3), installed for every DAW."
  !insertmacro MUI_DESCRIPTION_TEXT ${SecAudio} "The bundled TEKK Engineering audio track (MP3), placed in your Documents."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

;-------------------------------------------------------------- uninstall
Section "Uninstall"
  SetRegView 64
  RMDir /r "$COMMONFILES64\VST3\${VST3NAME}"
  Delete "$DOCUMENTS\${COMPANY}\TC-670\TEKK_Engineering_TC-670.mp3"
  RMDir  "$DOCUMENTS\${COMPANY}\TC-670"
  RMDir  "$DOCUMENTS\${COMPANY}"
  Delete "$INSTDIR\mascot.ico"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir  "$INSTDIR"
  RMDir  "$PROGRAMFILES64\${COMPANY}"
  DeleteRegKey HKLM "${UNINST_KEY}"
SectionEnd
