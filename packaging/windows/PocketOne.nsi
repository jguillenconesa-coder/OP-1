Unicode True
; Official Windows NSIS packages omit amd64 stubs. The standard Unicode
; installer can install x64 payloads; opt into amd64 only where stubs exist.
!ifdef POCKET_AMD64_INSTALLER
Target amd64-unicode
!endif
!include "MUI2.nsh"
!include "x64.nsh"
!include "WinVer.nsh"
!ifndef PATH_SEP
  !define PATH_SEP "/"
!endif
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
  !define VERSION "0.8.0"
!endif
Name "Pocket One"
OutFile "${OUTPUT_DIR}${PATH_SEP}Pocket-One-${VERSION}-Windows-x64-Setup.exe"
InstallDir "$PROGRAMFILES64\Pocket Instruments\Pocket One"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
SetCompressorDictSize 32
VIProductVersion "${VERSION}.0"
VIAddVersionKey /LANG=1033 "ProductName" "Pocket One"
VIAddVersionKey /LANG=1033 "FileDescription" "Pocket One VST3 and standalone installer"
VIAddVersionKey /LANG=1033 "FileVersion" "${VERSION}"
VIAddVersionKey /LANG=1033 "LegalCopyright" "Pocket Instruments"
!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${SOURCE_DIR}${PATH_SEP}LICENSE"
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
  File /r "${ARTIFACT_DIR}${PATH_SEP}VST3${PATH_SEP}Pocket One.vst3${PATH_SEP}*.*"
SectionEnd
Section "Standalone instrument" Standalone
  SetOutPath "$INSTDIR"
  File "${ARTIFACT_DIR}${PATH_SEP}Standalone${PATH_SEP}Pocket One.exe"
  CreateDirectory "$SMPROGRAMS\Pocket One"
  CreateShortcut "$SMPROGRAMS\Pocket One\Pocket One.lnk" "$INSTDIR\Pocket One.exe"
SectionEnd
Section "Documentation and uninstaller" Required
  SectionIn RO
  SetOutPath "$INSTDIR"
  File "${SOURCE_DIR}${PATH_SEP}README.md"
  File "${SOURCE_DIR}${PATH_SEP}LICENSE"
  File "${SOURCE_DIR}${PATH_SEP}CREDITS.md"
  File "${SOURCE_DIR}${PATH_SEP}THIRD_PARTY_NOTICES.md"
  File /r "${SOURCE_DIR}${PATH_SEP}licenses"
  SetOutPath "$INSTDIR\Factory Samples"
  File /r "${SOURCE_DIR}${PATH_SEP}Assets${PATH_SEP}Factory${PATH_SEP}*.*"
  SetOutPath "$INSTDIR"
  File "${SOURCE_DIR}${PATH_SEP}docs${PATH_SEP}windows.md"
  File "${SOURCE_DIR}${PATH_SEP}docs${PATH_SEP}user-banks.md"
  File "${SOURCE_DIR}${PATH_SEP}docs${PATH_SEP}hardware-interface.md"
  File "${SOURCE_DIR}${PATH_SEP}docs${PATH_SEP}buttons-and-browser.md"
  File "${SOURCE_DIR}${PATH_SEP}docs${PATH_SEP}realtime-audio.md"
  File "${SOURCE_DIR}${PATH_SEP}docs${PATH_SEP}sound-browser.png"
  SetOutPath "$INSTDIR\docs"
  File /r "${SOURCE_DIR}${PATH_SEP}docs${PATH_SEP}*.*"
  SetOutPath "$INSTDIR\Source"
  File "${SOURCE_DIR}${PATH_SEP}CMakeLists.txt"
  File "${SOURCE_DIR}${PATH_SEP}README.md" "${SOURCE_DIR}${PATH_SEP}README.en.md" "${SOURCE_DIR}${PATH_SEP}LICENSE" "${SOURCE_DIR}${PATH_SEP}CREDITS.md" "${SOURCE_DIR}${PATH_SEP}THIRD_PARTY_NOTICES.md"
  ; Anchor each recursive wildcard inside its source directory. A bare /r
  ; directory name also matches nested build directories with that name.
  !macro PocketSourceFolder folder
    SetOutPath "$INSTDIR\Source\${folder}"
    File /r /x __pycache__ /x *.pyc "${SOURCE_DIR}${PATH_SEP}${folder}${PATH_SEP}*.*"
  !macroend
  !insertmacro PocketSourceFolder Source
  !insertmacro PocketSourceFolder Tests
  !insertmacro PocketSourceFolder Tools
  !insertmacro PocketSourceFolder cmake
  !insertmacro PocketSourceFolder scripts
  !insertmacro PocketSourceFolder docs
  !insertmacro PocketSourceFolder licenses
  !insertmacro PocketSourceFolder packaging
  SetOutPath "$INSTDIR\Source\research\blackfin"
  File /r "${SOURCE_DIR}${PATH_SEP}research${PATH_SEP}blackfin${PATH_SEP}*.*"
  SetOutPath "$INSTDIR"
  SetOutPath "$INSTDIR\Source\Assets\Factory"
  File /r "${SOURCE_DIR}${PATH_SEP}Assets${PATH_SEP}Factory${PATH_SEP}*.*"
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
  RMDir /r "$INSTDIR\Factory Samples"
  Delete "$INSTDIR\windows.md"
  Delete "$INSTDIR\user-banks.md"
  Delete "$INSTDIR\hardware-interface.md"
  Delete "$INSTDIR\buttons-and-browser.md"
  Delete "$INSTDIR\realtime-audio.md"
  Delete "$INSTDIR\sound-browser.png"
  RMDir /r "$INSTDIR\docs"
  Delete "$SMPROGRAMS\Pocket One\Pocket One.lnk"
  RMDir "$SMPROGRAMS\Pocket One"
  ; Remove only this application's installed source and plugin bundle.
  RMDir /r "$INSTDIR\Source"
  RMDir /r "$COMMONFILES64\VST3\Pocket One.vst3"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PocketOne"
SectionEnd
