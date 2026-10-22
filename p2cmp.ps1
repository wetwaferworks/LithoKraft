param([string]$tag,[string]$ma,[string]$mb,[int]$reps=3,[int]$secs=8)
$d='R:\repos\LithoLattice-Workflow\Pass2-2026-10-16'; cd $d; $x="$d\build\Release\LithoKraft.exe"; $o="$d\p2logs"; $res=@{}
for($r=0;$r -lt $reps;$r++){ foreach($m in $ma,$mb){ $env:LL_SW_P2=$m; foreach($p in 'walk','orbit'){ $f="$o\${tag}_m${m}_${p}_$r"; Start-Process $x -ArgumentList "--renderer software --bench $secs --bench-path $p --present immediate --no-save --mute --bench-json $f.json --log $f.log" -Wait -WindowStyle Minimized; $l=(Select-String -Path "$f.log" -Pattern "bench renderer=" | select -Last 1).Line; $fr=[int]($l -replace '.*frames=(\d+).*','$1'); $k="$m $p"; if(-not $res[$k]){$res[$k]=@()}; $res[$k]+= [math]::Round($fr/$secs,1) } } }
Remove-Item Env:LL_SW_P2
foreach($k in $res.Keys | sort){ $v=$res[$k] | sort; "mask $k : runs=$($res[$k] -join ',') median=$($v[[int][math]::Floor($v.Count/2)])" }
