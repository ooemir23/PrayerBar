param(
    [ValidateSet("x64")]
    [string]$Arch = "x64",
    [switch]$CreateTestCert,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$configPath = Join-Path $repoRoot "packaging\store.config.json"
$templatePath = Join-Path $repoRoot "packaging\msix\AppxManifest.template.xml"
$assetsSrc = Join-Path $repoRoot "packaging\msix\Assets"
$soundsSrc = Join-Path $repoRoot "assets\sounds"
$buildExe = Join-Path $repoRoot "build\PrayerBar.exe"
$distDir = Join-Path $repoRoot "dist"
$stagingDir = Join-Path $repoRoot "dist\_stage_win-${Arch}_$PID"
$toolsDir = Join-Path $repoRoot ".tools\winsdk-buildtools"

$config = Get-Content $configPath -Raw -Encoding UTF8 | ConvertFrom-Json
$storeMsixPath = Join-Path $distDir "PrayerBar_$($config.Version)_${Arch}_Store.msix"
if (Test-Path $storeMsixPath) {
    throw "Refusing to overwrite an existing MSIX package: $storeMsixPath"
}

# 1. Görsel varlıklar eksikse üret
if (-not (Test-Path (Join-Path $assetsSrc "app.ico"))) {
    Write-Host "[1/5] Generating MSIX visual assets..."
    & (Join-Path $PSScriptRoot "generate-assets.ps1") -OutputDir $assetsSrc
}

# 2. Native C++20 derleme
if (-not $SkipBuild) {
    Write-Host "[2/5] Building Native C++20 PrayerBar.exe..."
    & (Join-Path $PSScriptRoot "build.ps1")
}
if (-not (Test-Path $buildExe)) {
    throw "Built binary not found: $buildExe"
}

# 3. Staging klasörünü hazırla ve paketle
New-Item -ItemType Directory -Force -Path $distDir | Out-Null
try {
    Write-Host "[3/5] Preparing staging directory, AppxManifest.xml, Assets and Sounds..."
    if (Test-Path $stagingDir) {
        Remove-Item $stagingDir -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $stagingDir | Out-Null

    Copy-Item -Path $buildExe -Destination (Join-Path $stagingDir "PrayerBar.exe") -Force
    # Metadata for the IWidgetProvider marshaling proxy declared in AppxManifest (see windows.activatableClass.proxyStub).
    Copy-Item -Path (Join-Path $repoRoot "packaging\msix\Microsoft.Windows.Widgets.winmd") -Destination $stagingDir -Force

    $assetsDest = Join-Path $stagingDir "Assets"
    New-Item -ItemType Directory -Force -Path $assetsDest | Out-Null
    Copy-Item -Path (Join-Path $assetsSrc "*") -Destination $assetsDest -Recurse -Force

    $soundsDest = Join-Path $assetsDest "Sounds"
    New-Item -ItemType Directory -Force -Path $soundsDest | Out-Null
    Copy-Item -Path (Join-Path $soundsSrc "*.mp3") -Destination $soundsDest -Force

    $xmlEsc = { param([string]$s) [System.Security.SecurityElement]::Escape($s) }
    $manifestContent = Get-Content $templatePath -Raw -Encoding UTF8
    $manifestContent = $manifestContent.Replace("{{IDENTITY_NAME}}", (& $xmlEsc $config.IdentityName))
    $manifestContent = $manifestContent.Replace("{{PUBLISHER}}", (& $xmlEsc $config.Publisher))
    $manifestContent = $manifestContent.Replace("{{PUBLISHER_DISPLAY_NAME}}", (& $xmlEsc $config.PublisherDisplayName))
    $manifestContent = $manifestContent.Replace("{{DISPLAY_NAME}}", (& $xmlEsc $config.DisplayName))
    $manifestContent = $manifestContent.Replace("{{DESCRIPTION}}", (& $xmlEsc $config.Description))
    $manifestContent = $manifestContent.Replace("{{VERSION}}", (& $xmlEsc $config.Version))
    $manifestContent = $manifestContent.Replace("{{ARCH}}", (& $xmlEsc $Arch))

    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText((Join-Path $stagingDir "AppxManifest.xml"), $manifestContent, $utf8NoBom)

    # 4. makeappx.exe ve signtool.exe bul
    function Find-SdkTool {
        param([string]$ExeName)
        $kitsBin = "C:\Program Files (x86)\Windows Kits\10\bin"
        if (Test-Path $kitsBin) {
            $found = Get-ChildItem -Path $kitsBin -Filter $ExeName -Recurse -ErrorAction SilentlyContinue |
                Where-Object { $_.FullName -match "\\x64\\" } |
                Sort-Object FullName -Descending |
                Select-Object -First 1
            if ($found) { return $found.FullName }
        }

        if (Test-Path $toolsDir) {
            $found = Get-ChildItem -Path $toolsDir -Filter $ExeName -Recurse -ErrorAction SilentlyContinue |
                Where-Object { $_.FullName -match "\\x64\\" } |
                Sort-Object FullName -Descending |
                Select-Object -First 1
            if ($found) { return $found.FullName }
        }
        return $null
    }

    $makeappx = Find-SdkTool "makeappx.exe"
    if (-not $makeappx) {
        Write-Host "[4/5] Downloading Microsoft.Windows.SDK.BuildTools (NuGet)..."
        New-Item -ItemType Directory -Force -Path $toolsDir | Out-Null
        $indexJson = Invoke-RestMethod -Uri "https://api.nuget.org/v3-flatcontainer/microsoft.windows.sdk.buildtools/index.json"
        $stableVer = $indexJson.versions | Where-Object { $_ -notmatch "-" } | Select-Object -Last 1
        if (-not $stableVer) { $stableVer = $indexJson.versions | Select-Object -Last 1 }

        $nupkgUrl = "https://api.nuget.org/v3-flatcontainer/microsoft.windows.sdk.buildtools/$stableVer/microsoft.windows.sdk.buildtools.$stableVer.nupkg"
        $nupkgFile = Join-Path $toolsDir "sdk.zip"
        Invoke-WebRequest -Uri $nupkgUrl -OutFile $nupkgFile
        Expand-Archive -Path $nupkgFile -DestinationPath $toolsDir -Force
        Remove-Item $nupkgFile -Force

        $makeappx = Find-SdkTool "makeappx.exe"
        if (-not $makeappx) {
            throw "makeappx.exe could not be located inside $toolsDir"
        }
    }
    else {
        Write-Host "[4/5] Using makeappx.exe: $makeappx"
    }

    # 5. Store MSIX paketle
    Write-Host "[5/5] Packing MSIX -> $storeMsixPath"
    & $makeappx pack /d $stagingDir /p $storeMsixPath /o
    if ($LASTEXITCODE -ne 0) {
        throw "makeappx pack failed with exit code $LASTEXITCODE"
    }
} finally {
    if (Test-Path $stagingDir) {
        Remove-Item $stagingDir -Recurse -Force -ErrorAction SilentlyContinue
    }
}

if ($CreateTestCert) {
    $signtool = Find-SdkTool "signtool.exe"
    if (-not $signtool) {
        throw "signtool.exe not found for -CreateTestCert"
    }

    $localMsixPath = Join-Path $distDir "PrayerBar_$($config.Version)_${Arch}_LocalTest.msix"
    Copy-Item $storeMsixPath $localMsixPath -Force

    $pfxPath = Join-Path $distDir "PrayerBar_TestCert.pfx"
    $cerPath = Join-Path $distDir "PrayerBar_TestCert.cer"
    $certPass = "VakitTest123!"

    $existing = Get-ChildItem Cert:\CurrentUser\My | Where-Object { $_.Subject -eq $config.Publisher } | Select-Object -First 1
    if (-not $existing) {
        $existing = New-SelfSignedCertificate `
            -Type Custom `
            -Subject $config.Publisher `
            -KeyUsage DigitalSignature `
            -FriendlyName "PrayerBar Local Sideload Test Cert" `
            -CertStoreLocation "Cert:\CurrentUser\My" `
            -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3", "2.5.29.19={text}")
    }

    $secPass = ConvertTo-SecureString -String $certPass -Force -AsPlainText
    Export-PfxCertificate -Cert $existing -FilePath $pfxPath -Password $secPass | Out-Null
    Export-Certificate -Cert $existing -FilePath $cerPath | Out-Null

    & $signtool sign /fd SHA256 /f $pfxPath /p $certPass $localMsixPath
    if ($LASTEXITCODE -ne 0) {
        throw "signtool failed with exit code $LASTEXITCODE"
    }
    Write-Host "Local signed MSIX ready: $localMsixPath"
}

$msixInfo = Get-Item $storeMsixPath
Write-Host ("Store MSIX ready: {0} ({1:F2} MB)" -f $storeMsixPath, ($msixInfo.Length / 1MB))
