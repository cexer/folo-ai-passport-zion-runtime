param([string]$BuildDirectory='build-final')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(-not $env:IDF_PATH -or -not $env:IDF_PYTHON_ENV_PATH) {
    throw 'Activate ESP-IDF 6.0.1 before building this experimental fork.'
}
$python=Join-Path $env:IDF_PYTHON_ENV_PATH 'Scripts/python.exe'
$idf=Join-Path $env:IDF_PATH 'tools/idf.py'
$version=& $python $idf --version
if($version -notmatch '6\.0\.1') {throw "Expected ESP-IDF 6.0.1; found $version"}
$previousDefaults=$env:SDKCONFIG_DEFAULTS
function Invoke-Checked([string]$File,[string[]]$Arguments) {
    $savedPreference=$ErrorActionPreference
    try {
        $ErrorActionPreference='Continue'
        & $File @Arguments
        $code=$LASTEXITCODE
    } finally {$ErrorActionPreference=$savedPreference}
    if($code -ne 0) {throw "Native command failed with exit code $code"}
}
Push-Location $root
try {
    $env:SDKCONFIG_DEFAULTS='sdkconfig.defaults;sdkconfig.with-xiaozhi.defaults'
    $merged=Join-Path $root "$BuildDirectory/ZION-Runtime-full.bin"
    Invoke-Checked $python @($idf,'-B',$BuildDirectory,'-D',"SDKCONFIG=$BuildDirectory/sdkconfig",'-D','ZION_RUNTIME_SIM=OFF','-D','ZION_RUNTIME_DEVICE_TEST=OFF','build')
    $mainHeader=Get-Content -Raw "$BuildDirectory/config/sdkconfig.h"
    $bootHeader=Get-Content -Raw "$BuildDirectory/bootloader/config/sdkconfig.h"
    $expected=[regex]::Match($mainHeader,'(?m)^#define CONFIG_ZION_FACTORY_SIZE (\d+)').Groups[1].Value
    if(-not $expected) {throw 'Missing APP factory geometry'}
    if($bootHeader -notmatch "(?m)^#define CONFIG_ZION_FACTORY_SIZE $expected`r?`$") {
        # ESP-IDF's ExternalProject can retain a bootloader configured against
        # an older Kconfig range. Reconfigure that subproject explicitly.
        Invoke-Checked 'cmake' @('-S',"$env:IDF_PATH/components/bootloader/subproject",'-B',"$BuildDirectory/bootloader")
        Invoke-Checked 'cmake' @('--build',"$BuildDirectory/bootloader")
    }
    Invoke-Checked $python @($idf,'-B',$BuildDirectory,'merge-bin','-o',$merged)
    Invoke-Checked $python @('tools/package_minimal.py','--profile','zion-runtime','--build',$BuildDirectory,'--merged',$merged,'--output','release')
} finally {
    Pop-Location
    $env:SDKCONFIG_DEFAULTS=$previousDefaults
}
