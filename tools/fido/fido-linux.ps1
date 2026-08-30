#!/usr/bin/env pwsh
<#
Fido v1.70 - ISO Downloader, for Microsoft Windows and UEFI Shell
Copyright © 2019-2026 Pete Batard <pete@akeo.ie>
Command line support: Copyright © 2021 flx5
ConvertTo-ImageSource: Copyright © 2016 Chris Carter

ClassicSetup Linux CLI adaptation copyright © 2026 ClassicSetup contributors.
This is a reduced Linux-only command-line adaptation of Fido v1.70. It keeps
the Microsoft Retail ISO resolver sequence and omits the Windows GUI/download
components. Modified source is distributed under GNU GPL version 3 or later.

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version. This program is distributed WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See LICENSE.txt for the complete license.
#>

param(
    [string]$Win,
    [string]$Rel,
    [string]$Ed,
    [string]$Lang,
    [string]$Arch,
    [string]$Locale = "en-US",
    [switch]$GetUrl,
    [switch]$Verbose,
    [switch]$Debug,
    [string]$PlatformArch,
    [switch]$LibraryMode
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# Product/release data copied from the uploaded original Fido v1.70 source.
$script:WindowsVersions = @(
    @(
        @("Windows 11", "windows11"),
        @(
            "25H2 v2 (Build 26200.8037 - 2026.03)",
            @("Windows 11 Home/Pro/Edu", @(3321, 3324)),
            @("Windows 11 Home China ", @(3322, 3325)),
            @("Windows 11 Pro China ", @(3323, 3326))
        )
    ),
    @(
        @("Windows 10", "Windows10ISO"),
        @(
            "22H2 v1 (Build 19045.2965 - 2023.05)",
            @("Windows 10 Home/Pro/Edu", 2618),
            @("Windows 10 Home China ", 2378)
        )
    )
)

$script:DefaultTimeout = 30
$script:OrgId = "y6jn8c31"
$script:ProfileId = "606624d44113"
$script:InstanceId = "560dc9f3-1aa5-4a2f-b63c-9e18f8d0e175"
$script:QueryLocale = $Locale
$script:SessionIds = @($null) * 8
$script:WebSessions = @($null) * 8
$script:RequestHook = $null

function Throw-FidoError([int]$ExitCode, [string]$Message)
{
    $exception = [System.Exception]::new($Message)
    $exception.Data["FidoExitCode"] = $ExitCode
    throw $exception
}

function Write-FidoDiagnostic([string]$Stage, [string]$Message)
{
    if ($Verbose -or $Debug) {
        [Console]::Error.WriteLine("fido-linux: stage={0} {1}", $Stage, $Message)
    }
}

function Get-Platform-Arch
{
    $native = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture
    switch ([string]$native) {
        "X64" { return "x64" }
        "X86" { return "x86" }
        "Arm64" { return "ARM64" }
        default { Throw-FidoError 4 "The host architecture is unsupported." }
    }
}

function Get-Arch-From-Type([int]$Type)
{
    switch ($Type) {
        0 { return "x86" }
        1 { return "x64" }
        2 { return "ARM64" }
        default { return "Unknown" }
    }
}

function Test-Selection([string]$Candidate, [string]$Selection)
{
    if ([string]::IsNullOrWhiteSpace($Selection)) {
        return $true
    }
    return $Candidate.IndexOf(
        $Selection, [System.StringComparison]::OrdinalIgnoreCase) -ge 0
}

function Get-Windows-Releases([int]$SelectedVersion)
{
    $releases = @()
    for ($index = 1; $index -lt $script:WindowsVersions[$SelectedVersion].Count;
         $index++) {
        $release = $script:WindowsVersions[$SelectedVersion][$index]
        $releases += [pscustomobject]@{
            Release = [string]$release[0]
            Index = $index
        }
    }
    return $releases
}

function Get-Windows-Editions([int]$SelectedVersion, [int]$SelectedRelease)
{
    $editions = @()
    $release = $script:WindowsVersions[$SelectedVersion][$SelectedRelease]
    for ($index = 1; $index -lt $release.Count; $index++) {
        $edition = $release[$index]
        if (!$edition[0].Contains("China") -or $Locale.StartsWith("zh")) {
            $editions += [pscustomobject]@{
                Edition = [string]$edition[0]
                Id = $edition[1]
            }
        }
    }
    return $editions
}

function Invoke-FidoRequest(
    [string]$Stage,
    [uri]$Uri,
    [object]$Session,
    [hashtable]$Headers,
    [bool]$AsRest,
    [bool]$NoRedirect,
    [bool]$UseSessionVariable = $false)
{
    Write-FidoDiagnostic $Stage ("host=" + $Uri.Host)
    if ($null -ne $script:RequestHook) {
        return & $script:RequestHook $Stage $Uri $Session $Headers $AsRest
    }
    try {
        $parameters = @{
            Uri = $Uri
            UseBasicParsing = $true
            TimeoutSec = $script:DefaultTimeout
        }
        if ($UseSessionVariable) {
            $parameters.SessionVariable = "Session"
        } elseif ($null -ne $Session) {
            $parameters.WebSession = $Session
        }
        if ($null -ne $Headers) {
            $parameters.Headers = $Headers
        }
        if ($NoRedirect) {
            $parameters.MaximumRedirection = 0
        }
        if ($AsRest) {
            return Invoke-RestMethod @parameters
        }
        return Invoke-WebRequest @parameters
    } catch {
        Throw-FidoError 5 ("Microsoft request failed during " + $Stage + ".")
    }
}

function Get-Windows-Languages([int]$SelectedVersion, [object]$SelectedEdition)
{
    $languages = [ordered]@{}
    $sessionIndex = 0

    foreach ($editionId in @($SelectedEdition)) {
        $sessionId = [guid]::NewGuid().ToString()
        $webSession = New-Object Microsoft.PowerShell.Commands.WebRequestSession
        $script:SessionIds[$sessionIndex] = $sessionId
        $script:WebSessions[$sessionIndex] = $webSession

        $tags = [uri]("https://vlscppe.microsoft.com/tags?org_id=" +
            $script:OrgId + "&session_id=" + $sessionId)
        [void](Invoke-FidoRequest "tags" $tags $webSession $null $false $true)

        $mdtUri = [uri]("https://ov-df.microsoft.com/mdt.js?instanceId=" +
            $script:InstanceId + "&PageId=si&session_id=" + $sessionId)
        $mdt = Invoke-FidoRequest "mdt" $mdtUri $webSession $null $true $true
        $mdtText = [string]$mdt
        $w = $null
        $rticks = $null
        if ($mdtText -match '[?&]w=([A-F0-9]+)') {
            $w = $matches[1]
        }
        if ($mdtText -match 'rticks\=\"\+?(\d+)') {
            $rticks = $matches[1]
        }
        if ([string]::IsNullOrEmpty($w) -or
            [string]::IsNullOrEmpty($rticks)) {
            Throw-FidoError 3 "Microsoft source session metadata could not be parsed."
        }

        $reply = [uri]("https://ov-df.microsoft.com/?session_id=" + $sessionId +
            "&CustomerId=" + $script:InstanceId + "&PageId=si&w=" + $w +
            "&mdt=" + [DateTimeOffset]::Now.ToUnixTimeMilliseconds() +
            "&rticks=" + $rticks)
        [void](Invoke-FidoRequest "ov-reply" $reply $webSession $null $false $true)

        $skuUri = [uri](
            "https://www.microsoft.com/software-download-connector/api/" +
            "getskuinformationbyproductedition?profile=" + $script:ProfileId +
            "&productEditionId=" + $editionId +
            "&SKU=undefined&friendlyFileName=undefined&Locale=" +
            $script:QueryLocale + "&sessionID=" + $sessionId)
        $skuResponse = Invoke-FidoRequest "sku" $skuUri $null $null $true $false $true
        if ($null -eq $skuResponse) {
            Throw-FidoError 3 "Microsoft returned no language metadata."
        }
        $errorsProperty = $skuResponse.PSObject.Properties["Errors"]
        if ($null -ne $errorsProperty -and
            @($errorsProperty.Value).Count -gt 0) {
            Throw-FidoError 3 "Microsoft did not accept the language request."
        }
        $skusProperty = $skuResponse.PSObject.Properties["Skus"]
        if ($null -eq $skusProperty) {
            Throw-FidoError 3 "Microsoft language metadata changed schema."
        }
        foreach ($sku in @($skusProperty.Value)) {
            if (!$languages.Contains([string]$sku.Language)) {
                $languages[[string]$sku.Language] = @{
                    DisplayName = [string]$sku.LocalizedLanguage
                    Data = @()
                }
            }
            $languages[[string]$sku.Language].Data += @{
                SessionIndex = $sessionIndex
                SkuId = [string]$sku.Id
            }
        }
        $sessionIndex++
    }

    if ($languages.Count -eq 0) {
        Throw-FidoError 3 "Microsoft returned no Windows languages."
    }
    $result = @()
    foreach ($name in $languages.Keys) {
        $result += [pscustomobject]@{
            DisplayName = $languages[$name].DisplayName
            Name = $name
            Data = $languages[$name].Data
        }
    }
    return $result
}

function Test-Official-Microsoft-Uri([string]$Value)
{
    $parsed = $null
    if (![uri]::TryCreate($Value, [System.UriKind]::Absolute, [ref]$parsed) -or
        $parsed.Scheme -ne "https") {
        return $false
    }
    return $parsed.Host -eq "microsoft.com" -or
        $parsed.Host.EndsWith(".microsoft.com",
            [System.StringComparison]::OrdinalIgnoreCase)
}

function Get-Windows-Download-Links([pscustomobject]$SelectedLanguage)
{
    $links = @()
    foreach ($entry in @($SelectedLanguage.Data)) {
        $sessionIndex = [int]$entry.SessionIndex
        $sessionId = $script:SessionIds[$sessionIndex]
        $linkUri = [uri](
            "https://www.microsoft.com/software-download-connector/api/" +
            "GetProductDownloadLinksBySku?profile=" + $script:ProfileId +
            "&productEditionId=undefined&SKU=" + $entry.SkuId +
            "&friendlyFileName=undefined&Locale=" + $script:QueryLocale +
            "&sessionID=" + $sessionId)
        $headers = @{ Referer = "https://www.microsoft.com/software-download/windows11" }
        $response = Invoke-FidoRequest "links" $linkUri $null $headers $true $false $true
        if ($null -eq $response) {
            Throw-FidoError 3 "Microsoft returned no download-link metadata."
        }
        $errorsProperty = $response.PSObject.Properties["Errors"]
        if ($null -ne $errorsProperty -and
            @($errorsProperty.Value).Count -gt 0) {
            Throw-FidoError 3 "Microsoft did not accept the download-link request."
        }
        $optionsProperty = $response.PSObject.Properties["ProductDownloadOptions"]
        if ($null -eq $optionsProperty) {
            Throw-FidoError 3 "Microsoft download-link metadata changed schema."
        }
        foreach ($option in @($optionsProperty.Value)) {
            $candidate = [string]$option.Uri
            if (!(Test-Official-Microsoft-Uri $candidate)) {
                Throw-FidoError 5 "Microsoft returned a download URI outside policy."
            }
            $links += [pscustomobject]@{
                Arch = Get-Arch-From-Type ([int]$option.DownloadType)
                Url = $candidate
            }
        }
    }
    if ($links.Count -eq 0) {
        Throw-FidoError 3 "Microsoft returned no ISO download links."
    }
    return $links
}

function Invoke-FidoLinuxMain
{
    if (!$GetUrl) {
        Throw-FidoError 1 "The Linux resolver requires -GetUrl."
    }
    if ([string]::IsNullOrWhiteSpace($Arch)) {
        if ([string]::IsNullOrWhiteSpace($PlatformArch)) {
            $script:PlatformArch = Get-Platform-Arch
        } else {
            $script:PlatformArch = $PlatformArch
        }
        $script:RequestedArch = $script:PlatformArch
    } else {
        $script:RequestedArch = $Arch
        $script:PlatformArch = $Arch
    }
    if (@("x64", "x86", "ARM64") -notcontains $script:RequestedArch) {
        Throw-FidoError 4 "The requested architecture is unsupported."
    }

    $versionIndex = $null
    for ($index = 0; $index -lt $script:WindowsVersions.Count; $index++) {
        if (Test-Selection $script:WindowsVersions[$index][0][0] $Win) {
            $versionIndex = $index
            break
        }
    }
    if ($null -eq $versionIndex) {
        Throw-FidoError 1 "The requested Windows version is unavailable."
    }

    $release = $null
    foreach ($candidate in @(Get-Windows-Releases $versionIndex)) {
        if ([string]::IsNullOrWhiteSpace($Rel) -or $Rel -eq "Latest" -or
            $candidate.Release.StartsWith(
                $Rel, [System.StringComparison]::OrdinalIgnoreCase)) {
            $release = $candidate
            break
        }
    }
    if ($null -eq $release) {
        Throw-FidoError 1 "The requested Windows release is unavailable."
    }

    $edition = $null
    foreach ($candidate in @(Get-Windows-Editions $versionIndex $release.Index)) {
        if (Test-Selection $candidate.Edition $Ed) {
            $edition = $candidate
            break
        }
    }
    if ($null -eq $edition) {
        Throw-FidoError 1 "The requested Windows edition is unavailable."
    }

    Write-FidoDiagnostic "selection" ("release=" + $release.Release)
    $language = $null
    foreach ($candidate in @(Get-Windows-Languages $versionIndex $edition.Id)) {
        if ((Test-Selection $candidate.Name $Lang) -or
            (Test-Selection $candidate.DisplayName $Lang)) {
            $language = $candidate
            break
        }
    }
    if ($null -eq $language) {
        Throw-FidoError 4 "The requested Windows language is unavailable."
    }

    $selected = $null
    foreach ($candidate in @(Get-Windows-Download-Links $language)) {
        if ($candidate.Arch -eq $script:RequestedArch) {
            $selected = $candidate
            break
        }
    }
    if ($null -eq $selected) {
        Throw-FidoError 4 "The requested Windows architecture is unavailable."
    }
    return [string]$selected.Url
}

if ($LibraryMode) {
    return
}

try {
    $resolvedUrl = Invoke-FidoLinuxMain
    [Console]::Out.WriteLine($resolvedUrl)
    exit 0
} catch {
    $exitCode = 2
    if ($_.Exception.Data.Contains("FidoExitCode")) {
        $exitCode = [int]$_.Exception.Data["FidoExitCode"]
    }
    [Console]::Error.WriteLine("fido-linux: " + $_.Exception.Message)
    exit $exitCode
}
