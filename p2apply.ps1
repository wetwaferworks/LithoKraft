param([string]$tgz)
$r='R:\repos\LithoLattice-Workflow\Pass2-2026-10-16\engine\src\render'; $st=Get-Date -Format 'yyyy-MM-dd-HHmm'
$files = tar -tzf $tgz
foreach($f in $files){ $p=Join-Path $r $f; if((Test-Path $p) -and -not (Test-Path "$p.orig-$st")){ Copy-Item $p "$p.orig-$st" } }
tar -xmzf $tgz -C $r
"applied $($files -join ', ') (backups .orig-$st)"
