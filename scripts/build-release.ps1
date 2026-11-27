param(
    [ValidateSet("x64")]
    [string]$Arch = "x64",
    [switch]$CreateTestCert,
    [switch]$SkipGitSyncCheck
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$configPath = Join-Path $repoRoot "packaging\store.config.json"
$config = Get-Content $configPath -Raw -Encoding UTF8 | ConvertFrom-Json
$ver = $config.Version
$releaseDir = Join-Path $repoRoot "dist\release"
$storeMsixSrc = Join-Path $repoRoot "dist\PrayerBar_${ver}_${Arch}_Store.msix"
$storeMsixDst = Join-Path $releaseDir "PrayerBar_${ver}_${Arch}_Store.msix"
$portableZip = Join-Path $releaseDir "PrayerBar_${ver}_${Arch}_Portable.zip"

Write-Host "=== [0/4] Verifying version & GitHub (origin/main) parity ==="
$commercialConfigPath = Join-Path $repoRoot "src\core\commercial_config.h"
$commercialConfigRaw = Get-Content $commercialConfigPath -Raw -Encoding UTF8
$verMatch = [regex]::Match($commercialConfigRaw, 'kAppVersionW\s*=\s*L"([^"]+)"')
if (-not $verMatch.Success) {
    throw "Could not parse kAppVersionW from $commercialConfigPath"
}
$cppVersion = $verMatch.Groups[1].Value
if ($cppVersion -ne $ver) {
    throw "Version mismatch: commercial_config.h ($cppVersion) != packaging/store.config.json ($ver)"
}

if (-not $SkipGitSyncCheck) {
    Push-Location $repoRoot
    try {
        $dirty = (& git status --porcelain)
        if ($LASTEXITCODE -ne 0) { throw "git status failed." }
        if ($dirty) {
            throw "Refusing to package a Store release with uncommitted changes. Commit and push to origin/main first."
        }
        & git fetch origin main
        if ($LASTEXITCODE -ne 0) {
            throw "git fetch origin main failed. Connect to GitHub and sync origin/main (or pass -SkipGitSyncCheck only for local non-Store tests)."
        }
        $headSha = (& git rev-parse HEAD).Trim()
        $originSha = (& git rev-parse origin/main).Trim()
        if ($LASTEXITCODE -ne 0 -or -not $headSha -or -not $originSha) {
            throw "Could not resolve HEAD or origin/main commit SHA."
        }
        if ($headSha -ne $originSha) {
            throw "Store/GitHub parity check failed: HEAD ($headSha) != origin/main ($originSha). Sync/push origin/main before building a Store release."
        }
        Write-Host "Parity OK: v$ver at origin/main ($headSha)"
    } finally {
        Pop-Location
    }
}

foreach ($path in @($storeMsixSrc, $storeMsixDst, $portableZip)) {
    if (Test-Path $path) {
        throw "Refusing to overwrite an existing release artifact: $path"
    }
}

Write-Host "=== [1/4] Building & Running Unit Tests ==="
& (Join-Path $PSScriptRoot "build.ps1")
if ($LASTEXITCODE -ne 0) { throw "build.ps1 failed." }

Write-Host "=== [2/4] Running Diagnostic Smoke Test ==="
$appExe = Join-Path $repoRoot "build\PrayerBar.exe"
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
New-Item -ItemType Directory -Force -Path $releaseDir | Out-Null

Copy-Item -Path $storeMsixSrc -Destination $storeMsixDst

$portableStage = Join-Path $repoRoot "dist\_portable_${Arch}_$PID"
try {
    New-Item -ItemType Directory -Force -Path (Join-Path $portableStage "Assets\Sounds") | Out-Null
    Copy-Item -Path $appExe -Destination (Join-Path $portableStage "PrayerBar.exe") -Force
    Copy-Item -Path (Join-Path $repoRoot "assets\sounds\*.mp3") -Destination (Join-Path $portableStage "Assets\Sounds") -Force
    Copy-Item -Path (Join-Path $repoRoot "docs\AUDIO_LICENSES.md") -Destination (Join-Path $portableStage "AUDIO_LICENSES.md") -Force
    Copy-Item -Path (Join-Path $repoRoot "README.md") -Destination (Join-Path $portableStage "README.md") -Force

    Compress-Archive -Path (Join-Path $portableStage "*") -DestinationPath $portableZip -CompressionLevel Optimal
} finally {
    if (Test-Path $portableStage) {
        Remove-Item $portableStage -Recurse -Force -ErrorAction SilentlyContinue
    }
}

$sumsPath = Join-Path $releaseDir "SHA256SUMS.txt"
$newLines = @()
foreach ($f in @($storeMsixDst, $portableZip)) {
    $hash = (Get-FileHash -Path $f -Algorithm SHA256).Hash
    $name = [System.IO.Path]::GetFileName($f)
    $newLines += "$hash  $name"
}
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
if (Test-Path $sumsPath) {
    $existingText = [System.IO.File]::ReadAllText($sumsPath, [System.Text.Encoding]::UTF8)
    if ($existingText.Length -gt 0 -and -not $existingText.EndsWith("`n")) {
        [System.IO.File]::AppendAllText($sumsPath, "`r`n", $utf8NoBom)
    }
    [System.IO.File]::AppendAllText($sumsPath, ($newLines -join "`r`n") + "`r`n", $utf8NoBom)
} else {
    [System.IO.File]::WriteAllLines($sumsPath, $newLines, $utf8NoBom)
}

Write-Host ""
Write-Host "Release artifacts ready in: $releaseDir"
Get-ChildItem $releaseDir | Format-Table Name, Length, LastWriteTime -AutoSize
