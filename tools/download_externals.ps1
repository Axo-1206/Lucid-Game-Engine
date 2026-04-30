# Lucid Engine - Externals Downloader
# This script fetches the stable, header-only dependencies.

$ScriptDir = $PSScriptRoot
if (!$ScriptDir) { $ScriptDir = (Get-Location).Path }

$ExternalsDir = Join-Path (Join-Path $ScriptDir "..") "externals"
if (!(Test-Path $ExternalsDir)) { New-Item -ItemType Directory -Path $ExternalsDir }

function Download-Header {
    param([string]$Url, [string]$Folder, [string]$Filename)
    $TargetDir = Join-Path $ExternalsDir $Folder
    if (!(Test-Path $TargetDir)) { New-Item -ItemType Directory -Path $TargetDir }
    $TargetPath = Join-Path $TargetDir $Filename
    Write-Host "Downloading $Filename to $Folder..." -ForegroundColor Cyan
    Invoke-WebRequest -Uri $Url -OutFile $TargetPath
}

# 1. GLM (Math) - v0.9.9.8 (Getting the zip is easier for the full header set)
$GLM_Url = "https://github.com/g-truc/glm/archive/refs/tags/0.9.9.8.zip"
$GLM_Zip = Join-Path $ExternalsDir "glm.zip"
Write-Host "Downloading GLM..." -ForegroundColor Cyan
Invoke-WebRequest -Uri $GLM_Url -OutFile $GLM_Zip
Expand-Archive -Path $GLM_Zip -DestinationPath $ExternalsDir -Force
Remove-Item $GLM_Zip
Move-Item -Path (Join-Path $ExternalsDir "glm-0.9.9.8") -Destination (Join-Path $ExternalsDir "glm") -Force

# 2. VMA (Vulkan Memory Allocator)
Download-Header "https://raw.githubusercontent.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/master/include/vk_mem_alloc.h" "vma" "vk_mem_alloc.h"

# 3. miniaudio (Audio)
Download-Header "https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h" "miniaudio" "miniaudio.h"

# 4. cgltf (GLTF Loader)
Download-Header "https://raw.githubusercontent.com/jkuhlmann/cgltf/master/cgltf.h" "cgltf" "cgltf.h"

# 5. nlohmann/json
Download-Header "https://github.com/nlohmann/json/releases/latest/download/json.hpp" "nlohmann" "json.hpp"

Write-Host "All header-only libraries downloaded successfully!" -ForegroundColor Green
