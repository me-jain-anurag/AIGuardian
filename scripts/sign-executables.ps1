# Sign executables with a self-signed certificate for Windows SmartScreen
# This allows Windows Defender to trust locally-built test executables
#
# NOTE: This is OPTIONAL and creates a LOCAL certificate only for YOUR machine.
# Other developers will need to run this script on their own machines.
#
# RECOMMENDED ALTERNATIVE: Add build folder to Defender exclusions instead.
# See docs/WINDOWS_SETUP.md for instructions.

$CertName = "GuardianAI Dev Certificate"
$BuildDir = Join-Path $PSScriptRoot "..\build\bin"

Write-Host "Checking for existing certificate..." -ForegroundColor Cyan

# Check if certificate already exists
$cert = Get-ChildItem -Path Cert:\CurrentUser\My | Where-Object { $_.Subject -match $CertName }

if (-not $cert) {
    Write-Host "Creating self-signed certificate for code signing..." -ForegroundColor Yellow
    
    $cert = New-SelfSignedCertificate `
        -Type CodeSigningCert `
        -Subject "CN=$CertName" `
        -KeyExportPolicy Exportable `
        -CertStoreLocation Cert:\CurrentUser\My `
        -NotAfter (Get-Date).AddYears(5) `
        -KeyLength 2048 `
        -KeyAlgorithm RSA `
        -HashAlgorithm SHA256
    
    Write-Host "Certificate created: $($cert.Thumbprint)" -ForegroundColor Green
    
    # Add certificate to Trusted Publishers to avoid SmartScreen warnings
    $store = New-Object System.Security.Cryptography.X509Certificates.X509Store("TrustedPublisher", "CurrentUser")
    $store.Open("ReadWrite")
    $store.Add($cert)
    $store.Close()
    
    Write-Host "Certificate added to Trusted Publishers" -ForegroundColor Green
} else {
    Write-Host "Using existing certificate: $($cert.Thumbprint)" -ForegroundColor Green
}

# Sign all executables in build/bin
Write-Host "`nSigning executables in $BuildDir..." -ForegroundColor Cyan

$exeFiles = Get-ChildItem -Path $BuildDir -Filter "*.exe" -ErrorAction SilentlyContinue

if ($exeFiles.Count -eq 0) {
    Write-Host "No executables found in $BuildDir" -ForegroundColor Yellow
    Write-Host "Build the project first with CMake" -ForegroundColor Yellow
    exit 1
}

foreach ($exe in $exeFiles) {
    Write-Host "Signing: $($exe.Name)..." -ForegroundColor White
    
    Set-AuthenticodeSignature -FilePath $exe.FullName -Certificate $cert -TimestampServer "http://timestamp.digicert.com" | Out-Null
    
    if ($?) {
        Write-Host "  ✓ Signed successfully" -ForegroundColor Green
    } else {
        Write-Host "  ✗ Failed to sign" -ForegroundColor Red
    }
}

Write-Host "`nAll executables signed!" -ForegroundColor Green
Write-Host "Windows SmartScreen should now trust these binaries." -ForegroundColor Cyan
