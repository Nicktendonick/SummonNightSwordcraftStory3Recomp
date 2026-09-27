param([Parameter(Mandatory=$true)][string]$Source)
$ErrorActionPreference = 'Stop'
$assetDir = Join-Path (Split-Path -Parent $PSScriptRoot) 'assets/beta'
New-Item -ItemType Directory -Path $assetDir -Force | Out-Null
# Format conversion only. Preserve the supplied artwork and its source hash.
Copy-Item -LiteralPath $Source -Destination (Join-Path $assetDir 'beta-icon-source.png')
Add-Type -AssemblyName System.Drawing
$sourceImage = [Drawing.Image]::FromFile((Resolve-Path -LiteralPath $Source).Path)
$images = @()
try {
    foreach ($size in @(16,24,32,48,64,128,256)) {
        $bitmap = [Drawing.Bitmap]::new($size,$size)
        $graphics = [Drawing.Graphics]::FromImage($bitmap)
        $memory = [IO.MemoryStream]::new()
        try {
            $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $graphics.DrawImage($sourceImage,0,0,$size,$size)
            $bitmap.Save($memory,[Drawing.Imaging.ImageFormat]::Png)
            $images += ,@($size,$memory.ToArray())
        } finally { $graphics.Dispose(); $bitmap.Dispose(); $memory.Dispose() }
    }
    $stream = [IO.File]::Create((Join-Path $assetDir 'beta.ico'))
    $writer = [IO.BinaryWriter]::new($stream)
    try {
        $writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$images.Count)
        $offset = 6 + 16*$images.Count
        foreach ($entry in $images) {
            $dimension = $entry[0] % 256
            $writer.Write([byte]$dimension); $writer.Write([byte]$dimension)
            $writer.Write([byte]0); $writer.Write([byte]0)
            $writer.Write([uint16]1); $writer.Write([uint16]32)
            $writer.Write([uint32]$entry[1].Length); $writer.Write([uint32]$offset)
            $offset += $entry[1].Length
        }
        foreach ($entry in $images) { $writer.Write([byte[]]$entry[1]) }
    } finally { $writer.Dispose(); $stream.Dispose() }
} finally { $sourceImage.Dispose() }
Get-FileHash -LiteralPath $Source,(Join-Path $assetDir 'beta-icon-source.png'),(Join-Path $assetDir 'beta.ico')
