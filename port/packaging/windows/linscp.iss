; linscp's Windows installer, for Inno Setup 6 (port/tools/fetch_innosetup.py),
; laid out as WinSCP's (deployment/winscpsetup.iss): installed for all users
; or, as chosen, only the current one; the program and its translations as
; components; a desktop icon, the Send To hook, the URL handler and the
; search path as tasks, done by the program itself, as WinSCP's maintenance
; tasks are. port/packaging/CMakeLists.txt compiles it, target installer:
;
;   ISCC /DAppVersion=<x.y.z> /DSourceDir=<deploy> /DLicensesDir=<dir>
;        /DOutputDir=<dir> linscp.iss

#ifndef AppVersion
  #error AppVersion is not defined
#endif
#ifndef SourceDir
  #error SourceDir (the deploy folder) is not defined
#endif
#ifndef LicensesDir
  #error LicensesDir is not defined
#endif
#ifndef OutputDir
  #define OutputDir "."
#endif

#define AppName "linscp"
#define AppExe "linscp.exe"

[Setup]
AppId={{6C1E36A4-5B9F-4B8C-9C2E-3F0A7D1E8B52}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppName}
AppPublisherURL=https://github.com/99ecarvalho/linscp
AppSupportURL=https://github.com/99ecarvalho/linscp/issues
AppUpdatesURL=https://github.com/99ecarvalho/linscp/releases
VersionInfoVersion={#AppVersion}
VersionInfoDescription={#AppName} Setup
DefaultDirName={autopf}\{#AppName}
DisableProgramGroupPage=yes
UsePreviousAppDir=yes
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=commandline dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
WizardStyle=modern dynamic
SetupIconFile=..\..\qt\icons\linscp.ico
UninstallDisplayIcon={app}\{#AppExe}
UninstallDisplayName={#AppName}
LicenseFile={#LicensesDir}\LICENSE
OutputDir={#OutputDir}
OutputBaseFilename={#AppName}-{#AppVersion}-setup
Compression=lzma2/max
SolidCompression=yes
CloseApplications=yes
ChangesAssociations=yes
ChangesEnvironment=yes
ShowLanguageDialog=auto

[Languages]
Name: en; MessagesFile: "compiler:Default.isl"
Name: ar; MessagesFile: "compiler:Languages\Arabic.isl"
Name: hy; MessagesFile: "compiler:Languages\Armenian.isl"
Name: ptbr; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
Name: bg; MessagesFile: "compiler:Languages\Bulgarian.isl"
Name: ca; MessagesFile: "compiler:Languages\Catalan.isl"
Name: co; MessagesFile: "compiler:Languages\Corsican.isl"
Name: cs; MessagesFile: "compiler:Languages\Czech.isl"
Name: da; MessagesFile: "compiler:Languages\Danish.isl"
Name: nl; MessagesFile: "compiler:Languages\Dutch.isl"
Name: fi; MessagesFile: "compiler:Languages\Finnish.isl"
Name: fr; MessagesFile: "compiler:Languages\French.isl"
Name: de; MessagesFile: "compiler:Languages\German.isl"
Name: he; MessagesFile: "compiler:Languages\Hebrew.isl"
Name: hu; MessagesFile: "compiler:Languages\Hungarian.isl"
Name: it; MessagesFile: "compiler:Languages\Italian.isl"
Name: ja; MessagesFile: "compiler:Languages\Japanese.isl"
Name: ko; MessagesFile: "compiler:Languages\Korean.isl"
Name: no; MessagesFile: "compiler:Languages\Norwegian.isl"
Name: pl; MessagesFile: "compiler:Languages\Polish.isl"
Name: pt; MessagesFile: "compiler:Languages\Portuguese.isl"
Name: ru; MessagesFile: "compiler:Languages\Russian.isl"
Name: sk; MessagesFile: "compiler:Languages\Slovak.isl"
Name: sl; MessagesFile: "compiler:Languages\Slovenian.isl"
Name: es; MessagesFile: "compiler:Languages\Spanish.isl"
Name: sv; MessagesFile: "compiler:Languages\Swedish.isl"
Name: ta; MessagesFile: "compiler:Languages\Tamil.isl"
Name: th; MessagesFile: "compiler:Languages\Thai.isl"
Name: tr; MessagesFile: "compiler:Languages\Turkish.isl"
Name: uk; MessagesFile: "compiler:Languages\Ukrainian.isl"

[CustomMessages]
; English for every installer language: linscp has no translations of its
; own installer's few texts yet.
ApplicationComponent=Application
TranslationsComponent=Translations
DesktopIconTask=Create a &desktop icon
SendToHookTask=Add upload shortcut to Explorer's '&Send to' context menu
SendToHookName={#AppName} (for upload)
UrlHandlerTask=Register to handle sftp://, scp://, ftp://, ftps://, dav://, s3:// and other URLs
SearchPathTask=Add installation directory to &search path (%PATH%)
LaunchProgram=Launch {#AppName}

[Types]
Name: full; Description: "{code:TypeDescription|full}"
Name: compact; Description: "{code:TypeDescription|compact}"
Name: custom; Description: "{code:TypeDescription|custom}"; Flags: iscustom

[Components]
Name: main; Description: "{cm:ApplicationComponent}"; Types: full compact custom; Flags: fixed
Name: transl; Description: "{cm:TranslationsComponent}"; Types: full

[Tasks]
Name: desktopicon; Description: "{cm:DesktopIconTask}"; Flags: unchecked
Name: sendtohook; Description: "{cm:SendToHookTask}"
Name: urlhandler; Description: "{cm:UrlHandlerTask}"
; As WinSCP's: the system path, so for all users only.
Name: searchpath; Description: "{cm:SearchPathTask}"; Flags: unchecked; Check: IsAdminInstallMode

[Files]
Source: "{#SourceDir}\*"; Excludes: "\Translations"; DestDir: "{app}"; Components: main; \
  Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\Translations\*"; DestDir: "{app}\Translations"; Components: transl; \
  Flags: ignoreversion
Source: "{#LicensesDir}\*"; DestDir: "{app}\licenses"; Components: main; Flags: ignoreversion

[InstallDelete]
; Translations deselected on an upgrade.
Type: filesandordirs; Name: "{app}\Translations"; Components: not transl

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExe}"; Components: main
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon
Name: "{usersendto}\{cm:SendToHookName}"; Filename: "{app}\{#AppExe}"; Parameters: "/upload"; \
  Tasks: sendtohook

[Run]
Filename: "{app}\{#AppExe}"; Parameters: "/RegisterForDefaultProtocols"; Tasks: urlhandler; \
  StatusMsg: "{cm:UrlHandlerTask}"
Filename: "{app}\{#AppExe}"; Parameters: "/AddSearchPath"; Tasks: searchpath; \
  StatusMsg: "{cm:SearchPathTask}"
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchProgram}"; \
  Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "{app}\{#AppExe}"; Parameters: "/RemoveSearchPath"; RunOnceId: "RemoveSearchPath"
Filename: "{app}\{#AppExe}"; Parameters: "/UnregisterForProtocols"; RunOnceId: "UnregisterForProtocols"

[Code]
function TypeDescription(Name: string): string;
begin
  // Inno's own messages name the types; WinSCP's do, as its script.
  if Name = 'full' then Result := SetupMessage(msgFullInstallation)
  else if Name = 'compact' then Result := SetupMessage(msgCompactInstallation)
  else Result := SetupMessage(msgCustomInstallation);
end;
