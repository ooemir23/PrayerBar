param(
    [ValidateSet("x64")]
    [string]$Arch = "x64",
    [switch]$CreateTestCert
)

$ErrorActionPreference = "Stop"

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$configPath = Join-Path $repoRoot "packaging\store.config.json"
$config = Get-Content $configPath -Raw -Encoding UTF8 | ConvertFrom-Json
$ver = $config.Version

Write-Host "=== [1/4] Building & Running Unit Tests ==="
& (Join-Path $PSScriptRoot "build.ps1")
if ($LASTEXITCODE -ne 0) { throw "build.ps1 failed." }

Write-Host "=== [2/4] Running Diagnostic Smoke Test ==="
$appExe = Join-Path $repoRoot "build\VakitCubugu.exe"
& $appExe --diag-smoke | Out-Host
if ($LASTEXITCODE -ne 0) { throw "Diagnostic smoke test failed with exit code $LASTEXITCODE" }

Write-Host "=== [3/4] Packing Store MSIX ==="
if ($CreateTestCert) {
    & (Join-Path $PSScriptRoot "build-msix.ps1") -Arch $Arch -SkipBuild -CreateTestCert
} else {
    & (Join-Path $PSScriptRoot "build-msix.ps1") -Arch $Arch -SkipBuild
}
if ($LASTEXITCODE -ne 0) { throw "build-msix.ps1 failed." }

Write-Host "=== [4/4] Assembling Global Release Folder ==="
$releaseDir = Join-Path $repoRoot "dist\release"
New-Item -ItemType Directory -Force -Path $releaseDir | Out-Null

$storeMsixSrc = Join-Path $repoRoot "dist\VakitCubugu_${ver}_${Arch}_Store.msix"
$storeMsixDst = Join-Path $releaseDir "VakitCubugu_${ver}_${Arch}_Store.msix"
Copy-Item -Path $storeMsixSrc -Destination $storeMsixDst -Force

$portableStage = Join-Path $repoRoot "dist\_portable_${Arch}_$PID"
$portableZip = Join-Path $releaseDir "VakitCubugu_${ver}_${Arch}_Portable.zip"
try {
    New-Item -ItemType Directory -Force -Path (Join-Path $portableStage "Assets\Sounds") | Out-Null
    Copy-Item -Path $appExe -Destination (Join-Path $portableStage "VakitCubugu.exe") -Force
    Copy-Item -Path (Join-Path $repoRoot "assets\sounds\*.mp3") -Destination (Join-Path $portableStage "Assets\Sounds") -Force
    Copy-Item -Path (Join-Path $repoRoot "docs\AUDIO_LICENSES.md") -Destination (Join-Path $portableStage "AUDIO_LICENSES.md") -Force
    Copy-Item -Path (Join-Path $repoRoot "README.md") -Destination (Join-Path $portableStage "README.md") -Force

    if (Test-Path $portableZip) { Remove-Item $portableZip -Force }
    Compress-Archive -Path (Join-Path $portableStage "*") -DestinationPath $portableZip -CompressionLevel Optimal
} finally {
    if (Test-Path $portableStage) {
        Remove-Item $portableStage -Recurse -Force -ErrorAction SilentlyContinue
    }
}

$sumsPath = Join-Path $releaseDir "SHA256SUMS.txt"
$lines = @()
foreach ($f in @($storeMsixDst, $portableZip)) {
    $hash = (Get-FileHash -Path $f -Algorithm SHA256).Hash
    $name = [System.IO.Path]::GetFileName($f)
    $lines += "$hash  $name"
}
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllLines($sumsPath, $lines, $utf8NoBom)

Write-Host ""
Write-Host "Release artifacts ready in: $releaseDir"
Get-ChildItem $releaseDir | Format-Table Name, Length, LastWriteTime -AutoSize
