param(
    [string]$BuildRoot = "build/xquic",
    [int]$Port = 18443
)

$ErrorActionPreference = "Stop"
$build = (Resolve-Path $BuildRoot).Path
$serverExe = Get-ChildItem -Path $build -Filter "test_server.exe" -File -Recurse | Select-Object -First 1
$clientExe = Get-ChildItem -Path $build -Filter "test_client.exe" -File -Recurse | Select-Object -First 1
if ($null -eq $serverExe -or $null -eq $clientExe) {
    throw "XQUIC test_server.exe and test_client.exe must both be built before loopback validation."
}
$serverExe = $serverExe.FullName
$clientExe = $clientExe.FullName

if ($env:DBSTUDIO_LIBEVENT_DIR) {
    $libeventBin = Join-Path $env:DBSTUDIO_LIBEVENT_DIR "bin"
    if (Test-Path $libeventBin) {
        $env:PATH = "$libeventBin;$env:PATH"
    }
}

$evidence = Join-Path $build "loopback-evidence"
New-Item -ItemType Directory -Path $evidence -Force | Out-Null

# Native STATUS_STACK_BUFFER_OVERRUN failures do not reliably write to redirected
# stdout/stderr. Enable per-image Windows Error Reporting dumps on this ephemeral
# CI runner so the failing XQUIC process can be diagnosed from a full dump.
$dumpDirectory = Join-Path $evidence "crash-dumps"
New-Item -ItemType Directory -Path $dumpDirectory -Force | Out-Null
$werKey = "HKCU:\\Software\\Microsoft\\Windows\\Windows Error Reporting\\LocalDumps\\test_server.exe"
New-Item -Path $werKey -Force | Out-Null
New-ItemProperty -Path $werKey -Name DumpFolder -PropertyType ExpandString -Value $dumpDirectory -Force | Out-Null
New-ItemProperty -Path $werKey -Name DumpType -PropertyType DWord -Value 2 -Force | Out-Null
New-ItemProperty -Path $werKey -Name DumpCount -PropertyType DWord -Value 3 -Force | Out-Null

function New-PocCertificate {
    $rsa = [System.Security.Cryptography.RSA]::Create(2048)
    $certificate = $null
    try {
        $subject = [System.Security.Cryptography.X509Certificates.X500DistinguishedName]::new("CN=localhost")
        $request = [System.Security.Cryptography.X509Certificates.CertificateRequest]::new(
            $subject,
            $rsa,
            [System.Security.Cryptography.HashAlgorithmName]::SHA256,
            [System.Security.Cryptography.RSASignaturePadding]::Pkcs1
        )
        $request.CertificateExtensions.Add(
            [System.Security.Cryptography.X509Certificates.X509BasicConstraintsExtension]::new($false, $false, 0, $true)
        )
        $keyUsage = [System.Security.Cryptography.X509Certificates.X509KeyUsageFlags]::DigitalSignature -bor
            [System.Security.Cryptography.X509Certificates.X509KeyUsageFlags]::KeyEncipherment
        $request.CertificateExtensions.Add(
            [System.Security.Cryptography.X509Certificates.X509KeyUsageExtension]::new($keyUsage, $true)
        )
        $san = [System.Security.Cryptography.X509Certificates.SubjectAlternativeNameBuilder]::new()
        $san.AddDnsName("localhost")
        $san.AddIpAddress([System.Net.IPAddress]::Parse("127.0.0.1"))
        $request.CertificateExtensions.Add($san.Build())
        $eku = [System.Security.Cryptography.OidCollection]::new()
        [void]$eku.Add([System.Security.Cryptography.Oid]::new("1.3.6.1.5.5.7.3.1"))
        $request.CertificateExtensions.Add(
            [System.Security.Cryptography.X509Certificates.X509EnhancedKeyUsageExtension]::new($eku, $true)
        )
        $certificate = $request.CreateSelfSigned(
            [DateTimeOffset]::UtcNow.AddMinutes(-5),
            [DateTimeOffset]::UtcNow.AddHours(2)
        )
        return [pscustomobject]@{
            CertificatePem = $certificate.ExportCertificatePem()
            PrivateKeyPem = $rsa.ExportPkcs8PrivateKeyPem()
        }
    }
    finally {
        if ($null -ne $certificate) { $certificate.Dispose() }
        $rsa.Dispose()
    }
}

