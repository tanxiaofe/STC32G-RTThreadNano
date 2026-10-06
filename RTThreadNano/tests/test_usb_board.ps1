param([Parameter(Mandatory=$true)][string]$Port)
# Run only after flashing this project's firmware. These queries do not modify the demo.
$ErrorActionPreference='Stop'
$serial=[System.IO.Ports.SerialPort]::new($Port,115200,[System.IO.Ports.Parity]::None,8,[System.IO.Ports.StopBits]::One)
$serial.DtrEnable=$true;$serial.RtsEnable=$true;$serial.ReadTimeout=100;$serial.WriteTimeout=2000
function Query([string]$command,[string]$expected){
 $serial.DiscardInBuffer();$serial.Write($command+"`r`n")
 $result='';$watch=[System.Diagnostics.Stopwatch]::StartNew()
 while($watch.ElapsedMilliseconds -lt 3000){
  $result+=$serial.ReadExisting()
  if($result -match $expected -and $result -match 'msh >'){Write-Host "PASS $command";return $result}
  Start-Sleep -Milliseconds 20
 }
 throw "$command timed out or returned unexpected text: $result"
}
try {
 $serial.Open();Start-Sleep -Milliseconds 300;$serial.Write([string][char]3);Start-Sleep -Milliseconds 100
 Query 'help' 'List commands' | Out-Null
 Query 'version' 'STC32G12K128' | Out-Null
 $threads=Query 'ps' 'stack used/size'
 foreach($name in @('beat','key','timer','lcd','tidle0','tshell')){if($threads -notmatch $name){throw "Missing $name thread"}}
 $first=Query 'tick' 'tick=(\d+)';$a=[long]([regex]::Match($first,'tick=(\d+)').Groups[1].Value)
 Start-Sleep -Milliseconds 200
 $second=Query 'tick' 'tick=(\d+)';$b=[long]([regex]::Match($second,'tick=(\d+)').Groups[1].Value)
 if($b -le $a){throw 'RTOS tick did not advance.'}
 Query 'ipc' 'mq sent=\d+ recv=\d+' | Out-Null
 Query 'mem' 'XDATA heap total=\d+ used=\d+' | Out-Null
 Query 'stat' 'beat=\d+ keys=\d+ lcd=\d+' | Out-Null
 Query 'echo USB_CDC_OK' 'USB_CDC_OK\r?\nmsh >' | Out-Null
 Write-Host 'Board smoke test passed. Also exercise keys and unplug/replug the USB cable.'
} finally {if($serial.IsOpen){$serial.Close()};$serial.Dispose()}
