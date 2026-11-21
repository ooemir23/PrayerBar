param(
    [switch]$SkipTests
)

$ErrorActionPreference = "Stop"

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$buildDir = Join-Path $repoRoot "build"
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

function Find-LlvmTool {
    param([string]$Name)
    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $wingetPkgs = Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Packages"
    if (Test-Path $wingetPkgs) {
        $found = Get-ChildItem -Path $wingetPkgs -Filter $Name -Recurse -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($found) { return $found.FullName }
    }

    $progFiles = "C:\Program Files"
    $foundPf = Get-ChildItem -Path $progFiles -Filter $Name -Recurse -Depth 3 -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($foundPf) { return $foundPf.FullName }
    return $null
}

$clangxx = Find-LlvmTool "x86_64-w64-mingw32-clang++.exe"
$windres = Find-LlvmTool "x86_64-w64-mingw32-windres.exe"
$objdump = Find-LlvmTool "x86_64-w64-mingw32-objdump.exe"
if (-not $objdump) { $objdump = Find-LlvmTool "llvm-objdump.exe" }
if (-not $clangxx) { throw "x86_64-w64-mingw32-clang++.exe not found. Run: winget install MartinStorsjo.LLVM-MinGW.UCRT" }
if (-not $windres) { throw "x86_64-w64-mingw32-windres.exe not found." }

Write-Host "[1/4] Compiling Win32 resources (app.rc -> app.res)..."
$rcFile = Join-Path $repoRoot "src\app.rc"
$resFile = Join-Path $buildDir "app.res"
& $windres $rcFile -O coff -o $resFile
if ($LASTEXITCODE -ne 0) { throw "windres failed with exit code $LASTEXITCODE" }

Write-Host "[2/4] Compiling PrayerBar.exe (Pure Native C++20)..."
$appSources = @(
    "src\core\city_catalog.cpp",
    "src\core\prayer_calc.cpp",
    "src\core\i18n.cpp",
    "src\core\formatter.cpp",
    "src\core\json_mini.cpp",
    "src\core\settings_store.cpp",
    "src\core\schedule_service.cpp",
    "src\core\license_manager.cpp",
    "src\core\prayer_tracking.cpp",
    "src\platform\win_http.cpp",
    "src\platform\win_store.cpp",
    "src\platform\toast.cpp",
    "src\platform\audio_player.cpp",
    "src\platform\theme_utils.cpp",
    "src\platform\taskbar_geom.cpp",
    "src\ui\taskbar_renderer.cpp",
    "src\ui\taskbar_band.cpp",
    "src\ui\tray_icon.cpp",
    "src\ui\flyout_window.cpp",
    "src\ui\imsakiye_window.cpp",
    "src\ui\pro_window.cpp",
    "src\ui\settings_window.cpp",
    "src\ui\app_host.cpp",
    "src\main.cpp"
) | ForEach-Object { Join-Path $repoRoot $_ }

$appExe = Join-Path $buildDir "PrayerBar.exe"
$commonFlags = @(
    "-std=c++20", "-O2", "-s",
    "-Wall", "-Wextra",
    "-finput-charset=UTF-8", "-fexec-charset=UTF-8",
    "-DUNICODE", "-D_UNICODE",
    "-D_WIN32_WINNT=0x0A00", "-DWINVER=0x0A00",
    "-static", "-static-libgcc", "-static-libstdc++"
)
$linkFlags = @(
    "-mwindows", "-municode",
    "-Wl,--dynamicbase,--nxcompat,--high-entropy-va,--tsaware",
    "-luser32", "-lgdi32", "-lgdiplus", "-lshell32", "-ldwmapi",
    "-lwinmm", "-lwinhttp", "-lole32", "-lcomctl32", "-lcomdlg32",
    "-luxtheme", "-ladvapi32", "-lpsapi", "-lruntimeobject", "-loleacc", "-loleaut32", "-luuid"
)

& $clangxx @commonFlags @appSources $resFile -o $appExe @linkFlags
if ($LASTEXITCODE -ne 0) { throw "clang++ failed compiling PrayerBar.exe with exit code $LASTEXITCODE" }

$soundsDest = Join-Path $buildDir "Assets\Sounds"
New-Item -ItemType Directory -Force -Path $soundsDest | Out-Null
Copy-Item -Path (Join-Path $repoRoot "assets\sounds\*.mp3") -Destination $soundsDest -Force

$exeInfo = Get-Item $appExe
Write-Host ("Built {0} ({1:F1} KB)" -f $appExe, ($exeInfo.Length / 1KB))

Write-Host "[3/4] Verifying PE security headers & DLL imports..."
$bytes = [System.IO.File]::ReadAllBytes($appExe)
$peOffset = [BitConverter]::ToInt32($bytes, 0x3C)
$magic = [BitConverter]::ToUInt16($bytes, $peOffset + 24)
if ($magic -ne 0x20B) { throw "Expected PE32+ (64-bit) image, got 0x$($magic.ToString('X'))" }
$dllChars = [BitConverter]::ToUInt16($bytes, $peOffset + 24 + 70)
$hasHighEntropy = ($dllChars -band 0x0020) -ne 0
$hasDynamicBase = ($dllChars -band 0x0040) -ne 0
$hasNxCompat    = ($dllChars -band 0x0100) -ne 0
Write-Host ("PE DllCharacteristics: 0x{0:X4} | HIGH_ENTROPY_VA={1} | DYNAMIC_BASE={2} | NX_COMPAT={3}" -f $dllChars, $hasHighEntropy, $hasDynamicBase, $hasNxCompat)
if (-not ($hasHighEntropy -and $hasDynamicBase -and $hasNxCompat)) {
    throw "PE binary missing mandatory Windows Store security flags (ASLR/DEP/HighEntropyVA)!"
}

if ($objdump) {
    $imports = & $objdump -p $appExe | Select-String "DLL Name:"
    $imports | ForEach-Object { Write-Host "  $($_.Line.Trim())" }
    $bad = $imports | Where-Object { $_.Line -match "libstdc\+\+|libgcc|winpthread|libc\+\+" }
    if ($bad) {
        throw "Unexpected external MinGW runtime DLL import detected: $bad"
    }
}

if (-not $SkipTests) {
    Write-Host "[4/4] Building & running native C++20 unit tests..."
    $testSources = @(
        "src\core\city_catalog.cpp",
        "src\core\prayer_calc.cpp",
        "src\core\i18n.cpp",
        "src\core\formatter.cpp",
        "src\core\json_mini.cpp",
        "src\core\settings_store.cpp",
        "src\core\schedule_service.cpp",
        "src\core\license_manager.cpp",
        "src\core\prayer_tracking.cpp",
        "src\platform\win_http.cpp",
        "src\platform\win_store.cpp",
        "tests\vakit_tests.cpp"
    ) | ForEach-Object { Join-Path $repoRoot $_ }

    $testExe = Join-Path $buildDir "vakit_tests.exe"
    & $clangxx @commonFlags @testSources -o $testExe -lwinhttp -lole32 -lshell32 -ladvapi32 -luser32
    if ($LASTEXITCODE -ne 0) { throw "clang++ failed compiling vakit_tests.exe" }

    & $testExe
    if ($LASTEXITCODE -ne 0) { throw "vakit_tests.exe failed with exit code $LASTEXITCODE" }
}