function Write-ScenarioCredentials {
    param(
        [string]$Directory,
        [string]$ServerCertificatePem,
        [string]$ServerPrivateKeyPem,
        [string]$ExpectedCertificatePem
    )
    New-Item -ItemType Directory -Path $Directory -Force | Out-Null
    $utf8 = [System.Text.UTF8Encoding]::new($false)
    [System.IO.File]::WriteAllText((Join-Path $Directory "server.crt"), $ServerCertificatePem, $utf8)
    [System.IO.File]::WriteAllText((Join-Path $Directory "server.key"), $ServerPrivateKeyPem, $utf8)
    [System.IO.File]::WriteAllText((Join-Path $Directory "expected_server.crt"), $ExpectedCertificatePem, $utf8)
}

function Start-PocServer {
    param([string]$Directory, [string[]]$Arguments, [string]$Name)
    $stdout = Join-Path $Directory "$Name-server.stdout.log"
    $stderr = Join-Path $Directory "$Name-server.stderr.log"
    $logFile = Join-Path $Directory "$Name-server.xquic.log"
    $serverArguments = @("-c", "b", "-l", "d", "-o", $logFile) + $Arguments
    $process = Start-Process -FilePath $serverExe -ArgumentList $serverArguments -WorkingDirectory $Directory -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    Start-Sleep -Milliseconds 1500
    $process.Refresh()
    if ($process.HasExited) {
        $exitCode = $process.ExitCode
        $outText = if (Test-Path $stdout) { Get-Content $stdout -Raw } else { "" }
        $errText = if (Test-Path $stderr) { Get-Content $stderr -Raw } else { "" }
        $xquicLog = if (Test-Path $logFile) { Get-Content $logFile -Raw } else { "" }
        throw "XQUIC $Name server exited during startup (exit=$exitCode). executable=$serverExe workingDirectory=$Directory args=$($serverArguments -join ' ') stdout=$outText stderr=$errText xquicLog=$xquicLog"
    }
    return $process
}

function Stop-PocServer {
    param($Process)
    if ($null -ne $Process) {
        $Process.Refresh()
        if (-not $Process.HasExited) {
            Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
            try { Wait-Process -Id $Process.Id -Timeout 5 -ErrorAction SilentlyContinue } catch {}
        }
    }
}

function Invoke-PocClient {
    param([string]$Directory, [string[]]$Arguments, [string]$Name)
    $stdout = Join-Path $Directory "$Name-client.stdout.log"
    $stderr = Join-Path $Directory "$Name-client.stderr.log"
    $process = Start-Process -FilePath $clientExe -ArgumentList $Arguments -WorkingDirectory $Directory -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    try {
        Wait-Process -Id $process.Id -Timeout 25 -ErrorAction Stop
    }
    catch {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        throw "XQUIC $Name client did not finish within 25 seconds."
    }
    $process.Refresh()
    $outText = if (Test-Path $stdout) { Get-Content $stdout -Raw } else { "" }
    $errText = if (Test-Path $stderr) { Get-Content $stderr -Raw } else { "" }
    return [pscustomobject]@{
        ExitCode = $process.ExitCode
        Output = $outText
        ErrorOutput = $errText
    }
}

function Assert-OutputContains {
    param([string]$Output, [string]$Expected, [string]$Message)
    if ($Output.IndexOf($Expected, [System.StringComparison]::Ordinal) -lt 0) {
        throw "$Message. Expected marker: $Expected"
    }
}

$good = New-PocCertificate
$bad = New-PocCertificate

# Reliable bidirectional stream echo over encrypted QUIC v1 / TLS 1.3.
$streamDir = Join-Path $evidence "stream"
Write-ScenarioCredentials $streamDir $good.CertificatePem $good.PrivateKeyPem $good.CertificatePem
$server = $null
try {
    $server = Start-PocServer $streamDir @("-a", "127.0.0.1", "-p", "$Port", "-e") "stream"
    $stream = Invoke-PocClient $streamDir @(
        "-a", "127.0.0.1", "-h", "localhost", "-p", "$Port",
        "-T", "1", "-1", "-V", "1", "-t", "5", "-s", "4096", "-E"
    ) "stream"
    if ($stream.ExitCode -ne 0) { throw "XQUIC stream client exited with code $($stream.ExitCode)." }
    Assert-OutputContains $stream.Output "[xquic-poc]|certificate_pin_match|" "The stream TLS certificate pin did not match."
    Assert-OutputContains $stream.Output "====>DCID" "The stream QUIC handshake did not complete."
    Assert-OutputContains $stream.Output ">>>>>>>> pass:1" "The reliable stream echo did not match the sent payload."
    if (Test-Path (Join-Path $streamDir "test_session")) { throw "The server issued a TLS session ticket; 0-RTT must remain disabled." }
    Write-Host "[xquic-loopback] TLS 1.3 / QUIC v1 certificate-pinned reliable stream echo: PASS"
}
finally {
    Stop-PocServer $server
}

