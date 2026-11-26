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

function Find-MsvcVcvars64 {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null | Select-Object -First 1
        if ($vsPath) {
            $vc64 = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
            if (Test-Path $vc64) { return $vc64 }
        }
    }
    $fallback = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    if (Test-Path $fallback) { return $fallback }
    return $null
}

$appSourcesRel = @(
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
    "src\platform\widget_provider.cpp",
    "src\platform\audio_player.cpp",
    "src\platform\theme_utils.cpp",
    "src\platform\taskbar_geom.cpp",
    "src\ui\taskbar_renderer.cpp",
    "src\ui\context_menu.cpp",
    "src\ui\taskbar_band.cpp",
    "src\ui\tray_icon.cpp",
    "src\ui\flyout_window.cpp",
    "src\ui\imsakiye_window.cpp",
    "src\ui\pro_window.cpp",
    "src\ui\settings_window.cpp",
    "src\ui\app_host.cpp",
    "src\main.cpp"
)
$testSourcesRel = @(
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
)

$appSources = $appSourcesRel | ForEach-Object { Join-Path $repoRoot $_ }
$testSources = $testSourcesRel | ForEach-Object { Join-Path $repoRoot $_ }
$rcFile = Join-Path $repoRoot "src\app.rc"
$resFile = Join-Path $buildDir "app.res"
$appExe = Join-Path $buildDir "PrayerBar.exe"
$testExe = Join-Path $buildDir "vakit_tests.exe"

$clangxx = Find-LlvmTool "x86_64-w64-mingw32-clang++.exe"
$windres = Find-LlvmTool "x86_64-w64-mingw32-windres.exe"
$objdump = Find-LlvmTool "x86_64-w64-mingw32-objdump.exe"
if (-not $objdump) { $objdump = Find-LlvmTool "llvm-objdump.exe" }

$useMinGw = ($null -ne $clangxx -and $null -ne $windres)

if ($useMinGw) {
    Write-Host "[1/4] Compiling Win32 resources (app.rc -> app.res) [LLVM-MinGW]..."
    & $windres $rcFile -O coff -o $resFile
    if ($LASTEXITCODE -ne 0) { throw "windres failed with exit code $LASTEXITCODE" }

    Write-Host "[2/4] Compiling PrayerBar.exe (Pure Native C++20)..."
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
} else {
    $vcvars = Find-MsvcVcvars64
    if (-not $vcvars) {
        throw "Neither x86_64-w64-mingw32-clang++.exe nor Visual Studio BuildTools vcvars64.bat found."
    }

    $objAppDir = Join-Path $buildDir "obj\app"
    $objTestDir = Join-Path $buildDir "obj\test"
    New-Item -ItemType Directory -Force -Path $objAppDir, $objTestDir | Out-Null

    Write-Host "[1/4] Compiling Win32 resources (app.rc -> app.res) [MSVC]..."
    $rcCmd = "`"$vcvars`" >nul && rc.exe /nologo /fo `"$resFile`" `"$rcFile`""
    cmd.exe /c $rcCmd
    if ($LASTEXITCODE -ne 0) { throw "rc.exe failed with exit code $LASTEXITCODE" }

    Write-Host "[2/4] Compiling PrayerBar.exe (Pure Native C++20) [MSVC]..."
    $appSrcQuoted = ($appSources | ForEach-Object { "`"$_`"" }) -join " "
    $msvcLibs = "user32.lib gdi32.lib gdiplus.lib shell32.lib dwmapi.lib winmm.lib winhttp.lib ole32.lib comctl32.lib comdlg32.lib uxtheme.lib advapi32.lib psapi.lib runtimeobject.lib oleacc.lib oleaut32.lib uuid.lib"
    $clCmd = "`"$vcvars`" >nul && cl.exe /nologo /std:c++20 /O2 /EHsc /utf-8 /MT /W3 /DUNICODE /D_UNICODE /D_WIN32_WINNT=0x0A00 /DWINVER=0x0A00 /Fo`"$objAppDir\\`" $appSrcQuoted `"$resFile`" /Fe`"$appExe`" /link /MANIFEST:NO /SUBSYSTEM:WINDOWS /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA /TSAWARE $msvcLibs"
    cmd.exe /c $clCmd
    if ($LASTEXITCODE -ne 0) { throw "cl.exe failed compiling PrayerBar.exe with exit code $LASTEXITCODE" }
}

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
if ($hasHighEntropy -and $hasDynamicBase -and $hasNxCompat) {
    # ok
} else {
    throw "PE binary missing mandatory Windows Store security flags (ASLR/DEP/HighEntropyVA)!"
}

if ($useMinGw -and $objdump) {
    $imports = & $objdump -p $appExe | Select-String "DLL Name:"
    $imports | ForEach-Object { Write-Host "  $($_.Line.Trim())" }
    $bad = $imports | Where-Object { $_.Line -match "libstdc\+\+|libgcc|winpthread|libc\+\+" }
    if ($bad) {
        throw "Unexpected external MinGW runtime DLL import detected: $bad"
    }
}

if (-not $SkipTests) {
    Write-Host "[4/4] Building & running native C++20 unit tests..."
    if ($useMinGw) {
        & $clangxx @commonFlags @testSources -o $testExe -lwinhttp -lole32 -lshell32 -ladvapi32 -luser32
        if ($LASTEXITCODE -ne 0) { throw "clang++ failed compiling vakit_tests.exe" }
    } else {
        $vcvars = Find-MsvcVcvars64
        $objTestDir = Join-Path $buildDir "obj\test"
        $testSrcQuoted = ($testSources | ForEach-Object { "`"$_`"" }) -join " "
        $clTestCmd = "`"$vcvars`" >nul && cl.exe /nologo /std:c++20 /O2 /EHsc /utf-8 /MT /W3 /DUNICODE /D_UNICODE /D_WIN32_WINNT=0x0A00 /DWINVER=0x0A00 /Fo`"$objTestDir\\`" $testSrcQuoted /Fe`"$testExe`" /link /SUBSYSTEM:CONSOLE winhttp.lib ole32.lib shell32.lib advapi32.lib user32.lib"
        cmd.exe /c $clTestCmd
        if ($LASTEXITCODE -ne 0) { throw "cl.exe failed compiling vakit_tests.exe" }
    }

    & $testExe
    if ($LASTEXITCODE -ne 0) { throw "vakit_tests.exe failed with exit code $LASTEXITCODE" }
}
