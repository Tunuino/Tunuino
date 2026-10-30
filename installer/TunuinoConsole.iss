; Tunuino Console Windows setup (Inno Setup 6)
; Build after cmake --install has produced a deployable Qt folder.
; Example: iscc /DAppSource="C:\\tunuino-stage" /DAppVersion="0.1.0" installer\\TunuinoConsole.iss
#ifndef AppSource
  #error AppSource must point to the deployed Qt application directory
#endif
#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif

#define AppName "Tunuino Console"
#define AppExe "TunuinoConsole.exe"

[Setup]
AppId={{8E03B6E2-4A7C-4D70-A7D8-B7A34AB98524}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=Tunuino
DefaultDirName={autopf}\Tunuino Console
DefaultGroupName=Tunuino
OutputDir=output
OutputBaseFilename=Tunuino-Console-Setup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
UninstallDisplayIcon={app}\{#AppExe}
DisableProgramGroupPage=no
SetupLogging=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Optional branded artwork, if supplied in the future:
; WizardImageFile=assets\wizard-large.bmp
; WizardSmallImageFile=assets\wizard-small.bmp
LicenseFile=..\LICENSE

[Languages]
Name: "portuguese"; MessagesFile: "compiler:Languages\Portuguese.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#AppSource}\{#AppExe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#AppSource}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "{#AppExe},*.pdb"

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent

[Code]
function InitializeSetup(): Boolean;
begin
  Result := True;
end;
