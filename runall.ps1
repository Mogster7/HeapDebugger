param (
    [string]$buildDir = "build-msvc", 
    [string]$config = "Debug"
)

$exePath = "$buildDir/$config"

for ($i=0; $i -le 11; $i++) {
    Write-Host $exePath/HeapDebugger.exe $i
    & ./$exePath/HeapDebugger.exe $i
}
