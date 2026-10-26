# Copyright (c) 2026 Tunuino.
# Real Windows Installer lifecycle test on a disposable hosted runner; no ECU required.
param([Parameter(Mandatory)][string]$Previous,
      [Parameter(Mandatory)][string]$Installer,
      [Parameter(Mandatory)][string]$Version)
$ErrorActionPreference = 'Stop'
$work = Join-Path $env:RUNNER_TEMP 'Tunuino installer checks'
New-Item -ItemType Directory -Force $work | Out-Null
$install = Join-Path $work 'Application with spaces'
$data = Join-Path $env:LOCALAPPDATA 'TunuinoConsole'
$seeds = @{}
foreach ($name in @('maps/installer-check.tnmap', 'configurations/installer-check.tncfg', 'logs/installer-check.csv', 'programmers.properties')) {
    $file = Join-Path $data $name
    if (Test-Path $file) { throw "Refusing to overwrite existing user file: $file" }
    New-Item -ItemType Directory -Force (Split-Path $file) | Out-Null
    [IO.File]::WriteAllText($file, "Tunuino preservation check: $name")
    $seeds[$file] = (Get-FileHash $file -Algorithm SHA256).Hash
}
function Assert-UserData {
    foreach ($file in $seeds.Keys) {
        if (-not (Test-Path $file) -or (Get-FileHash $file -Algorithm SHA256).Hash -ne $seeds[$file]) {
            throw "Installer changed user file: $file"
        }
    }
}
function Invoke-Msi([string]$arguments, [string]$label) {
    $log = Join-Path $work "$label.log"
    $p = Start-Process msiexec.exe -ArgumentList "$arguments /qn /norestart /l*v `"$log`"" -PassThru
    if (-not $p.WaitForExit(180000)) { $p.Kill(); throw "MSI $label timed out" }
    $p.Refresh()
    if ($p.ExitCode -notin @(0, 3010)) {
        Get-Content $log -Tail 120
        throw "MSI $label failed: $($p.ExitCode)"
    }
}
function Registrations {
    @(Get-ChildItem 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall' -ErrorAction SilentlyContinue |
      Get-ItemProperty | Where-Object DisplayName -eq 'TunuinoConsole')
}
function Assert-Payload {
    $manifest = Get-Content (Join-Path $install 'installation-manifest.json') -Raw | ConvertFrom-Json
    if ($manifest.source_commit -ne $env:GITHUB_SHA) { throw 'Installed source commit mismatch' }
    foreach ($entry in $manifest.files_sha256.PSObject.Properties) {
        $file = Join-Path $install $entry.Name
        if (-not (Test-Path $file) -or (Get-FileHash $file -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.Value) {
            throw "Installed payload differs from validated kit: $($entry.Name)"
        }
    }
    $desktop = Join-Path ([Environment]::GetFolderPath('Desktop')) 'TunuinoConsole.lnk'
    $menu = Join-Path ([Environment]::GetFolderPath('Programs')) 'Tunuino/TunuinoConsole.lnk'
    $shell = New-Object -ComObject WScript.Shell
    foreach ($link in @($desktop, $menu)) {
        if (-not (Test-Path $link)) { throw "Shortcut missing: $link" }
        if ($shell.CreateShortcut($link).TargetPath -ne (Join-Path $install 'TunuinoConsole.exe')) {
            throw "Shortcut targets a different application: $link"
        }
    }
}
function Assert-Runtime {
    $exe = Join-Path $install 'TunuinoConsole.exe'
    $out = Join-Path $work 'installed-runtime.txt'
    $err = Join-Path $work 'installed-runtime-errors.txt'
    # A Start menu launch does not necessarily use the application directory as cwd.
    $p = Start-Process $exe -ArgumentList '--verify-installation' -WorkingDirectory $work -PassThru -RedirectStandardOutput $out -RedirectStandardError $err
    if (-not $p.WaitForExit(60000)) { $p.Kill(); throw 'Installed runtime timed out' }
    $p.Refresh()
    Get-Content $out
    Get-Content $err
    if ($p.ExitCode -ne 0) { throw "Installed runtime failed: $($p.ExitCode)" }
}
try {
    if (@(Registrations).Count -ne 0) { throw 'TunuinoConsole is already installed on this runner' }
    Invoke-Msi "/i `"$Previous`" INSTALLDIR=`"$install`"" 'install-previous'
    Assert-Payload
    Assert-Runtime
    Assert-UserData
    Invoke-Msi "/i `"$Installer`" INSTALLDIR=`"$install`"" 'upgrade'
    Assert-Payload
    Assert-Runtime
    Assert-UserData
    $registered = @(Registrations)
    if ($registered.Count -ne 1 -or $registered[0].DisplayVersion -ne $Version) {
        throw 'Upgrade did not replace the previous installed version'
    }
    Invoke-Msi "/x $($registered[0].PSChildName)" 'uninstall'
    if (Test-Path (Join-Path $install 'TunuinoConsole.exe')) { throw 'Uninstaller left application executable' }
    if (@(Registrations).Count -ne 0) { throw 'Uninstaller left application registered' }
    foreach ($link in @((Join-Path ([Environment]::GetFolderPath('Desktop')) 'TunuinoConsole.lnk'),
                        (Join-Path ([Environment]::GetFolderPath('Programs')) 'Tunuino/TunuinoConsole.lnk'))) {
        if (Test-Path $link) { throw "Uninstaller left shortcut: $link" }
    }
    Assert-UserData
    # Also exercise a clean install of the exact MSI that will be published.
    Invoke-Msi "/i `"$Installer`" INSTALLDIR=`"$install`"" 'install-release'
    Assert-Payload
    Assert-Runtime
    Assert-UserData
    $registered = @(Registrations)
    if ($registered.Count -ne 1 -or $registered[0].DisplayVersion -ne $Version) { throw 'Release registration mismatch' }
    Invoke-Msi "/x $($registered[0].PSChildName)" 'uninstall-release'
    Assert-UserData
    Write-Output 'Installation, installed payload/runtime, shortcuts, upgrade, uninstallation and user-file preservation passed'
} finally {
    foreach ($registered in @(Registrations)) { Invoke-Msi "/x $($registered.PSChildName)" 'cleanup' }
}
