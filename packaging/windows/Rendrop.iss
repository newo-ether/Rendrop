#ifndef AppVersion
  #error Pass /DAppVersion=<version>
#endif
#ifndef PackageDir
  #error Pass /DPackageDir=<deployed application directory>
#endif
#ifndef ArtifactDir
  #define ArtifactDir "Output"
#endif

[Setup]
AppName=Rendrop
AppVersion={#AppVersion}
DefaultDirName={commonpf}\Rendrop
DefaultGroupName=Rendrop
DisableProgramGroupPage=no
AllowNoIcons=yes
OutputDir={#ArtifactDir}
OutputBaseFilename=Rendrop_{#AppVersion}_windows_x64_Setup
Compression=lzma
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64os
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "Additional icons:"; Flags: unchecked

[Files]
Source: "{#PackageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Rendrop"; Filename: "{app}\Rendrop.exe"
Name: "{commondesktop}\Rendrop"; Filename: "{app}\Rendrop.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Rendrop.exe"; Description: "Launch Rendrop"; Flags: nowait postinstall skipifsilent
