$ErrorActionPreference = "Stop"

$menuRoot = $PSScriptRoot
$projectRoot = (Resolve-Path (Join-Path $menuRoot "..\..")).Path
$vendorRoot = Join-Path $menuRoot "vendor"
$raylibVendorRoot = Join-Path $vendorRoot "raylib"
$raylibVersion = "6.0"
$generated = Join-Path $menuRoot "generated"
$bin = Join-Path $menuRoot "bin"

function Find-RaylibMsvcRoot {
    param([string[]]$Roots)

    foreach ($root in $Roots) {
        if (-not $root -or -not (Test-Path $root)) {
            continue
        }

        $header = Get-ChildItem -Path $root -Recurse -Filter "raylib.h" -File -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match "\\include\\raylib\.h$" } |
            Select-Object -First 1
        $library = Get-ChildItem -Path $root -Recurse -Filter "raylib.lib" -File -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match "\\lib\\raylib\.lib$" } |
            Select-Object -First 1

        if ($header -and $library) {
            return [PSCustomObject]@{
                Root = (Split-Path -Parent (Split-Path -Parent $header.FullName))
                Include = (Split-Path -Parent $header.FullName)
                Lib = (Split-Path -Parent $library.FullName)
            }
        }
    }

    return $null
}

function Find-RaylibMingwRoot {
    param([string[]]$Roots)

    foreach ($root in $Roots) {
        if (-not $root -or -not (Test-Path $root)) {
            continue
        }

        $compiler = Get-ChildItem -Path $root -Recurse -Filter "g++.exe" -File -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match "\\bin\\g\+\+\.exe$" } |
            Select-Object -First 1
        $header = Get-ChildItem -Path $root -Recurse -Filter "raylib.h" -File -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match "\\include\\raylib\.h$" } |
            Select-Object -First 1
        $library = Get-ChildItem -Path $root -Recurse -Filter "libraylib.a" -File -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match "\\lib\\libraylib\.a$" } |
            Select-Object -First 1

        if ($compiler -and $header -and $library) {
            return [PSCustomObject]@{
                Root = (Split-Path -Parent (Split-Path -Parent $compiler.FullName))
                Compiler = $compiler.FullName
                Include = (Split-Path -Parent $header.FullName)
                Lib = (Split-Path -Parent $library.FullName)
            }
        }
    }

    return $null
}

function Install-RaylibMsvcPackage {
    New-Item -ItemType Directory -Force -Path $raylibVendorRoot | Out-Null

    $packageName = "raylib-$raylibVersion`_win64_msvc16.zip"
    $packagePath = Join-Path $vendorRoot $packageName
    $downloadUrl = "https://github.com/raysan5/raylib/releases/download/$raylibVersion/$packageName"

    if (-not (Test-Path $packagePath)) {
        Write-Host "Descargando Raylib $raylibVersion para MSVC..."
        Invoke-WebRequest -Uri $downloadUrl -OutFile $packagePath
    }

    if (-not (Find-RaylibMsvcRoot @($raylibVendorRoot))) {
        Write-Host "Extrayendo Raylib en: $raylibVendorRoot"
        Expand-Archive -Path $packagePath -DestinationPath $raylibVendorRoot -Force
    }
}

function Find-VcVars64 {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $installPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($installPath) {
            $candidate = Join-Path $installPath "VC\Auxiliary\Build\vcvars64.bat"
            if (Test-Path $candidate) {
                return $candidate
            }
        }
    }

    $commonRoots = @(
        Join-Path $env:ProgramFiles "Microsoft Visual Studio",
        Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio"
    )

    foreach ($root in $commonRoots) {
        if (-not (Test-Path $root)) {
            continue
        }

        $candidate = Get-ChildItem -Path $root -Recurse -Filter "vcvars64.bat" -File -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($candidate) {
            return $candidate.FullName
        }
    }

    return $null
}

function Find-PythonCommand {
    $checks = @(
        @{ Command = "py"; Args = @("-3") },
        @{ Command = "python"; Args = @() }
    )

    foreach ($check in $checks) {
        $command = Get-Command $check.Command -ErrorAction SilentlyContinue
        if (-not $command) {
            continue
        }
        if ($command.Source -like "*\Microsoft\WindowsApps\python.exe") {
            continue
        }

        & $command.Source @($check.Args) --version > $null 2> $null
        if ($LASTEXITCODE -eq 0) {
            return [PSCustomObject]@{
                Source = $command.Source
                Args = $check.Args
            }
        }
    }

    return $null
}

