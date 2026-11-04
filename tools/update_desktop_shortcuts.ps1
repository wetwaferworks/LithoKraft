# update_desktop_shortcuts.ps1 - points the LithoKraft Play Build / Backup desktop shortcuts at play builds.
# No --display flag: the saved display mode wins. Working dir = exe folder. Old .lnk backed up as .orig.
param(
    [string]$PlayExe = '',
    [string]$BackupExe = '',
    [string]$BuildRoot = 'D:\scratch\th-work\build',
    [string]$Desktop = ''
)
Set-StrictMode -Version 2
$ErrorActionPreference = 'Stop'

class PlayBuildFinder {
    [string]$Root
    PlayBuildFinder([string]$root) { $this.Root = $root }
    [System.IO.FileInfo[]] Newest() {
        return @(Get-ChildItem -Path (Join-Path $this.Root 'Play*\DebugOpt\LithoKraft.exe') -ErrorAction SilentlyContinue |
                 Sort-Object LastWriteTime -Descending)
    }
}

class ShortcutWriter {
    [string]$Dir
    [object]$Shell
    ShortcutWriter([string]$dir) { $this.Dir = $dir; $this.Shell = New-Object -ComObject WScript.Shell }
    [void] Replace([string]$name, [string]$exe) {
        $path = Join-Path $this.Dir ($name + '.lnk')
        if (Test-Path -LiteralPath $path) {
            Copy-Item -LiteralPath $path -Destination ($path + '.orig') -Force
            Remove-Item -LiteralPath $path -Force
        }
        $s = $this.Shell.CreateShortcut($path)
        $s.TargetPath = $exe
        $s.Arguments = ''
        $s.WorkingDirectory = Split-Path -Parent $exe
        $s.IconLocation = $exe + ',0'
        $s.Description = $name
        $s.Save()
        Write-Output ("{0} -> {1} (wd {2})" -f $name, $exe, $s.WorkingDirectory)
    }
}

if (-not $Desktop) { $Desktop = [Environment]::GetFolderPath('Desktop') }
$found = (New-Object PlayBuildFinder $BuildRoot).Newest()
if (-not $PlayExe) { if ($found.Count -lt 1) { throw "no play builds under $BuildRoot" }; $PlayExe = $found[0].FullName }
if (-not $BackupExe) {
    $prev = @($found | Where-Object { $_.FullName -ne $PlayExe })
    if ($prev.Count -ge 1) { $BackupExe = $prev[0].FullName }
}
foreach ($e in @($PlayExe, $BackupExe)) { if ($e -and -not (Test-Path -LiteralPath $e)) { throw "missing exe $e" } }
$w = New-Object ShortcutWriter $Desktop
$w.Replace('LithoKraft (Play Build)', $PlayExe)
if ($BackupExe) { $w.Replace('LithoKraft (Play Build Backup)', $BackupExe) }
