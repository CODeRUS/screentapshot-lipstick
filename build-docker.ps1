#Requires -Version 5.0
<#
.SYNOPSIS
  Sailfish OS RPM build via Docker (same idea as coderus/github-sfos-build).

.PARAMETER Release
  SDK image tag, e.g. 4.6.0.13 (see hub.docker.com/r/coderus/sailfishos-platform-sdk/tags)

.PARAMETER Arch
  mb2 target arch: i486 or armv7hl

.PARAMETER ImageSuffix
  Optional suffix before colon in image name (rare).
#>
param(
    [string]$Release = "4.6.0.13",
    [string]$Arch = "i486",
    [string]$ImageSuffix = ""
)

$ErrorActionPreference = "Stop"
$RepoRoot = $PSScriptRoot
$Image = "coderus/sailfishos-platform-sdk${ImageSuffix}:${Release}"

if (-not (Get-Command docker -ErrorAction SilentlyContinue)) {
    Write-Error "docker not found in PATH. Install Docker Desktop (Linux containers / WSL2)."
}

Write-Host "Image: $Image"
Write-Host "Target: SailfishOS-$Release-$Arch"
Write-Host "Workspace: $RepoRoot"

New-Item -ItemType Directory -Force -Path (Join-Path $RepoRoot "RPMS") | Out-Null

# Bash inside container: same flow as github-sfos-build docker.sh
$BashCmd = 'set -e; mkdir -p build && cd build && cp -r /workspace/* . && mb2 -t SailfishOS-${INPUT_RELEASE}-${INPUT_ARCH} build && mkdir -p /workspace/RPMS && shopt -s nullglob && cp -v RPMS/*.rpm /workspace/RPMS/'

docker run --rm --privileged `
    -e "INPUT_RELEASE=$Release" `
    -e "INPUT_ARCH=$Arch" `
    -v "${RepoRoot}:/workspace" `
    $Image `
    /bin/bash -lc "$BashCmd"

if ($LASTEXITCODE -ne 0) {
    Write-Error "docker run failed with exit code $LASTEXITCODE (is Docker Desktop running with Linux containers?)"
}

Write-Host "Done. RPMS:"
Get-ChildItem -Path (Join-Path $RepoRoot "RPMS") -Filter "*.rpm" -ErrorAction SilentlyContinue | ForEach-Object { $_.FullName }
