<#
.SYNOPSIS
    Generates the complete Microsoft Store visual kit for PrayerBar:
    - Store Logos & Promotional Art (300x300, 1080x1080, 720x1080, 1920x1080)
    - Desktop Store Screenshots in English and Turkish (1920x1080 x 4 each)
#>
param(
    [string]$OutputDir = (Join-Path $PSScriptRoot "..\dist\store-assets")
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

function T {
    param([string]$Text)
    return [System.Text.RegularExpressions.Regex]::Unescape($Text)
}

$outRoot = [System.IO.Path]::GetFullPath($OutputDir)
$logosDir = Join-Path $outRoot "logos"
$enDir = Join-Path $outRoot "screenshots-en"
$trDir = Join-Path $outRoot "screenshots-tr"

foreach ($d in @($logosDir, $enDir, $trDir)) {
    New-Item -ItemType Directory -Force -Path $d | Out-Null
}

# Brand & Windows 11 Fluent dark palette
$C = @{
    BgTop        = [System.Drawing.Color]::FromArgb(255, 10, 16, 22)
    BgBottom     = [System.Drawing.Color]::FromArgb(255, 11, 43, 42)
    TealDark     = [System.Drawing.Color]::FromArgb(255, 11, 59, 56)
    TealLight    = [System.Drawing.Color]::FromArgb(255, 20, 122, 110)
    Accent       = [System.Drawing.Color]::FromArgb(255, 45, 212, 191)
    AccentSoft   = [System.Drawing.Color]::FromArgb(42, 45, 212, 191)
    Gold         = [System.Drawing.Color]::FromArgb(255, 255, 206, 92)
    GoldWarm     = [System.Drawing.Color]::FromArgb(255, 244, 174, 52)
    Surface      = [System.Drawing.Color]::FromArgb(244, 24, 30, 38)
    SurfaceCard  = [System.Drawing.Color]::FromArgb(255, 31, 39, 49)
    SurfaceHi    = [System.Drawing.Color]::FromArgb(255, 22, 74, 69)
    Border       = [System.Drawing.Color]::FromArgb(65, 255, 255, 255)
    BorderAccent = [System.Drawing.Color]::FromArgb(145, 45, 212, 191)
    TextPrimary  = [System.Drawing.Color]::FromArgb(255, 243, 246, 250)
    TextMuted    = [System.Drawing.Color]::FromArgb(255, 152, 166, 182)
    TaskbarBg    = [System.Drawing.Color]::FromArgb(245, 18, 22, 28)
}

function New-Font {
    param([float]$Size, [string]$Weight = "Regular", [bool]$Display = $false)
    $family = if ($Display) { "Segoe UI Variable Display" } else { "Segoe UI" }
    $style = switch ($Weight) {
        "Bold" { [System.Drawing.FontStyle]::Bold }
        "Italic" { [System.Drawing.FontStyle]::Italic }
        default { [System.Drawing.FontStyle]::Regular }
    }
    return New-Object System.Drawing.Font($family, $Size, $style, [System.Drawing.GraphicsUnit]::Pixel)
}

function New-RoundedPath {
    param([float]$X, [float]$Y, [float]$W, [float]$H, [float]$R)
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = [Math]::Min($R * 2.0, [Math]::Min($W, $H))
    if ($d -le 1.0) {
        $path.AddRectangle([System.Drawing.RectangleF]::new($X, $Y, $W, $H))
        return $path
    }
    $path.AddArc($X, $Y, $d, $d, 180, 90)
    $path.AddArc($X + $W - $d, $Y, $d, $d, 270, 90)
    $path.AddArc($X + $W - $d, $Y + $H - $d, $d, $d, 0, 90)
    $path.AddArc($X, $Y + $H - $d, $d, $d, 90, 90)
    $path.CloseFigure()
    return $path
}

function Fill-RoundedRect {
    param(
        [System.Drawing.Graphics]$G,
        [float]$X, [float]$Y, [float]$W, [float]$H, [float]$R,
        [System.Drawing.Brush]$Fill,
        [System.Drawing.Color]$StrokeColor = [System.Drawing.Color]::Empty,
        [float]$StrokeWidth = 1.0
    )
    $path = New-RoundedPath -X $X -Y $Y -W $W -H $H -R $R
    $G.FillPath($Fill, $path)
    if (-not $StrokeColor.IsEmpty) {
        $pen = New-Object System.Drawing.Pen($StrokeColor, $StrokeWidth)
        $G.DrawPath($pen, $path)
        $pen.Dispose()
    }
    $path.Dispose()
}

function Draw-SolidRoundedRect {
    param(
        [System.Drawing.Graphics]$G,
        [float]$X, [float]$Y, [float]$W, [float]$H, [float]$R,
        [System.Drawing.Color]$FillColor,
        [System.Drawing.Color]$StrokeColor = [System.Drawing.Color]::Empty,
        [float]$StrokeWidth = 1.0
    )
    $b = New-Object System.Drawing.SolidBrush($FillColor)
    Fill-RoundedRect -G $G -X $X -Y $Y -W $W -H $H -R $R -Fill $b -StrokeColor $StrokeColor -StrokeWidth $StrokeWidth
    $b.Dispose()
}

function Draw-BrandIcon {
    param([System.Drawing.Graphics]$G, [float]$X, [float]$Y, [float]$Size, [bool]$WithTile = $true)
    $G.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias

    if ($WithTile) {
        $pad = $Size * 0.04
        $bgSize = $Size - ($pad * 2)
        $rect = [System.Drawing.RectangleF]::new($X + $pad, $Y + $pad, $bgSize, $bgSize)
        $grad = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
            $rect, $C.TealDark, $C.TealLight,
            [System.Drawing.Drawing2D.LinearGradientMode]::ForwardDiagonal
        )
        Fill-RoundedRect -G $G -X $rect.X -Y $rect.Y -W $rect.Width -H $rect.Height -R ($bgSize * 0.23) `
            -Fill $grad -StrokeColor ([System.Drawing.Color]::FromArgb(75, 255, 255, 255)) -StrokeWidth ([Math]::Max(1.5, $Size * 0.015))
        $grad.Dispose()
    }

    $cx = $X + $Size * 0.50
    $cy = $Y + $Size * 0.50
    $ringR = $Size * 0.33
    $ringW = [Math]::Max(2.0, $Size * 0.06)

    $trackPen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(65, 255, 255, 255), $ringW)
    $G.DrawEllipse($trackPen, $cx - $ringR, $cy - $ringR, $ringR * 2, $ringR * 2)
    $trackPen.Dispose()

    $arcPen = New-Object System.Drawing.Pen($C.Gold, $ringW)
    $arcPen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $arcPen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $G.DrawArc($arcPen, $cx - $ringR, $cy - $ringR, $ringR * 2, $ringR * 2, -90, 270)
    $arcPen.Dispose()

    $tipR = $ringW * 0.90
    $tipBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 255, 248, 214))
    $G.FillEllipse($tipBrush, $cx - $ringR - $tipR, $cy - $tipR, $tipR * 2, $tipR * 2)
    $tipBrush.Dispose()

    $outerR = $Size * 0.20
    $outerX = $cx - $outerR
    $outerY = $cy - $outerR
    $innerR = $outerR * 0.80
    $innerX = $outerX + $outerR * 0.48
    $innerY = $outerY - $outerR * 0.16

    $outerPath = New-Object System.Drawing.Drawing2D.GraphicsPath
    $outerPath.AddEllipse($outerX, $outerY, $outerR * 2, $outerR * 2)
    $innerPath = New-Object System.Drawing.Drawing2D.GraphicsPath
    $innerPath.AddEllipse($innerX, $innerY, $innerR * 2, $innerR * 2)
    $region = New-Object System.Drawing.Region($outerPath)
    $region.Exclude($innerPath)

    $moonRect = [System.Drawing.RectangleF]::new($outerX, $outerY, $outerR * 2, $outerR * 2)
    $moonBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
        $moonRect, [System.Drawing.Color]::FromArgb(255, 255, 236, 160), $C.GoldWarm,
        [System.Drawing.Drawing2D.LinearGradientMode]::Vertical
    )
    $G.FillRegion($moonBrush, $region)
    $moonBrush.Dispose()
    $region.Dispose()
    $outerPath.Dispose()
    $innerPath.Dispose()
}

function Draw-Backdrop {
    param([System.Drawing.Graphics]$G, [int]$W, [int]$H)
    $G.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $G.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit

    $rect = [System.Drawing.RectangleF]::new(0, 0, $W, $H)
    $bgGrad = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
        $rect, $C.BgTop, $C.BgBottom,
        [System.Drawing.Drawing2D.LinearGradientMode]::ForwardDiagonal
    )
    $G.FillRectangle($bgGrad, $rect)
    $bgGrad.Dispose()

    # Soft radial ambient glows
    foreach ($glow in @(
        @{ X = $W * 0.72; Y = $H * 0.20; R = $W * 0.42; Col = [System.Drawing.Color]::FromArgb(55, 45, 212, 191) },
        @{ X = $W * 0.20; Y = $H * 0.78; R = $W * 0.36; Col = [System.Drawing.Color]::FromArgb(40, 20, 122, 110) },
        @{ X = $W * 0.85; Y = $H * 0.78; R = $W * 0.26; Col = [System.Drawing.Color]::FromArgb(24, 255, 206, 92) }
    )) {
        $gp = New-Object System.Drawing.Drawing2D.GraphicsPath
        $gp.AddEllipse([float]($glow.X - $glow.R), [float]($glow.Y - $glow.R), [float]($glow.R * 2), [float]($glow.R * 2))
        $pgb = New-Object System.Drawing.Drawing2D.PathGradientBrush($gp)
        $pgb.CenterColor = $glow.Col
        $pgb.SurroundColors = @([System.Drawing.Color]::FromArgb(0, 0, 0, 0))
        $G.FillPath($pgb, $gp)
        $pgb.Dispose()
        $gp.Dispose()
    }

    # Subtle 8-point geometric ornament watermark
    $ox = $W * 0.89
    $oy = $H * 0.14
    $os = [Math]::Min($W, $H) * 0.16
    $starPen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(22, 45, 212, 191), 1.5)
    foreach ($deg in @(0, 45)) {
        $G.TranslateTransform($ox, $oy)
        $G.RotateTransform($deg)
        $G.DrawRectangle($starPen, [float](-$os / 2), [float](-$os / 2), [float]$os, [float]$os)
        $G.ResetTransform()
    }
    $starPen.Dispose()
}

function Draw-StringCentered {
    param(
        [System.Drawing.Graphics]$G, [string]$Text, [System.Drawing.Font]$Font,
        [System.Drawing.Color]$Color, [float]$X, [float]$Y, [float]$W, [float]$H
    )
    $sf = New-Object System.Drawing.StringFormat
    $sf.Alignment = [System.Drawing.StringAlignment]::Center
    $sf.LineAlignment = [System.Drawing.StringAlignment]::Center
    $b = New-Object System.Drawing.SolidBrush($Color)
    $G.DrawString($Text, $Font, $b, [System.Drawing.RectangleF]::new($X, $Y, $W, $H), $sf)
    $b.Dispose()
    $sf.Dispose()
}

function Draw-StringAt {
    param(
        [System.Drawing.Graphics]$G, [string]$Text, [System.Drawing.Font]$Font,
        [System.Drawing.Color]$Color, [float]$X, [float]$Y
    )
    $b = New-Object System.Drawing.SolidBrush($Color)
    $G.DrawString($Text, $Font, $b, $X, $Y)
    $b.Dispose()
}

function Draw-Pill {
    param(
        [System.Drawing.Graphics]$G, [string]$Text, [System.Drawing.Font]$Font,
        [float]$X, [float]$Y, [float]$H = 38,
        [System.Drawing.Color]$BgColor = $C.SurfaceCard,
        [System.Drawing.Color]$BorderColor = $C.Border,
        [System.Drawing.Color]$TextColor = $C.TextPrimary,
        [bool]$WithDot = $true
    )
    $sz = $G.MeasureString($Text, $Font)
    $dotPad = if ($WithDot) { 20 } else { 0 }
    $w = [Math]::Ceiling($sz.Width) + 34 + $dotPad
    Draw-SolidRoundedRect -G $G -X $X -Y $Y -W $w -H $H -R ($H / 2) -FillColor $BgColor -StrokeColor $BorderColor -StrokeWidth 1.2
    $tx = $X + 17
    if ($WithDot) {
        $dotB = New-Object System.Drawing.SolidBrush($C.Accent)
        $G.FillEllipse($dotB, $X + 14, $Y + ($H - 8) / 2, 8, 8)
        $dotB.Dispose()
        $tx = $X + 30
    }
    $sf = New-Object System.Drawing.StringFormat
    $sf.LineAlignment = [System.Drawing.StringAlignment]::Center
    $tb = New-Object System.Drawing.SolidBrush($TextColor)
    $G.DrawString($Text, $Font, $tb, [System.Drawing.RectangleF]::new($tx, $Y, $w - ($tx - $X), $H), $sf)
    $tb.Dispose()
    $sf.Dispose()
    return $w
}

function Draw-PillRowCentered {
    param([System.Drawing.Graphics]$G, [string[]]$Items, [float]$CenterX, [float]$Y, [float]$H = 40)
    $f = New-Font -Size 19 -Weight "Bold"
    $gap = 14
    $widths = @()
    $total = 0
    foreach ($it in $Items) {
        $sz = $G.MeasureString($it, $f)
        $pw = [Math]::Ceiling($sz.Width) + 54
        $widths += $pw
        $total += $pw
    }
    $total += $gap * ([Math]::Max(0, $Items.Count - 1))
    $cx = $CenterX - ($total / 2)
    for ($i = 0; $i -lt $Items.Count; $i++) {
        $drawn = Draw-Pill -G $G -Text $Items[$i] -Font $f -X $cx -Y $Y -H $H -BgColor ([System.Drawing.Color]::FromArgb(220, 20, 55, 53)) -BorderColor $C.BorderAccent -WithDot $true
        $cx += $drawn + $gap
    }
    $f.Dispose()
}

function Draw-Headline {
    param([System.Drawing.Graphics]$G, [string]$Title, [string]$Sub, [int]$W, [float]$TopY = 48)
    $fTitle = New-Font -Size 52 -Weight "Bold" -Display $true
    $fSub = New-Font -Size 24 -Weight "Regular"
    Draw-StringCentered -G $G -Text $Title -Font $fTitle -Color $C.TextPrimary -X 80 -Y $TopY -W ($W - 160) -H 66
    Draw-StringCentered -G $G -Text $Sub -Font $fSub -Color $C.TextMuted -X 120 -Y ($TopY + 70) -W ($W - 240) -H 38
    $fTitle.Dispose()
    $fSub.Dispose()
}

function Draw-FluentWindow {
    param(
        [System.Drawing.Graphics]$G, [float]$X, [float]$Y, [float]$W, [float]$H,
        [string]$WindowTitle, [System.Drawing.Color]$Border = $C.Border
    )
    # Soft drop shadow layers
    for ($i = 3; $i -ge 1; $i--) {
        $pad = $i * 8
        Draw-SolidRoundedRect -G $G -X ($X - $pad * 0.3) -Y ($Y + $pad * 0.6) -W ($W + $pad * 0.6) -H ($H + $pad * 0.4) -R 18 `
            -FillColor ([System.Drawing.Color]::FromArgb(28, 0, 0, 0))
    }
    Draw-SolidRoundedRect -G $G -X $X -Y $Y -W $W -H $H -R 16 -FillColor $C.Surface -StrokeColor $Border -StrokeWidth 1.5

    if ($WindowTitle) {
        Draw-BrandIcon -G $G -X ($X + 20) -Y ($Y + 14) -Size 28 -WithTile $true
        $tf = New-Font -Size 16 -Weight "Bold"
        Draw-StringAt -G $G -Text $WindowTitle -Font $tf -Color $C.TextPrimary -X ($X + 58) -Y ($Y + 18)
        $tf.Dispose()

        # Subtle title divider
        $divPen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(35, 255, 255, 255), 1.0)
        $G.DrawLine($divPen, $X + 1, $Y + 54, $X + $W - 1, $Y + 54)
        $divPen.Dispose()
    }
}

