param([Parameter(Mandatory=$true)][string]$ProjectRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$port = Join-Path $ProjectRoot "tools/fido/fido-linux.ps1"
. $port -LibraryMode

function Assert-True([bool]$Value, [string]$Message)
{
    if (!$Value) {
        throw $Message
    }
}

function Reset-TestSelection
{
    $script:Win = "Windows 11"
    $script:Rel = "Latest"
    $script:Ed = "Home/Pro/Edu"
    $script:Lang = "Korean"
    $script:Arch = "x64"
    $script:PlatformArch = "x64"
    $script:GetUrl = $true
    $script:Verbose = $false
    $script:Debug = $false
    $script:SessionIds = @($null) * 8
    $script:WebSessions = @($null) * 8
}

function Install-SuccessHook
{
    $script:RequestHook = {
        param($Stage, $Uri, $Session, $Headers, $AsRest)
        $null = $Uri
        $null = $Session
        $null = $Headers
        $null = $AsRest
        switch ($Stage) {
            "tags" { return [pscustomobject]@{ StatusCode = 200 } }
            "mdt" { return '?w=ABCDEF&rticks="+12345' }
            "ov-reply" { return [pscustomobject]@{ StatusCode = 200 } }
            "sku" {
                return [pscustomobject]@{
                    Errors = @()
                    Skus = @([pscustomobject]@{
                        Language = "Korean"
                        LocalizedLanguage = "Korean"
                        Id = "mock-korean-sku"
                    })
                }
            }
            "links" {
                return [pscustomobject]@{
                    Errors = @()
                    ProductDownloadOptions = @([pscustomobject]@{
                        DownloadType = 1
                        Uri = "https://software.download.prss.microsoft.com/db/mock.iso?token=redacted"
                    })
                }
            }
            default { throw "Unexpected mock stage: $Stage" }
        }
    }
}

Assert-True ((Get-Arch-From-Type 0) -eq "x86") "x86 mapping failed"
Assert-True ((Get-Arch-From-Type 1) -eq "x64") "x64 mapping failed"
Assert-True ((Get-Arch-From-Type 2) -eq "ARM64") "ARM64 mapping failed"
Assert-True ((Get-Arch-From-Type 99) -eq "Unknown") "unknown mapping failed"

Reset-TestSelection
Install-SuccessHook
$output = @(Invoke-FidoLinuxMain)
Assert-True ($output.Count -eq 1) "GetUrl emitted more than one stdout item"
Assert-True ($output[0] -eq
    "https://software.download.prss.microsoft.com/db/mock.iso?token=redacted") `
    "signed-link mock output changed"

Reset-TestSelection
Install-SuccessHook
$script:Arch = "sparc"
try {
    Invoke-FidoLinuxMain
    throw "invalid architecture was accepted"
} catch {
    Assert-True ($_.Exception.Data["FidoExitCode"] -eq 4) `
        "invalid architecture exit code changed"
}

Reset-TestSelection
Install-SuccessHook
$script:Lang = "Unavailable Language"
try {
    Invoke-FidoLinuxMain
    throw "invalid language was accepted"
} catch {
    Assert-True ($_.Exception.Data["FidoExitCode"] -eq 4) `
        "invalid language exit code changed"
}

Reset-TestSelection
$script:RequestHook = {
    param($Stage, $Uri, $Session, $Headers, $AsRest)
    $null = $Uri
    $null = $Session
    $null = $Headers
    $null = $AsRest
    if ($Stage -eq "mdt") { return '?w=ABCDEF&rticks="+12345' }
    if ($Stage -eq "sku") {
        return [pscustomobject]@{
            Errors = @([pscustomobject]@{ Type = 9; Value = "rejected" })
            Skus = @()
        }
    }
    return [pscustomobject]@{ StatusCode = 200 }
}
try {
    Invoke-FidoLinuxMain
    throw "Microsoft error response was accepted"
} catch {
    Assert-True ($_.Exception.Data["FidoExitCode"] -eq 3) `
        "Microsoft error exit code changed"
    Assert-True (!$_.Exception.Message.Contains("rejected")) `
        "remote error body leaked"
}
