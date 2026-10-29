param(
  [string]$BuildDir = "build/test"
)

$ErrorActionPreference = "Stop"
if (Get-Variable -Name PSNativeCommandUseErrorActionPreference -ErrorAction SilentlyContinue) {
  $PSNativeCommandUseErrorActionPreference = $false
}

$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not [System.IO.Path]::IsPathRooted($BuildDir)) {
  $BuildDir = Join-Path $repoRoot $BuildDir
}
$BuildDir = (Resolve-Path -LiteralPath $BuildDir).Path
$cacheFile = Join-Path $BuildDir "CMakeCache.txt"
if (-not (Test-Path -LiteralPath $cacheFile)) {
  throw "CMake cache not found: $cacheFile"
}

$cache = Get-Content -LiteralPath $cacheFile
$compilerLine = $cache | Where-Object { $_ -match '^CMAKE_CXX_COMPILER:[^=]*=' } | Select-Object -First 1
$cmakeLine = $cache | Where-Object { $_ -match '^CMAKE_COMMAND:INTERNAL=' } | Select-Object -First 1
$runtimeDir = $null
$ctestPath = $null

if ($compilerLine -match '^CMAKE_CXX_COMPILER:[^=]*=(.+)$') {
  $compilerPath = $Matches[1]
  $candidateRuntimeDir = Split-Path -Parent $compilerPath
  $runtimeDlls = @(
    "libc++.dll",
    "libunwind.dll",
    "libstdc++-6.dll",
    "libgcc_s_seh-1.dll",
    "libgcc_s_dw2-1.dll",
    "libwinpthread-1.dll"
  )
  $runtimeDll = $runtimeDlls | Where-Object { Test-Path -LiteralPath (Join-Path $candidateRuntimeDir $_) } |
    Select-Object -First 1
  if ($runtimeDll) {
    $runtimeDir = $candidateRuntimeDir
  }
}

if ($cmakeLine -match '^CMAKE_COMMAND:INTERNAL=(.+)$') {
  $candidateCTest = Join-Path (Split-Path -Parent $Matches[1]) "ctest.exe"
  if (Test-Path -LiteralPath $candidateCTest) {
    $ctestPath = $candidateCTest
  }
}
if (-not $ctestPath) {
  $ctestCommand = Get-Command ctest.exe -ErrorAction SilentlyContinue
  if ($ctestCommand) {
    $ctestPath = $ctestCommand.Source
  }
}
if (-not $ctestPath) {
  throw "ctest.exe was not found beside the configured CMake executable or on PATH."
}

$previousPath = $env:PATH
Push-Location -LiteralPath $BuildDir
try {
  if ($runtimeDir) {
    $env:PATH = "$runtimeDir;$previousPath"
    Write-Host "Using host compiler runtime directory: $runtimeDir"
  }
  & $ctestPath --output-on-failure -j
  $testExitCode = $LASTEXITCODE
} finally {
  Pop-Location
  $env:PATH = $previousPath
}

exit $testExitCode
