param(
    [string]$QtDir = "C:\Qt\6.9.2\mingw_64",
    [string]$MingwDir = "C:\Qt\Tools\mingw1310_64",
    [string]$CMakeExe = "C:\Qt\Tools\CMake_64\bin\cmake.exe",
    [string]$NinjaExe = "C:\Qt\Tools\Ninja\ninja.exe",
    [string]$BuildDir = "build\release-windows",
    [string]$DistDir = "dist\xgps-qt-windows",
    [switch]$Zip
)

$ErrorActionPreference = "Stop"

function Resolve-ProjectPath([string]$Path) {
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }
    return Join-Path $ProjectRoot $Path
}

function Copy-RequiredFile([string]$Source, [string]$Destination) {
    if (-not (Test-Path -LiteralPath $Source)) {
        throw "Required file not found: $Source"
    }
    Copy-Item -Force -LiteralPath $Source -Destination $Destination
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Resolve-Path -LiteralPath (Join-Path $ScriptDir "..")
$BuildPath = Resolve-ProjectPath $BuildDir
$DistPath = Resolve-ProjectPath $DistDir
$QtBin = Join-Path $QtDir "bin"
$QtPlugins = Join-Path $QtDir "plugins"
$MingwBin = Join-Path $MingwDir "bin"
$CompilerExe = Join-Path $MingwBin "g++.exe"
$WindeployQtExe = Join-Path $QtBin "windeployqt.exe"
$TargetExe = Join-Path $BuildPath "xgps-qt.exe"

Write-Host "Project: $ProjectRoot"
Write-Host "Build:   $BuildPath"
Write-Host "Dist:    $DistPath"

$env:PATH = "$QtBin;$MingwBin;$(Split-Path -Parent $NinjaExe);$env:PATH"

if (-not (Test-Path -LiteralPath $CMakeExe)) {
    throw "cmake.exe not found: $CMakeExe"
}
if (-not (Test-Path -LiteralPath $NinjaExe)) {
    throw "ninja.exe not found: $NinjaExe"
}
if (-not (Test-Path -LiteralPath $CompilerExe)) {
    throw "g++.exe not found: $CompilerExe"
}

if (Test-Path -LiteralPath $BuildPath) {
    $resolvedBuild = Resolve-Path -LiteralPath $BuildPath
    if (-not $resolvedBuild.Path.StartsWith($ProjectRoot.Path, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clean a build directory outside the project: $resolvedBuild"
    }
    Remove-Item -LiteralPath $resolvedBuild.Path -Recurse -Force
}

$configureArgs = @(
    "-S", $ProjectRoot.Path,
    "-B", $BuildPath,
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release",
    "-DCMAKE_PREFIX_PATH=$QtDir",
    "-DCMAKE_MAKE_PROGRAM=$NinjaExe",
    "-DCMAKE_CXX_COMPILER=$CompilerExe"
)

& $CMakeExe @configureArgs

& $CMakeExe --build $BuildPath

if (-not (Test-Path -LiteralPath $TargetExe)) {
    throw "Build succeeded but executable was not found: $TargetExe"
}

if (Test-Path -LiteralPath $DistPath) {
    $resolvedDist = Resolve-Path -LiteralPath $DistPath
    if (-not $resolvedDist.Path.StartsWith($ProjectRoot.Path, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clean a dist directory outside the project: $resolvedDist"
    }
    Remove-Item -LiteralPath $resolvedDist.Path -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $DistPath | Out-Null
Copy-Item -Force -LiteralPath $TargetExe -Destination $DistPath

$deploySucceeded = $false
if (Test-Path -LiteralPath $WindeployQtExe) {
    try {
        & $WindeployQtExe --release --compiler-runtime (Join-Path $DistPath "xgps-qt.exe")
        if ($LASTEXITCODE -eq 0) {
            $deploySucceeded = $true
        }
    } catch {
        Write-Warning "windeployqt failed; falling back to manual DLL copy. $($_.Exception.Message)"
    }
}

if (-not $deploySucceeded) {
    Write-Host "Using manual Qt runtime copy."
    New-Item -ItemType Directory -Force -Path (Join-Path $DistPath "platforms") | Out-Null

    Copy-RequiredFile (Join-Path $QtBin "Qt6Core.dll") $DistPath
    Copy-RequiredFile (Join-Path $QtBin "Qt6Gui.dll") $DistPath
    Copy-RequiredFile (Join-Path $QtBin "Qt6Network.dll") $DistPath
    Copy-RequiredFile (Join-Path $QtBin "Qt6Widgets.dll") $DistPath
    Copy-RequiredFile (Join-Path $MingwBin "libgcc_s_seh-1.dll") $DistPath
    Copy-RequiredFile (Join-Path $MingwBin "libstdc++-6.dll") $DistPath
    Copy-RequiredFile (Join-Path $MingwBin "libwinpthread-1.dll") $DistPath
    Copy-RequiredFile (Join-Path $QtPlugins "platforms\qwindows.dll") (Join-Path $DistPath "platforms")
}

Copy-Item -Force -LiteralPath (Join-Path $ProjectRoot "readme.txt") -Destination $DistPath

if ($Zip) {
    $ZipPath = Join-Path (Split-Path -Parent $DistPath) "xgps-qt-windows.zip"
    if (Test-Path -LiteralPath $ZipPath) {
        Remove-Item -LiteralPath $ZipPath -Force
    }
    Compress-Archive -Path (Join-Path $DistPath "*") -DestinationPath $ZipPath
    Write-Host "Zip: $ZipPath"
}

Write-Host "Release package created: $DistPath"
