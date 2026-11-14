# Grava o firmware na BlackPill sem apertar BOOT0 (quando o Pac-Tilt ja esta gravado).
# Uso: powershell -ExecutionPolicy Bypass -File tools\upload.ps1
#
# 1) Procura a porta serial do Pac-Tilt (USB VID 0xCAFE) e abre a 1200 baud:
#    o firmware entende isso como pedido para reiniciar no bootloader DFU.
# 2) Roda "pio run -t upload" (dfu-util).
# Se a placa ainda nao tem o Pac-Tilt, entre no DFU manualmente (ver README).

$pio = "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe"
$root = Split-Path -Parent $PSScriptRoot

$port = Get-CimInstance Win32_PnPEntity |
    Where-Object { $_.PNPDeviceID -like 'USB\VID_CAFE&PID_4111*' -and $_.Name -match '\((COM\d+)\)' } |
    ForEach-Object { $Matches[1] } | Select-Object -First 1

if ($port) {
    Write-Host "Pedindo reinicio em modo DFU pela $port..."
    try {
        $sp = New-Object System.IO.Ports.SerialPort $port, 1200
        $sp.Open(); Start-Sleep -Milliseconds 100; $sp.Close()
    } catch { Write-Host "Aviso: $($_.Exception.Message)" }
    Start-Sleep -Seconds 2
} else {
    Write-Host "Porta do Pac-Tilt nao encontrada; supondo que a placa ja esta em DFU."
}

Push-Location $root
& $pio run -t upload
Pop-Location
