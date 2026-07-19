<#
Builds VirtualBoyGoPC (Release) and zips it into build-pc/dist/.
Run from anywhere; paths are resolved relative to this script.
#>

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot "build-pc"
$releaseDir = Join-Path $buildDir "Release"
$distDir = Join-Path $buildDir "dist"

cmake -S $repoRoot -B $buildDir
if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }

cmake --build $buildDir --target VirtualBoyGoPC --config Release
if ($LASTEXITCODE -ne 0) { throw "build failed" }

$versionHeader = Join-Path $buildDir "generated\Version.h"
$versionString = (Select-String -Path $versionHeader -Pattern 'kGeneratedVersionString = "([^"]+)"').Matches[0].Groups[1].Value

New-Item -ItemType Directory -Force -Path $distDir | Out-Null
$zipPath = Join-Path $distDir "VirtualBoyGoPC-$versionString.zip"
if (Test-Path $zipPath) { Remove-Item $zipPath -Force }

Compress-Archive -Path (Join-Path $releaseDir "VirtualBoyGoPC.exe"), (Join-Path $releaseDir "roms") -DestinationPath $zipPath

Write-Host "Created $zipPath"