function Invoke-PreviewConverter {
    param(
        [object]$Python,
        [string]$Converter,
        [string]$InputPath,
        [string]$OutputPath
    )

    & $Python.Source @($Python.Args) $Converter $InputPath $OutputPath
    if ($LASTEXITCODE -ne 0) {
        throw "El convertidor fallo para $InputPath"
    }
}

New-Item -ItemType Directory -Force -Path $generated, $bin | Out-Null

$converter = Join-Path $menuRoot "convert_dae_preview.py"
$world1Source = Join-Path $projectRoot "assets\mapa1\world1\CourseSelectW1.dae"
$world2Source = Join-Path $projectRoot "assets\Mundos\FreezeezyPeak\Freezeezy Peak.dae"
$world3Source = Join-Path $menuRoot "worlds\Mundo3\model.dae"
$world4Source = Join-Path $projectRoot "assets\mapa 4\mapamian\World 1\World 1\CourseSelectW1.dae"
$world1Preview = Join-Path $generated "world1_preview.preview"
$world2Preview = Join-Path $generated "world2_preview.preview"
$world3Preview = Join-Path $generated "world3_preview.preview"
$world4Preview = Join-Path $generated "world4_preview.preview"

$python = Find-PythonCommand
if ($python) {
    try {
        Invoke-PreviewConverter $python $converter $world1Source $world1Preview
        Invoke-PreviewConverter $python $converter $world2Source $world2Preview
        if (Test-Path $world3Source) {
            Invoke-PreviewConverter $python $converter $world3Source $world3Preview
        } else {
            Write-Host "Mundo 3 usa la previsualizacion conceptual. Agrega worlds\Mundo3\model.dae para reemplazarla."
        }
        if (Test-Path $world4Source) {
            Invoke-PreviewConverter $python $converter $world4Source $world4Preview
        } else {
            Write-Host "Mundo 4 usa la previsualizacion conceptual. No se encontro el DAE real del mapa 4."
        }
    } catch {
        Write-Warning "No se pudieron generar previews desde DAE. El menu se compilara sin bloquear el build. Detalle: $($_.Exception.Message)"
    }
} else {
    Write-Warning "Python no esta instalado. Se omite la generacion de previews, pero el menu se compilara."
}

$source = Join-Path $menuRoot "menu_raylib.cpp"
$output = Join-Path $bin "PaperPinixRaylibMenu.exe"

$searchRoots = @(
    $raylibVendorRoot,
    (Join-Path $menuRoot "raylib"),
    (Join-Path $projectRoot "External\raylib"),
    "C:\raylib\w64devkit",
    "C:\raylib"
)

$mingw = Find-RaylibMingwRoot $searchRoots
if ($mingw) {
    $previousPath = $env:Path
    $env:Path = "$(Split-Path -Parent $mingw.Compiler);$env:Path"

    try {
        & $mingw.Compiler `
            $source `
            -o $output `
            -std=c++20 `
            -O2 `
            -Wall `
            -Wextra `
            -mwindows `
            "-I$($mingw.Include)" `
            "-L$($mingw.Lib)" `
            -lraylib `
            -lopengl32 `
            -lgdi32 `
            -lwinmm `
            -lshell32
    } finally {
        $env:Path = $previousPath
    }

    if ($LASTEXITCODE -ne 0) {
        throw "No se pudo compilar el menu Raylib con MinGW."
    }

    Write-Host "Menu Raylib compilado en: $output"
    exit 0
}

$msvc = Find-RaylibMsvcRoot $searchRoots
if (-not $msvc) {
    Install-RaylibMsvcPackage
    $msvc = Find-RaylibMsvcRoot @($raylibVendorRoot)
}

if (-not $msvc) {
    throw "No se encontro Raylib. Agrega Raylib a Menu\RaylibMenu\vendor\raylib o permite que el script lo descargue."
}

$vcvars = Find-VcVars64
if (-not $vcvars) {
    throw "No se encontro vcvars64.bat. Instala Visual Studio con las herramientas de C++ para compilar el menu."
}

$buildCommand = Join-Path $bin "build_menu_msvc.cmd"
$batchLines = @(
    "@echo off",
    "call `"$vcvars`" >nul",
    "cl /nologo /std:c++20 /EHsc /O2 /W3 /MD /I`"$($msvc.Include)`" `"$source`" /Fe:`"$output`" /link /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup /LIBPATH:`"$($msvc.Lib)`" raylib.lib opengl32.lib gdi32.lib winmm.lib shell32.lib user32.lib"
)
Set-Content -Path $buildCommand -Value $batchLines -Encoding ASCII

& cmd.exe /d /c "`"$buildCommand`""
if ($LASTEXITCODE -ne 0) {
    throw "No se pudo compilar el menu Raylib con MSVC."
}

Write-Host "Menu Raylib compilado en: $output"
