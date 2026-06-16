$ErrorActionPreference = "Stop"

$menuRoot = $PSScriptRoot
$projectRoot = (Resolve-Path (Join-Path $menuRoot "..\..")).Path
$raylibRoot = "C:\raylib\w64devkit"
$compiler = Join-Path $raylibRoot "bin\g++.exe"
$raylibHeader = Join-Path $raylibRoot "include\raylib.h"
$raylibLibrary = Join-Path $raylibRoot "lib\libraylib.a"
$generated = Join-Path $menuRoot "generated"
$bin = Join-Path $menuRoot "bin"

if (-not (Test-Path $compiler) -or -not (Test-Path $raylibHeader) -or -not (Test-Path $raylibLibrary)) {
    throw "No se encontro Raylib en C:\raylib\w64devkit."
}

New-Item -ItemType Directory -Force -Path $generated, $bin | Out-Null

$converter = Join-Path $menuRoot "convert_dae_preview.py"
$world1Source = Join-Path $projectRoot "assets\mapa1\world1\CourseSelectW1.dae"
$world2Source = Join-Path $projectRoot "assets\Mundos\FreezeezyPeak\Freezeezy Peak.dae"
$world3Source = Join-Path $menuRoot "worlds\Mundo3\model.dae"
$world4Source = Join-Path $menuRoot "worlds\Mundo4\model.dae"
$world1Preview = Join-Path $generated "world1_preview.preview"
$world2Preview = Join-Path $generated "world2_preview.preview"
$world3Preview = Join-Path $generated "world3_preview.preview"
$world4Preview = Join-Path $generated "world4_preview.preview"

python $converter $world1Source $world1Preview
python $converter $world2Source $world2Preview
if (Test-Path $world3Source) {
    python $converter $world3Source $world3Preview
} else {
    Write-Host "Mundo 3 usa la previsualizacion conceptual. Agrega worlds\Mundo3\model.dae para reemplazarla."
}
if (Test-Path $world4Source) {
    python $converter $world4Source $world4Preview
} else {
    Write-Host "Mundo 4 usa la previsualizacion conceptual. Agrega worlds\Mundo4\model.dae para reemplazarla."
}

$source = Join-Path $menuRoot "menu_raylib.cpp"
$output = Join-Path $bin "PaperPinixRaylibMenu.exe"
$previousPath = $env:Path
$env:Path = "$(Join-Path $raylibRoot 'bin');$env:Path"

try {
    & $compiler `
        $source `
        -o $output `
        -std=c++20 `
        -O2 `
        -Wall `
        -Wextra `
        -mwindows `
        "-I$(Join-Path $raylibRoot 'include')" `
        "-L$(Join-Path $raylibRoot 'lib')" `
        -lraylib `
        -lopengl32 `
        -lgdi32 `
        -lwinmm `
        -lshell32
} finally {
    $env:Path = $previousPath
}

if ($LASTEXITCODE -ne 0) {
    throw "No se pudo compilar el menu Raylib."
}

Write-Host "Menu Raylib compilado en: $output"
