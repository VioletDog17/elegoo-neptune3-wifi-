param(
  [ValidateSet('MKS_E3_V2', 'NEPTUNE3_MKS_WIFI_DIAG', 'NEPTUNE3_MKS_WIFI')]
  [string]$Environment = 'MKS_E3_V2'
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$project = Join-Path $repo 'neptune3pro-main/Firmware/Mainboard/Marlin-2.1.1_TJC_029/Marlin-2.1.1_TJC_029'
python (Join-Path $PSScriptRoot 'prepare-build.py')
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$env:PLATFORMIO_CORE_DIR = Join-Path $repo '.platformio-wifi'
$localPio = Join-Path $repo '.tools/pio'
if (Test-Path (Join-Path $localPio 'platformio')) {
  $env:PYTHONPATH = $localPio
}
Push-Location $project
try {
  python -m platformio run -e $Environment
  $buildExit = $LASTEXITCODE
} finally {
  Pop-Location
}
if ($buildExit -eq 0) {
  $sourceName = switch ($Environment) { 'MKS_E3_V2' { 'ZNP_ROBIN_NANO.bin' } 'NEPTUNE3_MKS_WIFI' { 'ZNP_WIFI.bin' } default { 'ZNP_WIFI_DIAG.bin' } }
  $binary = Join-Path $project ".pio/build/$Environment/$sourceName"
  if (!(Test-Path $binary)) { throw "Build output missing: $binary" }
  $destination = Join-Path $repo "dist/$Environment"
  New-Item -ItemType Directory -Force -Path $destination | Out-Null
  $export = Join-Path $destination 'ZNP_ROBIN_NANO.bin'
  Copy-Item -LiteralPath $binary -Destination $export -Force
  $manifest = [ordered]@{
    environment = $Environment
    file = 'ZNP_ROBIN_NANO.bin'
    sha256 = (Get-FileHash -LiteralPath $export -Algorithm SHA256).Hash
    bytes = (Get-Item -LiteralPath $export).Length
    built_at = (Get-Date).ToString('o')
    hardware_tested = $false
    diagnostic_only = ($Environment -eq 'NEPTUNE3_MKS_WIFI_DIAG')
    upload_and_print_enabled = ($Environment -eq 'NEPTUNE3_MKS_WIFI')
    note = 'Source build, not a backup of the installed printer firmware. See FLASHING-RU.md.'
  }
  $manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $destination 'manifest.json') -Encoding utf8
  Write-Host "Exported SD update file: $export"
}
exit $buildExit
