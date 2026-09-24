# Reproducible, portable tooling installation. No global Java/PATH changes.
[CmdletBinding()]
param([string]$ProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../../..')),
      [switch]$RebuildLoader)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$base = Join-Path $ProjectRoot '.tooling/ghidra'
$install = Join-Path $base 'ghidra_12.0.4_PUBLIC'
foreach ($dir in @('downloads','sources','settings','cache','tmp','loader-classes')) {
    New-Item -ItemType Directory -Force -Path (Join-Path $base $dir) | Out-Null
}
$assets = @(
    @('https://github.com/NationalSecurityAgency/ghidra/releases/download/Ghidra_12.0.4_build/ghidra_12.0.4_PUBLIC_20260303.zip','ghidra.zip','c3b458661d69e26e203d739c0c82d143cc8a4a29d9e571f099c2cf4bda62a120'),
    @('https://github.com/adoptium/temurin21-binaries/releases/download/jdk-21.0.12.1%2B1/OpenJDK21U-jdk_x64_windows_hotspot_21.0.12.1_1.zip','jdk.zip','f9d6e191ab098c0d416e7d588a24420a8621cd2f4720dab2459b8b7b2d2d8b4e'),
    @('https://github.com/13bm/GhidraMCP/releases/download/v0.2.2%2Bghidra12.0.4/ghidra_12.0.4_PUBLIC_20260323_GhidraMCP.zip','mcp.zip','f10adcc8ae4aab1ddc82240f96cca0f1fa0412d0cbab6ff83759613f8e1ee03d')
)
foreach ($asset in $assets) {
    $file = Join-Path $base ('downloads/' + $asset[1])
    if (-not (Test-Path -LiteralPath $file)) { Invoke-WebRequest -Uri $asset[0] -OutFile $file }
    if ((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $asset[2]) {
        throw "Checksum mismatch: $file (retained for inspection)"
    }
}
if (-not (Test-Path -LiteralPath "$install/Ghidra/application.properties")) {
    Expand-Archive -LiteralPath "$base/downloads/ghidra.zip" -DestinationPath $base
}
$jdk = Join-Path $base 'jdk-21.0.12.1+1'
if (-not (Test-Path -LiteralPath "$jdk/bin/javac.exe")) {
    Expand-Archive -LiteralPath "$base/downloads/jdk.zip" -DestinationPath $base
}
if (-not (Test-Path -LiteralPath "$install/Ghidra/Extensions/GhidraMCP/lib/GhidraMCP.jar")) {
    Expand-Archive -LiteralPath "$base/downloads/mcp.zip" -DestinationPath "$install/Ghidra/Extensions"
}
$repos = @(
    @('pudii/gba-ghidra-loader','1.1.0','70fc62c388741bb03e5f4f68eaf0b491b223430d','gba-ghidra-loader'),
    @('13bm/GhidraMCP','v0.2.2+ghidra12.0.4','ffe716235a292e0c19714525bd0b6fab4925829b','GhidraMCP')
)
foreach ($repo in $repos) {
    $destination = Join-Path $base ('sources/' + $repo[3])
    if (-not (Test-Path -LiteralPath $destination)) {
        git clone --depth 1 --branch $repo[1] "https://github.com/$($repo[0]).git" $destination
        if ($LASTEXITCODE -ne 0) { throw 'Source checkout failed' }
    }
    if ((git -C $destination rev-parse HEAD) -ne $repo[2]) { throw "Unexpected source revision: $destination" }
}
$source = Join-Path $base 'sources/gba-ghidra-loader'
$extension = "$install/Ghidra/Extensions/gba-ghidra-loader"
if ((Test-Path -LiteralPath "$extension/lib/gba-ghidra-loader.jar") -and -not $RebuildLoader) {
    $contents = & "$jdk/bin/jar.exe" --list --file "$extension/lib/gba-ghidra-loader.jar"
    if ($LASTEXITCODE -ne 0 -or $contents -notcontains 'gba/GBALoader.class') { throw 'Invalid installed loader' }
    if (-not (Select-String -LiteralPath "$extension/extension.properties" -Pattern '^version=12\.0\.4$' -Quiet)) {
        throw 'Installed loader version mismatch'
    }
    Write-Output 'Verified installed tool archives, pinned sources, and GBA loader. No rebuild needed.'
    return
}
$cp = (@('Framework/Utility','Framework/Generic','Framework/SoftwareModeling',
    'Framework/Project','Framework/DB','Framework/Docking','Framework/FileSystem',
    'Features/Base','Features/Jython') | ForEach-Object { "$install/Ghidra/$_/lib/*" }) -join ';'
$files = (Get-ChildItem "$source/src/main/java/gba" -Filter '*.java').FullName
& "$jdk/bin/javac.exe" -proc:none -cp $cp -d "$base/loader-classes" @files
if ($LASTEXITCODE -ne 0) { throw 'GBA loader compilation failed' }
New-Item -ItemType Directory -Force -Path "$extension/lib" | Out-Null
& "$jdk/bin/jar.exe" --create --file "$extension/lib/gba-ghidra-loader.jar" -C "$base/loader-classes" .
if ($LASTEXITCODE -ne 0) { throw 'GBA loader packaging failed' }
if (-not (Test-Path -LiteralPath "$extension/data")) {
    Copy-Item -LiteralPath "$source/data" -Destination $extension -Recurse
}
Copy-Item -LiteralPath "$source/Module.manifest","$source/LICENSE" -Destination $extension
# Render the upstream packaging template, as its Gradle build does.
$properties = (Get-Content -LiteralPath "$source/extension.properties" -Raw).
    Replace('@extname@','gba-ghidra-loader').Replace('@extversion@','12.0.4')
[IO.File]::WriteAllText("$extension/extension.properties",$properties)
Write-Output "Installed and verified portable Ghidra 12.0.4 + GBA loader 1.1.0 + GhidraMCP 0.2.2 in $base"
