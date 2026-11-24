param(
    [string]$XquicRoot = "third_party/xquic"
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path $XquicRoot).Path

function Require-Pattern {
    param([string]$Text, [string]$Pattern, [string]$Message)
    if (-not [regex]::IsMatch($Text, $Pattern, [System.Text.RegularExpressions.RegexOptions]::Singleline)) {
        throw $Message
    }
}

$tlsContext = Get-Content (Join-Path $root "src/tls/xqc_tls_ctx.c") -Raw
$clientTransport = Get-Content (Join-Path $root "src/transport/xqc_client.c") -Raw
$publicHeader = Get-Content (Join-Path $root "include/xquic/xquic.h") -Raw
$cmake = Get-Content (Join-Path $root "CMakeLists.txt") -Raw
$testClient = Get-Content (Join-Path $root "tests/test_client.c") -Raw
$testServer = Get-Content (Join-Path $root "tests/test_server.c") -Raw
$testCmake = Get-Content (Join-Path $root "tests/CMakeLists.txt") -Raw

$minTls13 = [regex]::Matches($tlsContext, 'SSL_CTX_set_min_proto_version\s*\(\s*ssl_ctx\s*,\s*TLS1_3_VERSION\s*\)').Count
Require-Pattern $tlsContext 'SSL_CTX_set_options\s*\(\s*ssl_ctx\s*,\s*SSL_OP_NO_TICKET\s*\)' "XQUIC server TLS context must disable session tickets and 0-RTT."
$maxTls13 = [regex]::Matches($tlsContext, 'SSL_CTX_set_max_proto_version\s*\(\s*ssl_ctx\s*,\s*TLS1_3_VERSION\s*\)').Count
if ($minTls13 -lt 2 -or $maxTls13 -lt 2) {
    throw "XQUIC must pin both client and server TLS contexts to TLS 1.3 (min/max)."
}

Require-Pattern $clientTransport 'if\s*\(no_crypto_flag\s*==\s*1\).*?local_settings->no_crypto\s*=\s*XQC_TRUE;.*?else\s*\{.*?local_settings->no_crypto\s*=\s*XQC_FALSE;' "Could not verify XQUIC runtime no_crypto_flag semantics."
Require-Pattern $publicHeader 'XQC_VERSION_V1' "XQUIC public API does not expose QUIC v1."
Require-Pattern $publicHeader 'max_datagram_frame_size' "XQUIC public API does not expose DATAGRAM transport parameters."
Require-Pattern $publicHeader 'xqc_datagram_send\s*\(' "XQUIC public API does not expose QUIC DATAGRAM send."
Require-Pattern $publicHeader 'xqc_stream_create_with_direction\s*\(' "XQUIC public API does not expose reliable stream creation."
Require-Pattern $publicHeader 'xqc_stream_send\s*\(' "XQUIC public API does not expose reliable stream send."
Require-Pattern $publicHeader 'xqc_stream_recv\s*\(' "XQUIC public API does not expose reliable stream receive."

if ($cmake -notmatch 'add_definitions\(-DNOCRYPT=1\)') {
    throw "Pinned XQUIC CMake no longer declares the NOCRYPT definition; review the crypto configuration."
}
$sourceRoots = @((Join-Path $root "src"), (Join-Path $root "include"))
$noCryptReferences = @(Get-ChildItem -Path $sourceRoots -Recurse -File | Select-String -Pattern '\bNOCRYPT\b')
if ($noCryptReferences.Count -gt 0) {
    $details = ($noCryptReferences | ForEach-Object { "$($_.Path):$($_.LineNumber): $($_.Line.Trim())" }) -join [Environment]::NewLine
    throw "NOCRYPT is consumed by XQUIC source; the crypto configuration must be reviewed before proceeding." + [Environment]::NewLine + $details
}

Require-Pattern $testClient 'g_no_crypt\s*=\s*0;' "XQUIC loopback client must default to encrypted transport."
Require-Pattern $testClient 'conn_settings\.proto_version\s*=\s*XQC_VERSION_V1;' "XQUIC loopback client must select QUIC v1."
Require-Pattern $testClient 'xqc_connect\(ctx\.engine,\s*&conn_settings,\s*user_conn->token,\s*user_conn->token_len,\s*g_host,\s*0,\s*&conn_ssl_config' "XQUIC loopback client must pass no_crypto_flag=0 and the pinned hostname."
Require-Pattern $testClient 'certificate_pin_mismatch' "XQUIC loopback client certificate pin rejection patch is missing."
Require-Pattern $testClient 'memcmp\(certs\[0\],\s*expected_der' "XQUIC loopback client does not compare the peer leaf certificate pin."
Require-Pattern $testClient 'use_1rtt\s*=\s*1;' "XQUIC loopback client must support forcing 1-RTT without 0-RTT tickets."
Require-Pattern $testClient 'if\s*\(session_len\s*<\s*0\s*\|\|\s*tp_len\s*<\s*0\s*\|\|\s*use_1rtt\s*\).*?conn_ssl_config\.session_ticket_data\s*=\s*NULL;.*?conn_ssl_config\.transport_parameter_data\s*=\s*NULL;' "XQUIC loopback client must not offer session tickets or early data."
Require-Pattern $testClient 'XQC_TLS_CERT_FLAG_NEED_VERIFY' "XQUIC loopback client certificate verification is not enabled."
Require-Pattern $testClient 'XQC_TLS_CERT_FLAG_ALLOW_SELF_SIGNED' "XQUIC loopback test self-signed pin path is unavailable."
Require-Pattern $testServer 'config\.support_version_count\s*=\s*1;.*?config\.support_version_list\[0\]\s*=\s*XQC_VERSION_V1_VALUE;' "XQUIC loopback server must explicitly negotiate QUIC v1 only."

if ($testCmake -match '(?m)^\s*-lm\s*$') {
    throw "The Windows test-app CMake target still links the Unix-only -lm flag."
}

Write-Host "[xquic-contract] TLS 1.3 min/max on client and server: PASS"
Write-Host "[xquic-contract] QUIC v1, reliable streams and DATAGRAM APIs: PASS"
Write-Host "[xquic-contract] runtime crypto flag semantics; no NOCRYPT source consumer: PASS"
Write-Host "[xquic-contract] loopback client uses encrypted QUIC v1, 1-RTT and certificate pinning: PASS"
Write-Host "[xquic-contract] Windows test-app link configuration: PASS"