# Bidirectional QUIC DATAGRAM echo. The payload is small enough to avoid
# turning this transport capability check into a path-MTU test.
$datagramDir = Join-Path $evidence "datagram"
Write-ScenarioCredentials $datagramDir $good.CertificatePem $good.PrivateKeyPem $good.CertificatePem
$server = $null
try {
    $server = Start-PocServer $datagramDir @(
        "-a", "127.0.0.1", "-p", "$Port", "-e", "-U", "1", "-Q", "1200"
    ) "datagram"
    $datagram = Invoke-PocClient $datagramDir @(
        "-a", "127.0.0.1", "-h", "localhost", "-p", "$Port",
        "-T", "1", "-1", "-V", "1", "-t", "5", "-s", "900", "-E", "-U", "1", "-Q", "1200"
    ) "datagram"
    if ($datagram.ExitCode -ne 0) { throw "XQUIC DATAGRAM client exited with code $($datagram.ExitCode)." }
    Assert-OutputContains $datagram.Output "[xquic-poc]|certificate_pin_match|" "The DATAGRAM TLS certificate pin did not match."
    Assert-OutputContains $datagram.Output "====>DCID" "The DATAGRAM QUIC handshake did not complete."
    Assert-OutputContains $datagram.Output "[dgram]|echo_check|same_content:yes|" "The QUIC DATAGRAM echo did not match the sent payload."
    Write-Host "[xquic-loopback] TLS 1.3 / QUIC v1 bidirectional DATAGRAM echo: PASS"
}
finally {
    Stop-PocServer $server
}

# Negative certificate test: same DNS name, different self-signed leaf key.
# The client must reject it because the configured leaf-certificate pin differs.
$negativeDir = Join-Path $evidence "wrong-certificate"
Write-ScenarioCredentials $negativeDir $bad.CertificatePem $bad.PrivateKeyPem $good.CertificatePem
$server = $null
try {
    $server = Start-PocServer $negativeDir @("-a", "127.0.0.1", "-p", "$Port", "-e") "wrong-certificate"
    $negative = Invoke-PocClient $negativeDir @(
        "-a", "127.0.0.1", "-h", "localhost", "-p", "$Port",
        "-T", "1", "-1", "-V", "1", "-t", "3", "-s", "256"
    ) "wrong-certificate"
    Assert-OutputContains $negative.Output "[xquic-poc]|certificate_pin_mismatch|" "The client did not reject the unpinned server certificate."
    if ($negative.Output.Contains("====>DCID")) {
        throw "The client completed a QUIC handshake with a server certificate that did not match its pin."
    }
    Write-Host "[xquic-loopback] wrong server certificate rejected before handshake completion: PASS"
}
finally {
    Stop-PocServer $server
}

$summary = @"
### XQUIC Windows loopback validation
- TLS 1.3 / QUIC v1 / encrypted 1-RTT handshake with exact leaf-certificate pin: **PASS**
- Server TLS session tickets disabled; no client resumption ticket saved: **PASS**
- Reliable bidirectional QUIC stream echo: **PASS**
- Bidirectional QUIC DATAGRAM echo: **PASS**
- Different self-signed certificate with the same hostname rejected by the pin check: **PASS**
- Scope: Windows localhost loopback only. This does not validate Android interoperability, Wi-Fi behavior, production device authentication, AUX authorization, or audio streaming.
"@
if ($env:GITHUB_STEP_SUMMARY) {
    [System.IO.File]::AppendAllText($env:GITHUB_STEP_SUMMARY, $summary + [Environment]::NewLine)
}
Write-Host "All XQUIC Windows loopback checks passed."
