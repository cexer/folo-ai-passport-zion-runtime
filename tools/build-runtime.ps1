param([string]$BuildDirectory='',[switch]$Simulator,[switch]$DeviceTest)
$ErrorActionPreference='Stop'
if(-not $BuildDirectory) {$BuildDirectory=if($Simulator){'build-runtime-sim'}elseif($DeviceTest){'build-runtime-device'}else{'build-runtime-release'}}
if(-not $Simulator -and -not $DeviceTest) {
    & "$PSScriptRoot/build-with-xiaozhi.ps1" -BuildDirectory $BuildDirectory
    return
}
$root=Split-Path $PSScriptRoot -Parent
if(-not $env:IDF_PATH -or $env:ESP_IDF_VERSION -ne '6.0.1') {throw 'Activate ESP-IDF 6.0.1 first.'}
$python=Join-Path $env:IDF_PYTHON_ENV_PATH 'Scripts/python.exe'
$idf=Join-Path $env:IDF_PATH 'tools/idf.py'
$previous=$env:SDKCONFIG_DEFAULTS
function Invoke-NativeChecked([string]$File,[string[]]$Arguments) {
    $saved=$ErrorActionPreference
    try {$ErrorActionPreference='Continue'; & $File @Arguments; $code=$LASTEXITCODE}
    finally {$ErrorActionPreference=$saved}
    if($code -ne 0) {throw "Native command failed with exit code $code"}
}
Push-Location $root
try {
    $env:SDKCONFIG_DEFAULTS='sdkconfig.defaults;sdkconfig.with-xiaozhi.defaults'
    $simMode=if($Simulator){'ON'}else{'OFF'}
    $testMode=if($DeviceTest){'ON'}else{'OFF'}
    Invoke-NativeChecked $python @($idf,'-B',$BuildDirectory,'-D',"SDKCONFIG=$BuildDirectory/sdkconfig",'-D',"ZION_RUNTIME_SIM=$simMode",'-D',"ZION_RUNTIME_DEVICE_TEST=$testMode",'build')
    $appHeader=Get-Content -Raw "$BuildDirectory/config/sdkconfig.h"
    $factoryBytes=[regex]::Match($appHeader,'(?m)^#define CONFIG_ZION_FACTORY_SIZE (\d+)').Groups[1].Value
    if(-not $factoryBytes) {throw 'Missing APP factory geometry'}
    $bootHeader=Get-Content -Raw "$BuildDirectory/bootloader/config/sdkconfig.h"
    if($bootHeader -notmatch "(?m)^#define CONFIG_ZION_FACTORY_SIZE $factoryBytes`r?`$") {
        Invoke-NativeChecked 'cmake' @('-S',"$env:IDF_PATH/components/bootloader/subproject",'-B',"$BuildDirectory/bootloader")
        Invoke-NativeChecked 'cmake' @('--build',"$BuildDirectory/bootloader")
        $bootHeader=Get-Content -Raw "$BuildDirectory/bootloader/config/sdkconfig.h"
        if($bootHeader -notmatch "(?m)^#define CONFIG_ZION_FACTORY_SIZE $factoryBytes`r?`$") {throw 'Bootloader factory geometry differs from APP'}
    }
    $imageName=if($Simulator){'runtime-sim-full.bin'}else{'runtime-device-test-full.bin'}
    $image=Join-Path $root "$BuildDirectory/$imageName"
    Invoke-NativeChecked $python @($idf,'-B',$BuildDirectory,'merge-bin','-o',$image)
} finally {
    Pop-Location
    $env:SDKCONFIG_DEFAULTS=$previous
}
