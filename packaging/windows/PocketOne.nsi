Unicode True
; Official Windows NSIS packages omit amd64 stubs. The standard Unicode
; installer can install x64 payloads; opt into amd64 only where stubs exist.
!ifdef POCKET_AMD64_INSTALLER
Target amd64-unicode
!endif
!include "MUI2.nsh"
!include "x64.nsh"
!include "WinVer.nsh"
!ifndef ARTIFACT_DIR
  !error "Define ARTIFACT_DIR with the Release artefacts directory"
!endif
!ifndef SOURCE_DIR
  !error "Define SOURCE_DIR with the project source directory"
!endif
!ifndef OUTPUT_DIR
  !define OUTPUT_DIR "."
!endif
!ifndef VERSION
  !define VERSION "0.2.1"
!endif
Name "Pocket One"
OutFile "${OUTPUT_DIR}/Pocket-One-${VERSION}-Windows-x64-Setup.exe"
InstallDir "$PROGRAMFILES64\Pocket Instruments\Pocket One"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
SetCompressorDictSize 32
VIProductVersion "0.2.1.0"
VIAddVersionKey /LANG=1033 "ProductName" "Pocket One"
VIAddVersionKey /LANG=1033 "FileDescription" "Pocket One VST3 and standalone installer"
VIAddVersionKey /LANG=1033 "FileVersion" "${VERSION}"
VIAddVersionKey /LANG=1033 "LegalCopyright" "Pocket Instruments"
!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${SOURCE_DIR}/LICENSE"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "Spanish"
Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "Pocket One requires 64-bit Windows."
    Abort
  ${EndIf}
  ${IfNot} ${AtLeastWin10}
    MessageBox MB_ICONSTOP "Pocket One requires Windows 10 or later."
    Abort
  ${EndIf}
  SetRegView 64
  SetShellVarContext all
FunctionEnd
Section "VST3 instrument (DAW)" VST3
  SetOutPath "$COMMONFILES64\VST3\Pocket One.vst3"
  File /r "${ARTIFACT_DIR}/VST3/Pocket One.vst3/*.*"
SectionEnd
Section "Standalone instrument" Standalone
  SetOutPath "$INSTDIR"
  File "${ARTIFACT_DIR}/Standalone/Pocket One.exe"
  CreateDirectory "$SMPROGRAMS\Pocket One"
  CreateShortcut "$SMPROGRAMS\Pocket One\Pocket One.lnk" "$INSTDIR\Pocket One.exe"
SectionEnd
Section "Documentation and uninstaller" Required
  SectionIn RO
  SetOutPath "$INSTDIR"
  File "${SOURCE_DIR}/README.md"
  File "${SOURCE_DIR}/LICENSE"
  File "${SOURCE_DIR}/CREDITS.md"
  File "${SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
  File /r "${SOURCE_DIR}/licenses"
  File "${SOURCE_DIR}/docs/windows.md"
  SetOutPath "$INSTDIR\Source"
  File "${SOURCE_DIR}/CMakeLists.txt"
  File /r "${SOURCE_DIR}/Source" "${SOURCE_DIR}/Tests" "${SOURCE_DIR}/cmake"
  SetOutPath "$INSTDIR"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PocketOne" "DisplayName" "Pocket One"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PocketOne" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PocketOne" "Publisher" "Pocket Instruments"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PocketOne" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PocketOne" "UninstallString" '$\"$INSTDIR\Uninstall.exe$\"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PocketOne" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PocketOne" "NoRepair" 1
SectionEnd
Function un.onInit
  SetRegView 64
  SetShellVarContext all
FunctionEnd
Section "Uninstall"
  Delete "$INSTDIR\Pocket One.exe"
  Delete "$INSTDIR\README.md"
  Delete "$INSTDIR\LICENSE"
  Delete "$INSTDIR\CREDITS.md"
  Delete "$INSTDIR\THIRD_PARTY_NOTICES.md"
  RMDir /r "$INSTDIR\licenses"
  Delete "$INSTDIR\windows.md"
  Delete "$SMPROGRAMS\Pocket One\Pocket One.lnk"
  RMDir "$SMPROGRAMS\Pocket One"
  ; Remove only this application's installed source and plugin bundle.
  RMDir /r "$INSTDIR\Source"
  RMDir /r "$COMMONFILES64\VST3\Pocket One.vst3"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PocketOne"
SectionEnd
