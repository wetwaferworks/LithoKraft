param([string]$tag='b',[int]$secs=10,[switch]$nohash)
$d='R:\repos\LithoLattice-Workflow\Pass2-2026-10-16'; cd $d; $x="$d\build\Release\LithoKraft.exe"; $o="$d\p2logs"
if(-not $nohash){
 foreach($i in 1,2){ $h=& $x --handshake-scenario 2048 handshake 4 4 2>&1 | Select-String 'hash' | select -Last 1; "hash run$i : $h" }
 $st=& $x --selftest 2>&1; "selftest: " + (($st | Select-String 'ALL SELFTESTS' | select -Last 1).Line) + " PASS=" + (($st | Select-String 'PASS').Count)
 foreach($pp in @(@('walk',60),@('orbit',90))){ $f="$o\${tag}_ab_$($pp[0])"; $env:LL_SW_AB="$($pp[1])"; Start-Process $x -ArgumentList "--renderer software --bench 4 --bench-path $($pp[0]) --present immediate --no-save --mute --log $f.log" -Wait -WindowStyle Minimized; Remove-Item Env:LL_SW_AB; (Select-String -Path "$f.log" -Pattern 'swab' | select -Last 1).Line }
}
foreach($p in 'walk','orbit'){ $f="$o\${tag}_software_$p"; Start-Process $x -ArgumentList "--renderer software --bench $secs --bench-path $p --present immediate --no-save --mute --bench-json $f.json --log $f.log" -Wait -WindowStyle Minimized; $l=(Select-String -Path "$f.log" -Pattern "bench renderer=" | select -Last 1).Line; $fr=[int]($l -replace '.*frames=(\d+).*','$1'); "$tag software $p fps=$([math]::Round($fr/$secs,1))" }
