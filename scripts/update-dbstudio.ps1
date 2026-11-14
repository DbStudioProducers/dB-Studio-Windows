param(
    [Parameter(Mandatory=$true)][string]$InstallDir
)
$ErrorActionPreference = "Stop"
$repo = "DbStudioProducers/dB-Studio-Windows"
$assetName = "dB-Studio-Windows-8.1.zip"
$hashAssetName = "$assetName.sha256"
$temp = Join-Path $env:TEMP ("dBStudioUpdate-" + [guid]::NewGuid().ToString("N"))
$exePath = Join-Path $InstallDir "dB Studio.exe"
$appName = [IO.Path]::GetFileNameWithoutExtension($exePath)
try {
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    New-Item -ItemType Directory -Path $temp | Out-Null
    $release = Invoke-RestMethod -Headers @{ "User-Agent" = "dB-Studio-Updater" } -Uri "https://api.github.com/repos/$repo/releases/latest"
    $zipAsset = $release.assets | Where-Object { $_.name -eq $assetName } | Select-Object -First 1
    $hashAsset = $release.assets | Where-Object { $_.name -eq $hashAssetName } | Select-Object -First 1
    if (-not $zipAsset -or -not $hashAsset) { throw "A versão mais recente ainda não possui o pacote Windows 8.1 e o arquivo SHA-256." }
    $zipPath = Join-Path $temp $assetName
    $hashPath = Join-Path $temp $hashAssetName
    Invoke-WebRequest -Headers @{ "User-Agent" = "dB-Studio-Updater" } -Uri $zipAsset.browser_download_url -OutFile $zipPath
    Invoke-WebRequest -Headers @{ "User-Agent" = "dB-Studio-Updater" } -Uri $hashAsset.browser_download_url -OutFile $hashPath
    $expected = ((Get-Content -Raw $hashPath) -split '\s+')[0].Trim().ToLowerInvariant()
    $actual = (Get-FileHash -Algorithm SHA256 $zipPath).Hash.ToLowerInvariant()
    if ($expected -notmatch '^[0-9a-f]{64}$' -or $actual -ne $expected) { throw "Falha na verificação SHA-256. Nenhum arquivo foi alterado." }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $expandedPath = Join-Path $temp "expanded"
    [IO.Compression.ZipFile]::ExtractToDirectory($zipPath, $expandedPath)
    $newExe = Get-ChildItem -Path ($expandedPath) -Filter "dB Studio.exe" -Recurse | Select-Object -First 1
    if (-not $newExe) { throw "O pacote não contém dB Studio.exe." }
    while (Get-Process -Name $appName -ErrorAction SilentlyContinue) { Start-Sleep -Seconds 1 }
    $backup = "$exePath.previous"
    if (Test-Path $backup) { Remove-Item -Force $backup }
    if (Test-Path $exePath) { Move-Item -LiteralPath $exePath -Destination $backup }
    try {
        Copy-Item -LiteralPath $newExe.FullName -Destination $exePath -Force
        if ((Get-FileHash -Algorithm SHA256 $exePath).Hash -ne (Get-FileHash -Algorithm SHA256 $newExe.FullName).Hash) { throw "Falha ao validar o executável instalado." }
        if (Test-Path $backup) { Remove-Item -Force $backup }
        Start-Process -FilePath $exePath
        Write-Host "dB Studio atualizado com sucesso."
    } catch {
        if (Test-Path $exePath) { Remove-Item -Force $exePath }
        if (Test-Path $backup) { Move-Item -LiteralPath $backup -Destination $exePath }
        throw
    }
} catch {
    Write-Error $_
    Read-Host "Pressione Enter para fechar"
} finally {
    if (Test-Path $temp) { Remove-Item -Recurse -Force $temp -ErrorAction SilentlyContinue }
}
