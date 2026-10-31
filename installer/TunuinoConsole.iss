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
SetupIconFile=..\qt_console\assets\tunuino.ico
DisableProgramGroupPage=no
SetupLogging=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardImageFile=assets\wizard-large.bmp
WizardSmallImageFile=assets\wizard-small.bmp
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
// The built-in language selector is shown before the regular wizard.
// Its default icon is only 32x32; enlarge the actual selector layout here.
function InitializeLanguageDialog(): Boolean;
begin
  with SelectLanguageForm do
  begin
    ClientWidth := ScaleX(560);
    ClientHeight := ScaleY(240);

    IconBitmapImage.SetBounds(ScaleX(18), ScaleY(18), ScaleX(175), ScaleY(110));
    IconBitmapImage.Stretch := True;

    SelectLabel.SetBounds(ScaleX(215), ScaleY(28), ScaleX(325), ScaleY(55));
    LangCombo.SetBounds(ScaleX(215), ScaleY(100), ScaleX(325), LangCombo.Height);

    OKButton.SetBounds(ScaleX(340), ScaleY(192), ScaleX(95), OKButton.Height);
    CancelButton.SetBounds(ScaleX(445), ScaleY(192), ScaleX(95), CancelButton.Height);
  end;
  Result := True;
end;