function Draw-Win11Taskbar {
    param(
        [System.Drawing.Graphics]$G, [int]$W, [int]$H,
        [string]$BadgePrimary, [string]$BadgeSecondary, [string]$ClockTime, [string]$ClockDate
    )
    $tbH = 68
    $tbY = $H - $tbH
    $tbBrush = New-Object System.Drawing.SolidBrush($C.TaskbarBg)
    $G.FillRectangle($tbBrush, 0, $tbY, $W, $tbH)
    $tbBrush.Dispose()

    $topLine = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(50, 255, 255, 255), 1.0)
    $G.DrawLine($topLine, 0, $tbY, $W, $tbY)
    $topLine.Dispose()

    # Centered Win11 app icons (Start + Search + Explorer + Terminal)
    $startX = ($W / 2) - 110
    $iy = $tbY + 14
    $startBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 0, 164, 239))
    foreach ($sq in @(@(0,0), @(19,0), @(0,19), @(19,19))) {
        $G.FillRectangle($startBrush, $startX + $sq[0], $iy + 2 + $sq[1], 16, 16)
    }
    $startBrush.Dispose()
    Draw-SolidRoundedRect -G $G -X ($startX + 56) -Y ($iy + 2) -W 36 -H 36 -R 8 -FillColor ([System.Drawing.Color]::FromArgb(255, 50, 58, 70))
    Draw-SolidRoundedRect -G $G -X ($startX + 110) -Y ($iy + 2) -W 36 -H 36 -R 8 -FillColor ([System.Drawing.Color]::FromArgb(255, 244, 184, 68))
    Draw-SolidRoundedRect -G $G -X ($startX + 164) -Y ($iy + 2) -W 36 -H 36 -R 8 -FillColor ([System.Drawing.Color]::FromArgb(255, 45, 212, 191))

    # Right system tray area with PrayerBar docked pill
    $bandW = 196
    $bandX = $W - 420
    $bandRectX = $bandX
    Draw-SolidRoundedRect -G $G -X $bandRectX -Y ($tbY + 8) -W $bandW -H 52 -R 10 `
        -FillColor ([System.Drawing.Color]::FromArgb(255, 16, 70, 66)) -StrokeColor $C.Accent -StrokeWidth 2.0

    Draw-BrandIcon -G $G -X ($bandRectX + 10) -Y ($tbY + 17) -Size 34 -WithTile $true
    $bf1 = New-Font -Size 15 -Weight "Bold"
    $bf2 = New-Font -Size 17 -Weight "Bold"
    Draw-StringAt -G $G -Text $BadgePrimary -Font $bf1 -Color $C.TextMuted -X ($bandRectX + 54) -Y ($tbY + 12)
    Draw-StringAt -G $G -Text $BadgeSecondary -Font $bf2 -Color $C.Gold -X ($bandRectX + 54) -Y ($tbY + 32)
    $bf1.Dispose()
    $bf2.Dispose()

    # Tray chevron & system status dots
    $trayX = $bandRectX + $bandW + 22
    Draw-SolidRoundedRect -G $G -X $trayX -Y ($tbY + 22) -W 22 -H 22 -R 6 -FillColor ([System.Drawing.Color]::FromArgb(40, 255, 255, 255))
    Draw-SolidRoundedRect -G $G -X ($trayX + 34) -Y ($tbY + 22) -W 22 -H 22 -R 6 -FillColor ([System.Drawing.Color]::FromArgb(40, 255, 255, 255))
    Draw-SolidRoundedRect -G $G -X ($trayX + 66) -Y ($tbY + 22) -W 22 -H 22 -R 6 -FillColor ([System.Drawing.Color]::FromArgb(40, 255, 255, 255))

    # System clock
    $cf1 = New-Font -Size 16 -Weight "Bold"
    $cf2 = New-Font -Size 14 -Weight "Regular"
    Draw-StringAt -G $G -Text $ClockTime -Font $cf1 -Color $C.TextPrimary -X ($W - 96) -Y ($tbY + 12)
    Draw-StringAt -G $G -Text $ClockDate -Font $cf2 -Color $C.TextMuted -X ($W - 108) -Y ($tbY + 34)
    $cf1.Dispose()
    $cf2.Dispose()
}

# ==============================================================================
# Localized String Tables
# ==============================================================================
$Strings = @{
    en = @{
        s1_title = "Live Prayer Countdown on Your Windows 11 Taskbar"
        s1_sub   = "Always visible beside your system tray with 0% idle CPU and ~3 MB RAM"
        s1_pills = @("~3 MB RAM", "0% Idle CPU", "4 Taskbar Positions", "9 Display Styles")
        s1_zoom_cap = "Taskbar Preview (Asr in 23 min)"
        s1_band_top = "Asr Prayer"
        s1_band_bot = "in 23 min  15:25"
        s1_badge_p  = "Asr  15:25"
        s1_badge_s  = "23m left"
        s1_card_t   = "Never Slows Down Your PC"
        s1_card_l   = @("Uses Only ~3 MB Memory (Zero Lag)", "0% Background CPU & Battery Friendly", "Auto-Mutes During Games & Fullscreen", "9 Taskbar Styles + Custom Text Format")

        s2_title = "4 Windows 11 Fluent Flyouts & Live Qibla Compass"
        s2_sub   = "Click the taskbar countdown anytime for daily prayer times and instant Qibla bearing"
        s2_city  = "Istanbul, Turkey  (Diyanet Method)"
        s2_hijri = "14 Sha'ban 1448 AH"
        s2_next  = "Next: Asr at 15:25"
        s2_rem   = "23 min remaining"
        s2_prayers = @(
            @("Fajr", "06:12", $false),
            @("Sunrise", "07:41", $false),
            @("Dhuhr", "12:58", $false),
            @("Asr", "15:25", $true),
            @("Maghrib", "18:04", $false),
            @("Isha", "19:26", $false)
        )
        s2_qibla_h = "Live Qibla Compass"
        s2_qibla_deg = T "151\u00b0 SE toward Makkah"
        s2_qibla_sub = "Auto-calculated from your city coordinates"
        s2_flyouts   = "Includes 4 Flyout Styles: Compact, Minimal Glass, Bento Grid & Timeline"

        s3_title = "Monthly Schedule & 30-Day Ramadan Imsakiye"
        s3_sub   = "Special Iftar & Suhoor countdown mode with one-click CSV & printable HTML export"
        s3_win_t = "PrayerBar - Monthly & Ramadan Imsakiye (2027)"
        s3_iftar = "Ramadan Mode Active  |  Iftar in 02h 14m  (18:04)"
        s3_cols  = @("Day", "Date", "Fajr / Imsak", "Sunrise", "Dhuhr", "Asr", "Maghrib / Iftar", "Isha")
        s3_btn1  = "Export CSV"
        s3_btn2  = "Print HTML Calendar"
        s3_badge = "23 Global Methods + 100% Offline Fallback"

        s4_title = "12 Languages, Custom Alerts & Full User Control"
        s4_sub   = "Customize every reminder, Adhan audio, shortcut and optional spiritual tools"
        s4_win_t = "PrayerBar - Settings & Features"
        s4_tabs  = @("General & City", "Taskbar Look", "Adhan & Alerts", "Features")
        s4_mods  = @(
            @("Jumu'ah (Friday) Reminder", "Notify 45 min before Friday Dhuhr prayer", $true),
            @("Islamic Holiday Alerts", "Ramadan, Eid, Ashura & blessed nights notifications", $true),
            @("Daily Prayer Tracker", "Check off completed prayers with weekly stats", $true),
            @("Duha, Midnight & Tahajjud", "Optional extra Sunnah & night time badges", $true),
            @("Travel Mode (Time-Zone Sync)", "Auto-detect city shifts & adjust schedule cleanly", $true),
            @("Fullscreen / Game Quiet Mode", "Mute Adhan automatically while gaming or presenting", $true)
        )
        s4_right_h = "Global & Shortcut Controls"
        s4_lang_k  = "Interface Language"
        s4_lang_v  = "12 Languages (EN, TR, AR, DE, FR, ID, UR, BN, MS, FA, RU, ES)"
        s4_meth_k  = "Calculation Method"
        s4_meth_v  = "23 Global Methods + Hanafi / Standard Asr"
        s4_key_k   = "Customizable Global Shortcut"
        s4_key_v   = "Win + Alt + P"
        s4_free_k  = "Privacy & Pricing"
        s4_free_v  = T "0% Telemetry  \u2022  Core App 100% Free & Ad-Free"
    }
    tr = @{
        s1_title = T "Windows 11 G\u00f6rev \u00c7ubu\u011funda Canl\u0131 Vakit Sayac\u0131"
        s1_sub   = T "Saat yan\u0131nda her an g\u00f6r\u00fcn\u00fcr  \u2022  Bo\u015fta %0 CPU  \u2022  Sadece ~3 MB RAM"
        s1_pills = @("~3 MB RAM", (T "Bo\u015fta %0 CPU"), (T "4 G\u00f6rev \u00c7ubu\u011fu Konumu"), (T "9 G\u00f6r\u00fcn\u00fcm Stili"))
        s1_zoom_cap = T "G\u00f6rev \u00c7ubu\u011fu \u00d6nizlemesi (\u0130kindiye 23 dk)"
        s1_band_top = T "\u0130kindi Vakti"
        s1_band_bot = T "23 dk kald\u0131  \u00b7  15:25"
        s1_badge_p  = T "\u0130kindi \u00b7 15:25"
        s1_badge_s  = T "23 dk kald\u0131"
        s1_card_t   = T "Bilgisayar\u0131n\u0131z\u0131 Hi\u00e7 Yormaz"
        s1_card_l   = @(
            (T "Sadece ~3 MB Bellek (S\u0131f\u0131r Kasma)"),
            (T "Arka Planda %0 CPU ve Pil Dostu"),
            (T "Oyun ve Tam Ekranda Otomatik Sessiz"),
            (T "9 G\u00f6r\u00fcn\u00fcm Stili + \u00d6zel Metin Bi\u00e7imi")
        )

        s2_title = T "4 Fluent A\u00e7\u0131l\u0131r Ekran & Canl\u0131 K\u0131ble Pusulas\u0131"
        s2_sub   = T "Tek t\u0131klamayla g\u00fcnl\u00fck vakitler, kalan s\u00fcre \u00e7ubu\u011fu ve k\u0131ble a\u00e7\u0131s\u0131 elinizin alt\u0131nda"
        s2_city  = T "\u0130stanbul, T\u00fcrkiye  (Diyanet Hesaplamas\u0131)"
        s2_hijri = T "14 \u015eaban 1448"
        s2_next  = T "S\u0131radaki: \u0130kindi (15:25)"
        s2_rem   = T "23 dakika kald\u0131"
        s2_prayers = @(
            @((T "\u0130msak"), "06:12", $false),
            @((T "G\u00fcne\u015f"), "07:41", $false),
            @((T "\u00d6\u011fle"), "12:58", $false),
            @((T "\u0130kindi"), "15:25", $true),
            @((T "Ak\u015fam"), "18:04", $false),
            @((T "Yats\u0131"), "19:26", $false)
        )
        s2_qibla_h = T "Canl\u0131 K\u0131ble Pusulas\u0131"
        s2_qibla_deg = T "151\u00b0 G\u00fcneydo\u011fu  \u2022  Kabe Y\u00f6n\u00fc"
        s2_qibla_sub = T "\u015eehrinizin koordinatlar\u0131na g\u00f6re otomatik hesaplan\u0131r"
        s2_flyouts   = T "4 Farkl\u0131 Tasar\u0131m: Kompakt, Minimal Cam, Bento Kartlar ve Zaman \u00c7izelgesi"

        s3_title = T "Ayl\u0131k Takvim ve 30 G\u00fcnl\u00fck Ramazan \u0130msakiyesi"
        s3_sub   = T "\u0130ftar ve Sahur \u00f6zel geri say\u0131m modu + Tek t\u0131kla CSV ve Yazd\u0131r\u0131labilir HTML"
        s3_win_t = T "PrayerBar - Ayl\u0131k Takvim & Ramazan \u0130msakiyesi"
        s3_iftar = T "Ramazan Modu Aktif  |  \u0130ftara kalan 02 sa 14 dk  (18:04)"
        s3_cols  = @((T "G\u00fcn"), "Tarih", (T "\u0130msak"), (T "G\u00fcne\u015f"), (T "\u00d6\u011fle"), (T "\u0130kindi"), (T "Ak\u015fam / \u0130ftar"), (T "Yats\u0131"))
        s3_btn1  = T "CSV Olarak Kaydet"
        s3_btn2  = T "HTML Takvim Yazd\u0131r"
        s3_badge = T "23 K\u00fcresel Metot + %100 \u00c7evrimd\u0131\u015f\u0131 G\u00fcne\u015f Hesab\u0131"

        s4_title = T "12 Dil, \u00d6zel Hat\u0131rlatmalar ve Tam Kontrol"
        s4_sub   = T "Ezan sesi, vaktinden \u00f6nce uyar\u0131, k\u0131sayol tu\u015fu ve iste\u011fe ba\u011fl\u0131 t\u00fcm \u00f6zellikler"
        s4_win_t = T "PrayerBar - Ayarlar ve \u00d6zellikler"
        s4_tabs  = @((T "Genel & \u015eehir"), (T "G\u00f6r\u00fcn\u00fcm"), (T "Ezan & Uyar\u0131lar"), (T "\u00d6zellikler"))
        s4_mods  = @(
            @((T "Cuma Namaz\u0131 Hat\u0131rlatmas\u0131"), (T "Cuma g\u00fcnleri \u00f6\u011fleden 45 dk \u00f6nce uyar\u0131"), $true),
            @((T "Dini G\u00fcn ve Kandil Uyar\u0131lar\u0131"), (T "Ramazan, Bayram, A\u015fure ve m\u00fcbarek geceler"), $true),
            @((T "G\u00fcnl\u00fck Namaz Takibi"), (T "K\u0131l\u0131nan vakitleri i\u015faretleme ve haftal\u0131k \u00f6zet"), $true),
            @((T "Ku\u015fluk, Gece Yar\u0131s\u0131 & Tehecc\u00fcd"), (T "\u0130ste\u011fe ba\u011fl\u0131 nafile ve gece vakitleri"), $true),
            @((T "Seyahat Modu (Saat Dilimi)"), (T "\u015eehir veya saat dilimi de\u011fi\u015fikli\u011fini otomatik alg\u0131lar"), $true),
            @((T "Tam Ekran / Oyun Sessiz Modu"), (T "Oyun veya sunum s\u0131ras\u0131nda ezan sesini otomatik susturur"), $true)
        )
        s4_right_h = T "K\u00fcresel Ayarlar ve K\u0131sayol"
        s4_lang_k  = T "Uygulama Dili"
        s4_lang_v  = T "12 Dil Deste\u011fi + Namaz \u0130simlerini Manuel D\u00fczenleme"
        s4_meth_k  = T "Hesaplama Metodu"
        s4_meth_v  = T "Diyanet \u0130\u015fleri Ba\u015fkanl\u0131\u011f\u0131 + 22 K\u00fcresel Metot"
        s4_key_k   = T "\u00d6zelle\u015ftirilebilir K\u0131sayol Tu\u015fu"
        s4_key_v   = "Win + Alt + P"
        s4_free_k  = T "Gizlilik ve Lisans"
        s4_free_v  = T "S\u0131f\u0131r Telemetri  \u2022  Ana \u00d6zellikler %100 \u00dccretsiz & Reklams\u0131z"
    }
}

# ==============================================================================
# Logo & Promotional Art Renderers
# ==============================================================================
function Save-SquareStoreArt {
    param([string]$OutPath, [int]$Size, [bool]$IncludeWordmark)
    $bmp = New-Object System.Drawing.Bitmap($Size, $Size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    Draw-Backdrop -G $g -W $Size -H $Size

    if ($IncludeWordmark) {
        $iconS = $Size * 0.44
        Draw-BrandIcon -G $g -X (($Size - $iconS) / 2) -Y ($Size * 0.14) -Size $iconS -WithTile $true
        $fMain = New-Font -Size ($Size * 0.105) -Weight "Bold" -Display $true
        $fSub = New-Font -Size ($Size * 0.045) -Weight "Regular"
        Draw-StringCentered -G $g -Text "PrayerBar" -Font $fMain -Color $C.TextPrimary -X 40 -Y ($Size * 0.62) -W ($Size - 80) -H ($Size * 0.13)
        Draw-StringCentered -G $g -Text "Prayer Times & Adhan" -Font $fSub -Color $C.Accent -X 40 -Y ($Size * 0.75) -W ($Size - 80) -H ($Size * 0.07)
        $fMain.Dispose()
        $fSub.Dispose()
    } else {
        $iconS = $Size * 0.72
        Draw-BrandIcon -G $g -X (($Size - $iconS) / 2) -Y (($Size - $iconS) / 2) -Size $iconS -WithTile $true
    }

    $g.Dispose()
    $bmp.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

function Save-PosterArt720x1080 {
    param([string]$OutPath)
    $w = 720; $h = 1080
    $bmp = New-Object System.Drawing.Bitmap($w, $h, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    Draw-Backdrop -G $g -W $w -H $h

    Draw-BrandIcon -G $g -X (($w - 300) / 2) -Y 110 -Size 300 -WithTile $true
    $fTitle = New-Font -Size 72 -Weight "Bold" -Display $true
    $fSub = New-Font -Size 30 -Weight "Regular"
    Draw-StringCentered -G $g -Text "PrayerBar" -Font $fTitle -Color $C.TextPrimary -X 40 -Y 445 -W ($w - 80) -H 85
    Draw-StringCentered -G $g -Text "Prayer Times & Adhan" -Font $fSub -Color $C.Accent -X 40 -Y 532 -W ($w - 80) -H 45
    $fTitle.Dispose()
    $fSub.Dispose()

    # Mini Taskbar Card Preview
    Draw-FluentWindow -G $g -X 80 -Y 645 -W 560 -H 145 -WindowTitle $null -Border $C.BorderAccent
    Draw-BrandIcon -G $g -X 112 -Y 680 -Size 74 -WithTile $true
    $fB1 = New-Font -Size 24 -Weight "Bold"
    $fB2 = New-Font -Size 32 -Weight "Bold" -Display $true
    Draw-StringAt -G $g -Text "Next: Asr (15:25)" -Font $fB1 -Color $C.TextMuted -X 206 -Y 678
    Draw-StringAt -G $g -Text "23 min left" -Font $fB2 -Color $C.Gold -X 206 -Y 716
    $fB1.Dispose()
    $fB2.Dispose()

    # Bottom Badges
    Draw-PillRowCentered -G $g -Items @("~3 MB RAM", "23 Methods", "12 Languages") -CenterX ($w / 2) -Y 860 -H 44
    Draw-PillRowCentered -G $g -Items @("0% Idle CPU", "100% Ad-Free") -CenterX ($w / 2) -Y 922 -H 44

    $g.Dispose()
    $bmp.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

function Save-HeroArt1920x1080 {
    param([string]$OutPath)
    $w = 1920; $h = 1080
    $bmp = New-Object System.Drawing.Bitmap($w, $h, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    Draw-Backdrop -G $g -W $w -H $h

    # Left hero copy
    Draw-BrandIcon -G $g -X 130 -Y 190 -Size 156 -WithTile $true
    $fH1 = New-Font -Size 84 -Weight "Bold" -Display $true
    $fH2 = New-Font -Size 36 -Weight "Regular"
    $fH3 = New-Font -Size 26 -Weight "Regular"
    Draw-StringAt -G $g -Text "PrayerBar" -Font $fH1 -Color $C.TextPrimary -X 130 -Y 375
    Draw-StringAt -G $g -Text "Windows 11 Prayer Times & Adhan" -Font $fH2 -Color $C.Accent -X 134 -Y 478
    Draw-StringAt -G $g -Text "Live taskbar countdown that never slows down your PC," -Font $fH3 -Color $C.TextMuted -X 134 -Y 538
    Draw-StringAt -G $g -Text "4 clean popups, Qibla compass & Ramadan Imsakiye." -Font $fH3 -Color $C.TextMuted -X 134 -Y 574
    $fH1.Dispose(); $fH2.Dispose(); $fH3.Dispose()

    $pf = New-Font -Size 22 -Weight "Bold"
    $px = 134
    foreach ($tag in @("~3 MB RAM", "0% Idle CPU", "23 Methods", "12 Languages")) {
        $pw = Draw-Pill -G $g -Text $tag -Font $pf -X $px -Y 645 -H 48 -BgColor ([System.Drawing.Color]::FromArgb(220, 20, 55, 53)) -BorderColor $C.BorderAccent
        $px += $pw + 14
    }
    $pf.Dispose()

    # Right hero visual: Fluent Flyout preview
    $S = $Strings.en
    Draw-FlyoutMockup -G $g -X 1120 -Y 155 -W 660 -H 720 -Lang $S

    Draw-Win11Taskbar -G $g -W $w -H $h -BadgePrimary "Asr  15:25" -BadgeSecondary "23m left" -ClockTime "15:02" -ClockDate "22.11.2026"
    $g.Dispose()
    $bmp.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

# ==============================================================================
# Reusable UI Scene Mockups (1920x1080)
# ==============================================================================
function Draw-FlyoutMockup {
    param([System.Drawing.Graphics]$G, [float]$X, [float]$Y, [float]$W, [float]$H, [hashtable]$Lang)
    Draw-FluentWindow -G $G -X $X -Y $Y -W $W -H $H -WindowTitle $Lang.s2_city -Border $C.BorderAccent

    $fHijri = New-Font -Size 18 -Weight "Regular"
    Draw-StringAt -G $G -Text $Lang.s2_hijri -Font $fHijri -Color $C.Accent -X ($X + 32) -Y ($Y + 72)
    $fHijri.Dispose()

    # Hero countdown banner inside flyout
    Draw-SolidRoundedRect -G $G -X ($X + 30) -Y ($Y + 112) -W ($W - 60) -H 118 -R 14 -FillColor $C.SurfaceHi -StrokeColor $C.BorderAccent -StrokeWidth 1.5
    $fNext = New-Font -Size 22 -Weight "Bold"
    $fRem = New-Font -Size 34 -Weight "Bold" -Display $true
    Draw-StringAt -G $G -Text $Lang.s2_next -Font $fNext -Color $C.TextPrimary -X ($X + 54) -Y ($Y + 128)
    Draw-StringAt -G $G -Text $Lang.s2_rem -Font $fRem -Color $C.Gold -X ($X + 54) -Y ($Y + 166)
    $fNext.Dispose(); $fRem.Dispose()

    # Progress bar
    Draw-SolidRoundedRect -G $G -X ($X + 30) -Y ($Y + 244) -W ($W - 60) -H 8 -R 4 -FillColor ([System.Drawing.Color]::FromArgb(70, 255, 255, 255))
    Draw-SolidRoundedRect -G $G -X ($X + 30) -Y ($Y + 244) -W (($W - 60) * 0.78) -H 8 -R 4 -FillColor $C.Accent

    # 6 prayer rows
    $fRow = New-Font -Size 22 -Weight "Bold"
    $fTime = New-Font -Size 22 -Weight "Bold"
    $ry = $Y + 276
    foreach ($p in $Lang.s2_prayers) {
        $isAct = [bool]$p[2]
        $rowBg = if ($isAct) { $C.SurfaceHi } else { $C.SurfaceCard }
        $rowBr = if ($isAct) { $C.Accent } else { $C.Border }
        Draw-SolidRoundedRect -G $G -X ($X + 30) -Y $ry -W ($W - 60) -H 58 -R 10 -FillColor $rowBg -StrokeColor $rowBr -StrokeWidth $(if ($isAct) { 2.0 } else { 1.0 })
        if ($isAct) {
            Draw-SolidRoundedRect -G $G -X ($X + 38) -Y ($ry + 12) -W 6 -H 34 -R 3 -FillColor $C.Gold
        }
        $tc = if ($isAct) { $C.Gold } else { $C.TextPrimary }
        Draw-StringAt -G $G -Text $p[0] -Font $fRow -Color $tc -X ($X + 60) -Y ($ry + 15)
        Draw-StringAt -G $G -Text $p[1] -Font $fTime -Color $tc -X ($X + $W - 125) -Y ($ry + 15)
        $ry += 68
    }
    $fRow.Dispose(); $fTime.Dispose()
}

function Save-Screenshot1 {
    param([string]$OutPath, [hashtable]$Lang)
    $w = 1920; $h = 1080
    $bmp = New-Object System.Drawing.Bitmap($w, $h, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    Draw-Backdrop -G $g -W $w -H $h
    Draw-Headline -G $g -Title $Lang.s1_title -Sub $Lang.s1_sub -W $w -TopY 48
    Draw-PillRowCentered -G $g -Items $Lang.s1_pills -CenterX ($w / 2) -Y 178 -H 42

    # Left specs card
    Draw-FluentWindow -G $g -X 140 -Y 285 -W 740 -H 480 -WindowTitle $Lang.s1_card_t -Border $C.Border
    $fItem = New-Font -Size 26 -Weight "Bold"
    $iy = 385
    foreach ($line in $Lang.s1_card_l) {
        Draw-SolidRoundedRect -G $g -X 178 -Y $iy -W 664 -H 68 -R 12 -FillColor $C.SurfaceCard -StrokeColor $C.Border
        Draw-SolidRoundedRect -G $g -X 200 -Y ($iy + 24) -W 20 -H 20 -R 10 -FillColor $C.Accent
        Draw-StringAt -G $g -Text $line -Font $fItem -Color $C.TextPrimary -X 240 -Y ($iy + 17)
        $iy += 88
    }
    $fItem.Dispose()

    # Right zoomed taskbar callout card
    Draw-FluentWindow -G $g -X 950 -Y 310 -W 830 -H 370 -WindowTitle $Lang.s1_zoom_cap -Border $C.BorderAccent
    Draw-SolidRoundedRect -G $g -X 995 -Y 405 -W 740 -H 195 -R 22 -FillColor ([System.Drawing.Color]::FromArgb(255, 14, 52, 50)) -StrokeColor $C.Accent -StrokeWidth 2.5
    Draw-BrandIcon -G $g -X 1035 -Y 442 -Size 120 -WithTile $true
    $fZ1 = New-Font -Size 34 -Weight "Bold"
    $fZ2 = New-Font -Size 48 -Weight "Bold" -Display $true
    Draw-StringAt -G $g -Text $Lang.s1_band_top -Font $fZ1 -Color $C.TextMuted -X 1190 -Y 442
    Draw-StringAt -G $g -Text $Lang.s1_band_bot -Font $fZ2 -Color $C.Gold -X 1190 -Y 496
    $fZ1.Dispose(); $fZ2.Dispose()

    # Dashed callout line pointing down toward real taskbar PrayerBar badge
    $dashPen = New-Object System.Drawing.Pen($C.Accent, 3.0)
    $dashPen.DashStyle = [System.Drawing.Drawing2D.DashStyle]::Dash
    $g.DrawLine($dashPen, 1595, 680, 1595, 1004)
    $dashPen.Dispose()

    Draw-Win11Taskbar -G $g -W $w -H $h -BadgePrimary $Lang.s1_badge_p -BadgeSecondary $Lang.s1_badge_s -ClockTime "15:02" -ClockDate "22.11.2026"
    $g.Dispose()
    $bmp.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

function Save-Screenshot2 {
    param([string]$OutPath, [hashtable]$Lang)
    $w = 1920; $h = 1080
    $bmp = New-Object System.Drawing.Bitmap($w, $h, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    Draw-Backdrop -G $g -W $w -H $h
    Draw-Headline -G $g -Title $Lang.s2_title -Sub $Lang.s2_sub -W $w -TopY 44

    # Left: Main Fluent Flyout
    Draw-FlyoutMockup -G $g -X 180 -Y 205 -W 740 -H 740 -Lang $Lang

    # Right: Live Vector Qibla Compass Card + Flyout Styles Note
    Draw-FluentWindow -G $g -X 990 -Y 205 -W 750 -H 590 -WindowTitle $Lang.s2_qibla_h -Border $C.BorderAccent
    $cx = 1365; $cy = 485; $cr = 175
    $ringPen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(120, 45, 212, 191), 3.0)
    $g.DrawEllipse($ringPen, $cx - $cr, $cy - $cr, $cr * 2, $cr * 2)
    $g.DrawEllipse($ringPen, $cx - ($cr * 0.72), $cy - ($cr * 0.72), ($cr * 1.44), ($cr * 1.44))
    $ringPen.Dispose()

    $fComp = New-Font -Size 22 -Weight "Bold"
    Draw-StringCentered -G $g -Text "N" -Font $fComp -Color $C.TextMuted -X ($cx - 20) -Y ($cy - $cr - 34) -W 40 -H 28
    Draw-StringCentered -G $g -Text "S" -Font $fComp -Color $C.TextMuted -X ($cx - 20) -Y ($cy + $cr + 6) -W 40 -H 28
    Draw-StringCentered -G $g -Text "W" -Font $fComp -Color $C.TextMuted -X ($cx - $cr - 42) -Y ($cy - 14) -W 40 -H 28
    Draw-StringCentered -G $g -Text "E" -Font $fComp -Color $C.TextMuted -X ($cx + $cr + 4) -Y ($cy - 14) -W 40 -H 28
    $fComp.Dispose()

    # Compass needle tilted to 151 deg SE (clockwise from top N = 151 - 90 = 61 deg in screen space)
    $rad = (151.0 - 90.0) * [Math]::PI / 180.0
    $nx = $cx + [Math]::Cos($rad) * ($cr * 0.74)
    $ny = $cy + [Math]::Sin($rad) * ($cr * 0.74)
    $needlePen = New-Object System.Drawing.Pen($C.Gold, 7.0)
    $needlePen.EndCap = [System.Drawing.Drawing2D.LineCap]::ArrowAnchor
    $g.DrawLine($needlePen, [float]$cx, [float]$cy, [float]$nx, [float]$ny)
    $needlePen.Dispose()
    $hubB = New-Object System.Drawing.SolidBrush($C.Accent)
    $g.FillEllipse($hubB, $cx - 12, $cy - 12, 24, 24)
    $hubB.Dispose()

    $fDeg = New-Font -Size 26 -Weight "Bold"
    $fQSub = New-Font -Size 19 -Weight "Regular"
    Draw-StringCentered -G $g -Text $Lang.s2_qibla_deg -Font $fDeg -Color $C.Gold -X 1020 -Y 705 -W 690 -H 36
    Draw-StringCentered -G $g -Text $Lang.s2_qibla_sub -Font $fQSub -Color $C.TextMuted -X 1020 -Y 744 -W 690 -H 30
    $fDeg.Dispose(); $fQSub.Dispose()

    Draw-SolidRoundedRect -G $g -X 990 -Y 825 -W 750 -H 120 -R 14 -FillColor $C.SurfaceCard -StrokeColor $C.Border
    $fNote = New-Font -Size 22 -Weight "Bold"
    Draw-StringCentered -G $g -Text $Lang.s2_flyouts -Font $fNote -Color $C.TextPrimary -X 1015 -Y 845 -W 700 -H 80
    $fNote.Dispose()

    Draw-Win11Taskbar -G $g -W $w -H $h -BadgePrimary $Lang.s1_badge_p -BadgeSecondary $Lang.s1_badge_s -ClockTime "15:02" -ClockDate "22.11.2026"
    $g.Dispose()
    $bmp.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

function Save-Screenshot3 {
    param([string]$OutPath, [hashtable]$Lang)
    $w = 1920; $h = 1080
    $bmp = New-Object System.Drawing.Bitmap($w, $h, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    Draw-Backdrop -G $g -W $w -H $h
    Draw-Headline -G $g -Title $Lang.s3_title -Sub $Lang.s3_sub -W $w -TopY 44

    Draw-FluentWindow -G $g -X 150 -Y 195 -W 1620 -H 770 -WindowTitle $Lang.s3_win_t -Border $C.BorderAccent

    # Top Ramadan Banner + Action Buttons
    Draw-SolidRoundedRect -G $g -X 190 -Y 275 -W 820 -H 54 -R 12 -FillColor $C.SurfaceHi -StrokeColor $C.BorderAccent
    $fIftar = New-Font -Size 21 -Weight "Bold"
    Draw-StringAt -G $g -Text $Lang.s3_iftar -Font $fIftar -Color $C.Gold -X 215 -Y 289
    $fIftar.Dispose()

    $fBtn = New-Font -Size 20 -Weight "Bold"
    $bw2 = [Math]::Ceiling($g.MeasureString($Lang.s3_btn2, $fBtn).Width) + 34
    $bw1 = [Math]::Ceiling($g.MeasureString($Lang.s3_btn1, $fBtn).Width) + 34
    $bx2 = 1730 - $bw2
    $bx1 = $bx2 - 16 - $bw1
    Draw-Pill -G $g -Text $Lang.s3_btn1 -Font $fBtn -X $bx1 -Y 279 -H 46 -BgColor $C.SurfaceCard -BorderColor $C.Border -WithDot $false | Out-Null
    Draw-Pill -G $g -Text $Lang.s3_btn2 -Font $fBtn -X $bx2 -Y 279 -H 46 -BgColor $C.SurfaceHi -BorderColor $C.Accent -WithDot $false | Out-Null
    $fBtn.Dispose()

    # Table Header
    $colX = @(205, 310, 505, 700, 880, 1060, 1245, 1505)
    Draw-SolidRoundedRect -G $g -X 190 -Y 350 -W 1540 -H 52 -R 10 -FillColor ([System.Drawing.Color]::FromArgb(255, 38, 49, 62))
    $fCol = New-Font -Size 20 -Weight "Bold"
    for ($col = 0; $col -lt $Lang.s3_cols.Count; $col++) {
        Draw-StringAt -G $g -Text $Lang.s3_cols[$col] -Font $fCol -Color $C.Accent -X $colX[$col] -Y 364
    }
    $fCol.Dispose()

    # 8 Clean Rows with Day 14 highlighted
    $rows = @(
        @("11", "01.03", "05:58", "07:22", "13:18", "16:28", "18:56", "20:14", $false),
        @("12", "02.03", "05:56", "07:21", "13:18", "16:29", "18:57", "20:15", $false),
        @("13", "03.03", "05:55", "07:19", "13:17", "16:30", "18:58", "20:16", $false),
        @("14", "04.03", "05:53", "07:17", "13:17", "16:31", "18:59", "20:17", $true),
        @("15", "05.03", "05:51", "07:16", "13:17", "16:32", "19:01", "20:18", $false),
        @("16", "06.03", "05:50", "07:14", "13:17", "16:33", "19:02", "20:20", $false),
        @("17", "07.03", "05:48", "07:13", "13:16", "16:34", "19:03", "20:21", $false),
        @("18", "08.03", "05:46", "07:11", "13:16", "16:35", "19:04", "20:22", $false)
    )
    $fCell = New-Font -Size 20 -Weight "Bold"
    $ry = 412
    foreach ($r in $rows) {
        $hi = [bool]$r[8]
        $bg = if ($hi) { $C.SurfaceHi } else { $C.SurfaceCard }
        $br = if ($hi) { $C.Accent } else { [System.Drawing.Color]::FromArgb(30, 255, 255, 255) }
        Draw-SolidRoundedRect -G $g -X 190 -Y $ry -W 1540 -H 54 -R 8 -FillColor $bg -StrokeColor $br -StrokeWidth $(if ($hi) { 2.0 } else { 1.0 })
        for ($col = 0; $col -lt 8; $col++) {
            $colColor = if ($hi) { $C.Gold } elseif ($col -eq 6) { $C.Accent } else { $C.TextPrimary }
            Draw-StringAt -G $g -Text $r[$col] -Font $fCell -Color $colColor -X $colX[$col] -Y ($ry + 14)
        }
        $ry += 62
    }
    $fCell.Dispose()

    $fFoot = New-Font -Size 20 -Weight "Bold"
    Draw-StringCentered -G $g -Text $Lang.s3_badge -Font $fFoot -Color $C.TextMuted -X 190 -Y 915 -W 1540 -H 32
    $fFoot.Dispose()

    Draw-Win11Taskbar -G $g -W $w -H $h -BadgePrimary $Lang.s1_badge_p -BadgeSecondary $Lang.s1_badge_s -ClockTime "15:02" -ClockDate "22.11.2026"
    $g.Dispose()
    $bmp.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

function Save-Screenshot4 {
    param([string]$OutPath, [hashtable]$Lang)
    $w = 1920; $h = 1080
    $bmp = New-Object System.Drawing.Bitmap($w, $h, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    Draw-Backdrop -G $g -W $w -H $h
    Draw-Headline -G $g -Title $Lang.s4_title -Sub $Lang.s4_sub -W $w -TopY 44

    Draw-FluentWindow -G $g -X 130 -Y 195 -W 1660 -H 770 -WindowTitle $Lang.s4_win_t -Border $C.BorderAccent

    # Tabs strip
    $fTab = New-Font -Size 20 -Weight "Bold"
    $tx = 170
    for ($i = 0; $i -lt $Lang.s4_tabs.Count; $i++) {
        $act = ($i -eq 3)
        $bg = if ($act) { $C.SurfaceHi } else { $C.SurfaceCard }
        $br = if ($act) { $C.Accent } else { $C.Border }
        $tw = Draw-Pill -G $g -Text $Lang.s4_tabs[$i] -Font $fTab -X $tx -Y 272 -H 44 -BgColor $bg -BorderColor $br -WithDot $act
        $tx += $tw + 14
    }
    $fTab.Dispose()

    # Left Column: Feature toggles
    $fModT = New-Font -Size 21 -Weight "Bold"
    $fModS = New-Font -Size 16 -Weight "Regular"
    $my = 342
    foreach ($m in $Lang.s4_mods) {
        Draw-SolidRoundedRect -G $g -X 170 -Y $my -W 840 -H 90 -R 12 -FillColor $C.SurfaceCard -StrokeColor $C.Border
        Draw-StringAt -G $g -Text $m[0] -Font $fModT -Color $C.TextPrimary -X 198 -Y ($my + 16)
        Draw-StringAt -G $g -Text $m[1] -Font $fModS -Color $C.TextMuted -X 198 -Y ($my + 50)
        # Fluent Toggle Switch ON
        Draw-SolidRoundedRect -G $g -X 920 -Y ($my + 29) -W 64 -H 32 -R 16 -FillColor $C.Accent
        Draw-SolidRoundedRect -G $g -X 958 -Y ($my + 34) -W 22 -H 22 -R 11 -FillColor $C.BgTop
        $my += 102
    }
    $fModT.Dispose(); $fModS.Dispose()

    # Right Column: Global Config & Shortcut Summary Card
    Draw-SolidRoundedRect -G $g -X 1055 -Y 342 -W 690 -H 598 -R 14 -FillColor $C.SurfaceHi -StrokeColor $C.BorderAccent -StrokeWidth 1.5
    $fRh = New-Font -Size 26 -Weight "Bold"
    $fRk = New-Font -Size 18 -Weight "Regular"
    $fRv = New-Font -Size 21 -Weight "Bold"
    Draw-StringAt -G $g -Text $Lang.s4_right_h -Font $fRh -Color $C.Gold -X 1090 -Y 372
    $ry = 432
    foreach ($kv in @(
        @($Lang.s4_lang_k, $Lang.s4_lang_v),
        @($Lang.s4_meth_k, $Lang.s4_meth_v),
        @($Lang.s4_key_k,  $Lang.s4_key_v),
        @($Lang.s4_free_k, $Lang.s4_free_v)
    )) {
        Draw-SolidRoundedRect -G $g -X 1090 -Y $ry -W 620 -H 106 -R 12 -FillColor $C.Surface -StrokeColor $C.Border
        Draw-StringAt -G $g -Text $kv[0] -Font $fRk -Color $C.Accent -X 1118 -Y ($ry + 18)
        Draw-StringAt -G $g -Text $kv[1] -Font $fRv -Color $C.TextPrimary -X 1118 -Y ($ry + 50)
        $ry += 122
    }
    $fRh.Dispose(); $fRk.Dispose(); $fRv.Dispose()

    Draw-Win11Taskbar -G $g -W $w -H $h -BadgePrimary $Lang.s1_badge_p -BadgeSecondary $Lang.s1_badge_s -ClockTime "15:02" -ClockDate "22.11.2026"
    $g.Dispose()
    $bmp.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

Write-Host "Generating PrayerBar Microsoft Store Visual Kit -> $outRoot"

# Logos & Promotional Art
Save-SquareStoreArt -OutPath (Join-Path $logosDir "StoreLogo_300x300.png") -Size 300 -IncludeWordmark $false
Save-SquareStoreArt -OutPath (Join-Path $logosDir "BoxArt_1080x1080.png") -Size 1080 -IncludeWordmark $true
Save-PosterArt720x1080 -OutPath (Join-Path $logosDir "PosterArt_720x1080.png")
Save-HeroArt1920x1080 -OutPath (Join-Path $logosDir "HeroArt_1920x1080.png")

# Desktop Screenshots (English)
Save-Screenshot1 -OutPath (Join-Path $enDir "01_taskbar_countdown.png") -Lang $Strings.en
Save-Screenshot2 -OutPath (Join-Path $enDir "02_fluent_flyout_qibla.png") -Lang $Strings.en
Save-Screenshot3 -OutPath (Join-Path $enDir "03_ramadan_imsakiye.png") -Lang $Strings.en
Save-Screenshot4 -OutPath (Join-Path $enDir "04_settings_features.png") -Lang $Strings.en

# Desktop Screenshots (Turkish)
Save-Screenshot1 -OutPath (Join-Path $trDir "01_gorev_cubugu_sayac.png") -Lang $Strings.tr
Save-Screenshot2 -OutPath (Join-Path $trDir "02_acilir_kart_kible.png") -Lang $Strings.tr
Save-Screenshot3 -OutPath (Join-Path $trDir "03_ramazan_imsakiyesi.png") -Lang $Strings.tr
Save-Screenshot4 -OutPath (Join-Path $trDir "04_ayarlar_ozellikler.png") -Lang $Strings.tr

Write-Host "Generated 12 PNG files successfully."
