# Thread Handshake P0 determinism gate (2026-10-29).
# Runs the scenario and demo hashes at several thread counts and checks they are identical
# across thread counts and (optionally) identical to a saved baseline.
# usage: tools\thread_gate.ps1 -Build <build dir> [-Config Release] [-Out gate.json] [-Baseline base.json] [-Workers 1,2,4,8,16]
param([Parameter(Mandatory)][string]$Build,[string]$Config='Release',[string]$Out,[string]$Baseline,[int[]]$Workers=@(1,2,4,8,16),[switch]$NoEngineTests)
$ErrorActionPreference='Continue'
$x=Join-Path $Build "$Config\LithoKraft.exe"; $et=Join-Path $Build "tests\$Config\litholattice_tests.exe"
if(!(Test-Path $x)){throw "missing $x"}
$r=[ordered]@{}; $fail=@()
function Get-GateHash($lines,$pat){ $m=$lines | Select-String -Pattern $pat | Select -First 1; if($m){$m.Matches[0].Groups[1].Value}else{'MISSING'} }
Push-Location $Build
try {
  $sw=[Diagnostics.Stopwatch]::StartNew()
  foreach($c in 'on','off'){
    $leg = Get-GateHash (& $x --handshake-scenario 256 legacy 1 1 --cities $c 2>&1) 'hash=([0-9a-f]{16})'
    $r["scenario.$c.legacy"]=$leg
    foreach($w in $Workers){
      $r["scenario.$c.w$w"] = Get-GateHash (& $x --handshake-scenario 256 handshake $w 4 --cities $c 2>&1) 'hash=([0-9a-f]{16})'
      $env:LL_GATE_WORKERS="$w"; $d = & $x --demo-test --cities $c 2>&1; $dx=$LASTEXITCODE; Remove-Item env:LL_GATE_WORKERS
      $r["demo.$c.recorded"] = Get-GateHash $d 'recorded \(legacy\): hash=([0-9a-f]{16})'
      $r["demo.$c.w$w"] = Get-GateHash $d 'replay \(handshake, \d+ workers\): hash=([0-9a-f]{16})'
      if($dx -ne 0){ $fail += "demo $c w$w exit $dx" }
    }
    $sv = $Workers | % { $r["scenario.$c.w$_"] } | Sort -Unique
    if(@($sv).Count -ne 1){ $fail += "scenario $c differs across workers: $($sv -join ',')" }
    $dv = @($Workers | % { $r["demo.$c.w$_"] }) + $r["demo.$c.recorded"] | Sort -Unique
    if(@($dv).Count -ne 1){ $fail += "demo $c differs across workers: $($dv -join ',')" }
  }
  $hs = & $x --handshake-test 2>&1; if($LASTEXITCODE -ne 0){ $fail += "handshake-test exit $LASTEXITCODE" }
  $r['handshake-test.fails'] = ($hs | Select-String '\[FAIL\]').Count
  if(-not $NoEngineTests -and (Test-Path $et)){
    $o = & $et 2>&1; $r['engine.exit']=$LASTEXITCODE; if($LASTEXITCODE -ne 0){ $fail += "engine tests exit $LASTEXITCODE" }
    $r['engine.pass'] = ($o | Select-String '\[PASS\]').Count; $r['engine.fail'] = ($o | Select-String '\[FAIL\]').Count
    $r['engine.failnames'] = @($o | Select-String '\[FAIL\]' | % { $_.Line.Trim() })
  }
  $r['seconds']=[math]::Round($sw.Elapsed.TotalSeconds,1)
} finally { Pop-Location }
foreach($k in @($r.Keys)){ if($r[$k] -eq 'MISSING'){ $fail += "$k missing" } }
if($Baseline){
  $b = Get-Content $Baseline -Raw | ConvertFrom-Json
  foreach($k in $r.Keys){ if($k -match '^(scenario|demo)\.' -and $b.$k -and $b.$k -ne $r[$k]){ $fail += "$k changed: $($b.$k) -> $($r[$k])" } }
}
$r['result'] = if($fail.Count){'FAIL'}else{'PASS'}; $r['problems']=$fail
$j = $r | ConvertTo-Json -Depth 4
if($Out){ [IO.File]::WriteAllText($Out,$j,(New-Object Text.UTF8Encoding $false)) }
$j
if($fail.Count){ exit 1 }