param(
    [string]$OutputDir = (Join-Path $PSScriptRoot "..\packaging\msix\Assets")
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$assetsPath = [System.IO.Path]::GetFullPath($OutputDir)
New-Item -ItemType Directory -Force -Path $assetsPath | Out-Null

function New-RoundedRectPath {
    param([float]$X, [float]$Y, [float]$W, [float]$H, [float]$R)
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = [Math]::Min($R * 2, [Math]::Min($W, $H))
    if ($d -le 0.5) {
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

function Draw-VakitIconGlyph {
    param(
        [System.Drawing.Graphics]$G,
        [float]$X,
        [float]$Y,
        [float]$Size,
        [bool]$WithBackground = $true
    )

    $G.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $G.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $G.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

    if ($WithBackground) {
        $pad = $Size * 0.05
        $bgSize = $Size - ($pad * 2)
        $radius = $bgSize * 0.24
        $bgPath = New-RoundedRectPath -X ($X + $pad) -Y ($Y + $pad) -W $bgSize -H $bgSize -R $radius

        $rect = [System.Drawing.RectangleF]::new($X + $pad, $Y + $pad, $bgSize, $bgSize)
        $grad = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
            $rect,
            [System.Drawing.Color]::FromArgb(255, 11, 59, 56),
            [System.Drawing.Color]::FromArgb(255, 20, 122, 110),
            [System.Drawing.Drawing2D.LinearGradientMode]::ForwardDiagonal
        )
        $G.FillPath($grad, $bgPath)

        # Subtle luminous rim
        $rimPen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(55, 255, 255, 255), [Math]::Max(1.0, $Size * 0.02))
        $G.DrawPath($rimPen, $bgPath)
        $rimPen.Dispose()
        $grad.Dispose()
        $bgPath.Dispose()
    }

    # Subtle radial glow behind crescent
    $cx = $X + $Size * 0.50
    $cy = $Y + $Size * 0.50
    $glowR = $Size * 0.34
    $glowPath = New-Object System.Drawing.Drawing2D.GraphicsPath
    $glowPath.AddEllipse($cx - $glowR, $cy - $glowR, $glowR * 2, $glowR * 2)
    $pgb = New-Object System.Drawing.Drawing2D.PathGradientBrush($glowPath)
    $pgb.CenterColor = [System.Drawing.Color]::FromArgb(60, 255, 215, 110)
    $pgb.SurroundColors = @([System.Drawing.Color]::FromArgb(0, 255, 215, 110))
    $G.FillEllipse($pgb, $cx - $glowR, $cy - $glowR, $glowR * 2, $glowR * 2)
    $pgb.Dispose()
    $glowPath.Dispose()

    # Crescent moon via Region difference
    $outerR = $Size * 0.28
    $outerX = $cx - $outerR - ($Size * 0.02)
    $outerY = $cy - $outerR
    $innerR = $Size * 0.23
    $innerX = $outerX + ($Size * 0.13)
    $innerY = $outerY - ($Size * 0.05)

    $outerPath = New-Object System.Drawing.Drawing2D.GraphicsPath
    $outerPath.AddEllipse($outerX, $outerY, $outerR * 2, $outerR * 2)
    $innerPath = New-Object System.Drawing.Drawing2D.GraphicsPath
    $innerPath.AddEllipse($innerX, $innerY, $innerR * 2, $innerR * 2)

    $region = New-Object System.Drawing.Region($outerPath)
    $region.Exclude($innerPath)

    $moonRect = [System.Drawing.RectangleF]::new($outerX, $outerY, $outerR * 2, $outerR * 2)
    $moonBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
        $moonRect,
        [System.Drawing.Color]::FromArgb(255, 255, 232, 148),
        [System.Drawing.Color]::FromArgb(255, 244, 174, 52),
        [System.Drawing.Drawing2D.LinearGradientMode]::Vertical
    )
    $G.FillRegion($moonBrush, $region)
    $moonBrush.Dispose()
    $region.Dispose()
    $outerPath.Dispose()
    $innerPath.Dispose()

    # Crisp 4-point star next to crescent
    $sx = $cx + $Size * 0.14
    $sy = $cy - $Size * 0.10
    $sr = [Math]::Max(1.5, $Size * 0.075)
    $ir = $sr * 0.34
    $starPts = [System.Drawing.PointF[]]@(
        [System.Drawing.PointF]::new($sx, $sy - $sr),
        [System.Drawing.PointF]::new($sx + $ir, $sy - $ir),
        [System.Drawing.PointF]::new($sx + $sr, $sy),
        [System.Drawing.PointF]::new($sx + $ir, $sy + $ir),
        [System.Drawing.PointF]::new($sx, $sy + $sr),
        [System.Drawing.PointF]::new($sx - $ir, $sy + $ir),
        [System.Drawing.PointF]::new($sx - $sr, $sy),
        [System.Drawing.PointF]::new($sx - $ir, $sy - $ir)
    )
    $starBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 255, 245, 200))
    $G.FillPolygon($starBrush, $starPts)
    $starBrush.Dispose()

    # Minimal progress pill bar at bottom of icon
    if ($Size -ge 24) {
        $barW = $Size * 0.44
        $barH = [Math]::Max(2.0, $Size * 0.055)
        $barX = $cx - ($barW * 0.5)
        $barY = $cy + ($Size * 0.30)
        $trackPath = New-RoundedRectPath -X $barX -Y $barY -W $barW -H $barH -R ($barH / 2)
        $trackBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(70, 255, 255, 255))
        $G.FillPath($trackBrush, $trackPath)
        $trackBrush.Dispose()
        $trackPath.Dispose()

        $fillPath = New-RoundedRectPath -X $barX -Y $barY -W ($barW * 0.68) -H $barH -R ($barH / 2)
        $fillBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 255, 212, 102))
        $G.FillPath($fillBrush, $fillPath)
        $fillBrush.Dispose()
        $fillPath.Dispose()
    }
}

