param([string]$Python='python')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
& (Join-Path $PSScriptRoot 'test-host.cmd')
if($LASTEXITCODE){throw 'Native kernel/FinSH/CDC tests failed.'}
& $Python (Join-Path $root 'tests\context_machine.py')
if($LASTEXITCODE){throw 'Linked opcode tests failed.'}
& $Python (Join-Path $root 'tests\usb_descriptors.py')
if($LASTEXITCODE){throw 'USB descriptor tests failed.'}
