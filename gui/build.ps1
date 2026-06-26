# Build simpl-slidrr-config.exe (Windows one-file bundle)
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

pip install -r requirements.txt -r requirements-build.txt
pyinstaller --noconfirm simpl-slidrr-config.spec

Write-Host ""
Write-Host "Built: $PSScriptRoot\dist\simpl-slidrr-config.exe"
