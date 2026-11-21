param(
    [ValidateSet("Lifetime", "Annual")]
    [string]$Tier = "Lifetime",

    [string]$ExpiryDate = "",

    [int]$Count = 1
)

$ErrorActionPreference = "Stop"
$salt = "VakitCubugu::Pro::Global::2026::v2"

if ($Tier -eq "Annual") {
    if ([string]::IsNullOrWhiteSpace($ExpiryDate)) {
        $ExpiryDate = (Get-Date).AddYears(1).ToString("yyyyMMdd")
    }
    if ($ExpiryDate -notmatch '^\d{8}$') {
        throw "ExpiryDate must be in YYYYMMDD format (e.g. 20271121)."
    }
}

$sha = [System.Security.Cryptography.SHA256]::Create()
$rng = [System.Security.Cryptography.RandomNumberGenerator]::Create()
try {
    for ($i = 0; $i -lt $Count; $i++) {
        if ($Tier -eq "Lifetime") {
            $tierCode = "LIFE"
            $bytes = New-Object byte[] 4
            $rng.GetBytes($bytes)
            $idPart = ($bytes | ForEach-Object { $_.ToString("X2") }) -join ""
        } else {
            $tierCode = "YEAR"
            $idPart = $ExpiryDate
        }

        $payload = "$salt|$tierCode|$idPart"
        $hashBytes = $sha.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($payload))
        $sig = (($hashBytes[0..3]) | ForEach-Object { $_.ToString("X2") }) -join ""
        Write-Output "VKPR-$tierCode-$idPart-$sig"
    }
} finally {
    $sha.Dispose()
    $rng.Dispose()
}
