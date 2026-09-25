# Windows build of core + tools + tests (MSVC via vcvars64 + Ninja). PowerShell only.
#   ./build.ps1                 configure (once) + build + ctest
#   ./build.ps1 -Configure      force a reconfigure
#   ./build.ps1 -NoTest         skip ctest
#   ./build.ps1 -Update         rewrite the golden expectations (review the diff!)
param(
  [switch]$Configure,
  [switch]$NoTest,
  [switch]$Update,
  [string]$Config = "Release"
)
$ErrorActionPreference = "Continue"
$vcvars = "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) { throw "vcvars64.bat not found at $vcvars" }
$src = $PSScriptRoot
$build = Join-Path $src "build"

if ($Configure -or -not (Test-Path (Join-Path $build "build.ninja"))) {
  Write-Host "==> configure"
  cmd /c "`"$vcvars`" >nul 2>&1 && cmake -S `"$src`" -B `"$build`" -G Ninja -DCMAKE_BUILD_TYPE=$Config"
  if ($LASTEXITCODE -ne 0) { throw "configure failed" }
}
Write-Host "==> build"
cmd /c "`"$vcvars`" >nul 2>&1 && cmake --build `"$build`""
if ($LASTEXITCODE -ne 0) { throw "build failed" }

if ($Update) {
  & (Join-Path $build "anycanvas-golden.exe") (Join-Path $src "tests\golden") --update
  if ($LASTEXITCODE -ne 0) { throw "golden update failed" }
}
if (-not $NoTest) {
  Write-Host "==> ctest"
  cmd /c "`"$vcvars`" >nul 2>&1 && ctest --test-dir `"$build`" --output-on-failure"
  if ($LASTEXITCODE -ne 0) { throw "tests failed" }
}
