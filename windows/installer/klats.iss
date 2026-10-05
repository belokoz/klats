; Installer of Klats for Windows: per user, no administrator rights, Russian or English by the
; Windows display language. Built by build.ps1 -Installer, which passes the version from
; CMakeLists.txt and the folder of the built Klats.exe:
;   iscc /DAppVersion=0.1.0 /DBuildDir=..\build\release klats.iss
#ifndef AppVersion
  #error Pass the version: iscc /DAppVersion=X.Y.Z klats.iss
#endif
#ifndef BuildDir
  #define BuildDir "..\build\release"
#endif

[Setup]
; Never change the id: Windows knows an installed Klats by it, and an update installs over it.
AppId={{D3B4F077-BB8A-47FD-BE3B-6A07FB3BE2DE}
AppName={cm:AppName}
AppVersion={#AppVersion}
AppVerName={cm:AppName} {#AppVersion}
AppPublisher=belokoz
AppPublisherURL=https://github.com/belokoz/klats
AppSupportURL=https://github.com/belokoz/klats/issues
AppUpdatesURL=https://github.com/belokoz/klats/releases
VersionInfoVersion={#AppVersion}
VersionInfoProductName=Klats
VersionInfoDescription=Klats Setup
; %LOCALAPPDATA%\Programs\Klats and the user's Start menu: installing needs no administrator.
PrivilegesRequired=lowest
DefaultDirName={autopf}\Klats
DisableDirPage=yes
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Windows 10 1903: the first with icu.dll.
MinVersion=10.0.18362
OutputDir={#BuildDir}
OutputBaseFilename=Klats-{#AppVersion}-windows-x64
SetupIconFile=..\res\klats.ico
UninstallDisplayIcon={app}\Klats.exe
UninstallDisplayName={cm:AppName}
WizardStyle=modern
Compression=lzma2/max
SolidCompression=yes
ShowLanguageDialog=no
LanguageDetectionMethod=uilanguage
; Klats is closed by the code below. Restart Manager only steps in if that fails.
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "ru"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
ru.AppName=Клац
en.AppName=Klats

[Files]
Source: "{#BuildDir}\Klats.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{cm:AppName}"; Filename: "{app}\Klats.exe"

[Run]
; Checked on the last page; a silent install starts Klats too, as an update from inside Klats needs.
Filename: "{app}\Klats.exe"; Description: "{cm:LaunchProgram,{cm:AppName}}"; Flags: nowait postinstall

[UninstallDelete]
; The log. The settings in HKCU\Software\Klats stay, as on the Mac: a reinstall keeps them.
Type: filesandordirs; Name: "{localappdata}\Klats"

[Code]
const
  WM_CLOSE = $0010;

// A running Klats is asked to quit the way its own menu does it: a conversion in flight puts the
// user's clipboard back first. Any Klats is closed, the installed one or a copy run from elsewhere.
procedure CloseKlats();
var
  Attempts: Integer;
begin
  if FindWindowByClassName('KlatsTrayOwner') = 0 then Exit;
  PostMessage(FindWindowByClassName('KlatsTrayOwner'), WM_CLOSE, 0, 0);
  Attempts := 0;
  while (FindWindowByClassName('KlatsTrayOwner') <> 0) and (Attempts < 100) do
  begin
    Sleep(100);
    Attempts := Attempts + 1;
  end;
  // The window goes last thing before the process ends: give the exe a moment to be released.
  Sleep(300);
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  CloseKlats();
  Result := '';
end;

function InitializeUninstall(): Boolean;
begin
  CloseKlats();
  Result := True;
end;

// Klats writes its autostart itself; nothing may start a deleted exe at the next sign-in.
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
  begin
    RegDeleteValue(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Run', 'Klats');
    RegDeleteValue(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Explorer\StartupApproved\Run', 'Klats');
  end;
end;