function Save-SquareIcon {
    param([string]$FileName, [int]$Size, [bool]$WithBackground = $true)
    $bmp = New-Object System.Drawing.Bitmap($Size, $Size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.Clear([System.Drawing.Color]::Transparent)
    Draw-VakitIconGlyph -G $g -X 0 -Y 0 -Size $Size -WithBackground $WithBackground
    $g.Dispose()
    $out = Join-Path $assetsPath $FileName
    $bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

function Save-WideBanner {
    param([string]$FileName, [int]$Width, [int]$Height, [bool]$IncludeText)
    $bmp = New-Object System.Drawing.Bitmap($Width, $Height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit

    $rect = [System.Drawing.RectangleF]::new(0, 0, $Width, $Height)
    $bgGrad = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
        $rect,
        [System.Drawing.Color]::FromArgb(255, 10, 48, 46),
        [System.Drawing.Color]::FromArgb(255, 16, 94, 86),
        [System.Drawing.Drawing2D.LinearGradientMode]::Horizontal
    )
    $g.FillRectangle($bgGrad, $rect)
    $bgGrad.Dispose()

    if ($IncludeText) {
        $iconSize = $Height * 0.56
        $iconX = $Width * 0.14
        $iconY = ($Height - $iconSize) * 0.5
        Draw-VakitIconGlyph -G $g -X $iconX -Y $iconY -Size $iconSize -WithBackground $true

        $titleFont = New-Object System.Drawing.Font("Segoe UI", [float]($Height * 0.19), [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
        $subFont = New-Object System.Drawing.Font("Segoe UI", [float]($Height * 0.105), [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
        $whiteBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::White)
        $subBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(215, 212, 245, 238))

        $tx = $iconX + $iconSize + ($Width * 0.045)
        $g.DrawString("VakitÇubuğu", $titleFont, $whiteBrush, $tx, $Height * 0.30)
        $g.DrawString("Görev Çubuğu Namaz Vakitleri", $subFont, $subBrush, $tx, $Height * 0.55)

        $titleFont.Dispose(); $subFont.Dispose(); $whiteBrush.Dispose(); $subBrush.Dispose()
    }
    else {
        $iconSize = $Height * 0.46
        Draw-VakitIconGlyph -G $g -X (($Width - $iconSize) * 0.5) -Y (($Height - $iconSize) * 0.5) -Size $iconSize -WithBackground $true
    }

    $g.Dispose()
    $bmp.Save((Join-Path $assetsPath $FileName), [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

function Save-MultiResIco {
    param([string]$FileName)
    $sizes = @(16, 24, 32, 48, 256)
    $pngBlobs = New-Object System.Collections.Generic.List[byte[]]

    foreach ($s in $sizes) {
        $bmp = New-Object System.Drawing.Bitmap($s, $s, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        $g.Clear([System.Drawing.Color]::Transparent)
        Draw-VakitIconGlyph -G $g -X 0 -Y 0 -Size $s -WithBackground $true
        $g.Dispose()

        $ms = New-Object System.IO.MemoryStream
        $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
        $pngBlobs.Add($ms.ToArray())
        $ms.Dispose()
        $bmp.Dispose()
    }

    $outPath = Join-Path $assetsPath $FileName
    $fs = [System.IO.File]::Create($outPath)
    $bw = New-Object System.IO.BinaryWriter($fs)

    # ICONDIR header
    $bw.Write([uint16]0)
    $bw.Write([uint16]1)
    $bw.Write([uint16]$sizes.Count)

    $offset = 6 + (16 * $sizes.Count)
    for ($i = 0; $i -lt $sizes.Count; $i++) {
        $s = $sizes[$i]
        $blob = $pngBlobs[$i]
        $dimByte = if ($s -ge 256) { [byte]0 } else { [byte]$s }
        $bw.Write($dimByte)            # Width
        $bw.Write($dimByte)            # Height
        $bw.Write([byte]0)             # ColorCount
        $bw.Write([byte]0)             # Reserved
        $bw.Write([uint16]1)           # Planes
        $bw.Write([uint16]32)          # BitCount
        $bw.Write([uint32]$blob.Length)
        $bw.Write([uint32]$offset)
        $offset += $blob.Length
    }

    foreach ($blob in $pngBlobs) {
        $bw.Write($blob)
    }

    $bw.Flush()
    $bw.Dispose()
    $fs.Dispose()
}

# Store Logos
Save-SquareIcon "StoreLogo.png" 50
Save-SquareIcon "StoreLogo.scale-100.png" 50
Save-SquareIcon "StoreLogo.scale-200.png" 100

# Square44x44 Logos + targetsize + unplated variants
Save-SquareIcon "Square44x44Logo.png" 44
Save-SquareIcon "Square44x44Logo.scale-100.png" 44
Save-SquareIcon "Square44x44Logo.scale-200.png" 88
foreach ($ts in @(16, 24, 32, 48, 256)) {
    Save-SquareIcon "Square44x44Logo.targetsize-$ts.png" $ts
    Save-SquareIcon "Square44x44Logo.targetsize-${ts}_altform-unplated.png" $ts
    Save-SquareIcon "Square44x44Logo.targetsize-${ts}_altform-lightunplated.png" $ts
}

# Tiles & Splash
Save-SquareIcon "Square71x71Logo.png" 71
Save-SquareIcon "Square71x71Logo.scale-100.png" 71
Save-SquareIcon "Square71x71Logo.scale-200.png" 142
Save-SquareIcon "Square150x150Logo.png" 150
Save-SquareIcon "Square150x150Logo.scale-100.png" 150
Save-SquareIcon "Square150x150Logo.scale-200.png" 300
Save-WideBanner "Wide310x150Logo.png" 310 150 $true
Save-WideBanner "Wide310x150Logo.scale-100.png" 310 150 $true
Save-WideBanner "Wide310x150Logo.scale-200.png" 620 300 $true
Save-WideBanner "SplashScreen.png" 620 300 $false
Save-WideBanner "SplashScreen.scale-100.png" 620 300 $false
Save-WideBanner "SplashScreen.scale-200.png" 1240 600 $false

# Desktop ICO
Save-MultiResIco "app.ico"

Write-Output "Generated all MSIX visual assets and app.ico in $assetsPath"
