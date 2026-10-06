param([string]$Keil='D:\Keil_v5\UV4\UV4.exe')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
if (!(Test-Path -LiteralPath $Keil)) {throw 'Specify your Keil UV4 path with -Keil.'}
$project=Join-Path $projectRoot 'RTThreadNano.uvproj'
$log=Join-Path $projectRoot 'build\build.log'
if(Test-Path -LiteralPath $log){Remove-Item -LiteralPath $log}
$process=Start-Process -FilePath $Keil -ArgumentList @('-r',('"'+$project+'"'),'-o',('"'+$log+'"')) -WindowStyle Hidden -Wait -PassThru
$content=Get-Content -LiteralPath $log -Raw
if ($content -notmatch '0 Error\(s\)' -or $content -match 'Target not created|error C|ERROR #') {Write-Output $content;throw 'Keil build failed.'}
Copy-Item -LiteralPath (Join-Path $projectRoot 'build\RTThreadNano.hex') -Destination (Join-Path $projectRoot 'release\RTThreadNano-CDC-FinSH.hex')
Copy-Item -LiteralPath (Join-Path $projectRoot 'build\RTThreadNano.map') -Destination (Join-Path $projectRoot 'release\RTThreadNano.map')
Write-Output ($content -split "`n" | Select-String 'Program Size|Error\(s\)')
