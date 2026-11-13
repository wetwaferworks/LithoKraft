param([string]$tag='b')
$ErrorActionPreference='Continue'
$d='R:\repos\LithoLattice-Workflow\Pass2-2026-10-16'; $r="$d/_depsrc" -replace '\\','/'
$cm='C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
New-Item -ItemType Directory "$d\p2logs" -Force | Out-Null
if(-not (Test-Path "$d\build\CMakeCache.txt")){
 & $cm -S "$d\engine" -B "$d\build" -G "Visual Studio 18 2026" -DLITHOKRAFT_DEV_BUILD=ON -DLITHOLATTICE_BUILD_TESTS=ON -DFETCHCONTENT_FULLY_DISCONNECTED=ON "-DFETCHCONTENT_SOURCE_DIR_GLFW=$r/glfw-src" "-DFETCHCONTENT_SOURCE_DIR_ANGELSCRIPT=$r/angelscript-src" "-DFETCHCONTENT_SOURCE_DIR_VULKAN_HEADERS=$r/vulkan_headers-src" "-DFETCHCONTENT_SOURCE_DIR_GLSLANG=$r/glslang-src" *> "$d\p2logs\cfg-$tag.txt"
 if($LASTEXITCODE){ "CONFIGURE FAIL"; Get-Content "$d\p2logs\cfg-$tag.txt" -Tail 20; exit 1 }
}
& $cm --build "$d\build" --config Release -- /m:2 /v:minimal *> "$d\p2logs\build-$tag.txt"
$e=$LASTEXITCODE
Select-String -Path "$d\p2logs\build-$tag.txt" -Pattern ': error |error C|error LNK' | select -First 15 | %{$_.Line}
"BUILD exit $e"
