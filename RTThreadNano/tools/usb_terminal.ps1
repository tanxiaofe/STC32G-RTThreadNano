param([string]$Port)
$ErrorActionPreference='Stop'
if(!$Port){
 $matches=@(Get-CimInstance Win32_SerialPort | Where-Object {$_.PNPDeviceID -match 'VID_34BF&PID_FF02'})
 if($matches.Count -ne 1){[System.IO.Ports.SerialPort]::GetPortNames() | Write-Host;throw 'Specify -Port COMx.'}
 $Port=$matches[0].DeviceID
}
$serial=[System.IO.Ports.SerialPort]::new($Port,115200,[System.IO.Ports.Parity]::None,8,[System.IO.Ports.StopBits]::One)
$serial.DtrEnable=$true;$serial.RtsEnable=$true;$serial.ReadTimeout=100;$serial.WriteTimeout=2000
function Read-Reply {
 $reply='';$clock=[System.Diagnostics.Stopwatch]::StartNew();$lastData=0
 do {
  $chunk=$serial.ReadExisting()
  if($chunk){$reply+=$chunk;$lastData=$clock.ElapsedMilliseconds}
  if($reply -and $clock.ElapsedMilliseconds-$lastData -gt 150){break}
  Start-Sleep -Milliseconds 20
 } while($clock.ElapsedMilliseconds -lt 2500)
 return $reply
}
try {
 $serial.Open();$serial.Write([string][char]3)
 Write-Host (Read-Reply) -NoNewline
 Write-Host "USB CDC on $Port. Type help; type exit to close this PC terminal."
 while($true){
  $line=Read-Host 'command'
  if($line -eq 'exit'){break}
  $serial.Write($line+"`r`n")
  Write-Host (Read-Reply) -NoNewline
 }
} finally {if($serial.IsOpen){$serial.Close()};$serial.Dispose()}
