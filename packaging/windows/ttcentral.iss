; TT Central — Windows installer (Inno Setup 6).
; SPDX-License-Identifier: GPL-3.0-or-later
;
; Build the package folder first (python scripts/package.py), then:
;   iscc /DSourceDir=dist\TTCentral-<ver>-win64 /DAppVersion=<ver> /DOutputDir=dist packaging\windows\ttcentral.iss
;
; Installs per user without administrator rights by default (the setup can
; still install for all users). Settings stay in %APPDATA%\TTCentral and are
; kept on uninstall.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef SourceDir
  #error Pass /DSourceDir=<package folder>
#endif
#ifndef OutputDir
  #define OutputDir "."
#endif

[Setup]
AppId={{6B0E1F4C-7D2A-4C55-9E2B-5B8A7A3D2C11}
AppName=TT Central
AppVersion={#AppVersion}
AppVerName=TT Central {#AppVersion}
AppPublisher=TT Central contributors
AppPublisherURL=https://github.com/donovanmalisch23-alt/TT_Central
AppSupportURL=https://github.com/donovanmalisch23-alt/TT_Central/issues
DefaultDirName={autopf}\TT Central
DefaultGroupName=TT Central
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog commandline
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename=TTCentral-{#AppVersion}-setup
Compression=lzma2/ultra64
SolidCompression=yes
LicenseFile={#SourceDir}\LICENSE.txt
InfoBeforeFile={#SourceDir}\NOTICE.txt
UninstallDisplayIcon={app}\TTCentral.exe
WizardStyle=modern
CloseApplications=yes

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Excludes: "TTCentral.ini"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\TT Central"; Filename: "{app}\TTCentral.exe"; WorkingDir: "{app}"
Name: "{group}\TT Central User Guide"; Filename: "{app}\User Guide.html"; Check: FileExists(ExpandConstant('{app}\User Guide.html'))
Name: "{group}\Uninstall TT Central"; Filename: "{uninstallexe}"
Name: "{autodesktop}\TT Central"; Filename: "{app}\TTCentral.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\TTCentral.exe"; Description: "{cm:LaunchProgram,TT Central}"; WorkingDir: "{app}"; Flags: postinstall nowait skipifsilent

[UninstallRun]
; Stop a media bot or local server that is still running from this folder.
Filename: "{sys}\taskkill.exe"; Parameters: "/F /IM ttc-mediabot.exe"; Flags: runhidden; RunOnceId: "StopMediaBot"
