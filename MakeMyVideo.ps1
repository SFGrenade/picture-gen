$fps = 60
$half_fps = 30

function Log-Message
{
    [CmdletBinding()]
    Param
    (
        [Parameter(Mandatory=$true, Position=0)]
        [string]$LogMessage
    )

    Write-Output("{0} - {1}" -f (Get-Date), $LogMessage) >>../__internal_timing.log
}
Write-Output("") >../__internal_timing.log

Log-Message("Getting duration and bitrate of audio.wav...")

$time = $(ffprobe -select_streams a:0 -show_entries "stream=duration" -of compact audio.wav 2>$null) -replace '^[^=]+=(.*)$','$1';
$bitrate = $(ffprobe -select_streams a:0 -show_entries "stream=bit_rate" -of compact audio.wav 2>$null) -replace '^[^=]+=(.*)$','$1';

Log-Message("Got duration and bitrate of audio.wav: $time s, $bitrate b/s")

Log-Message("Calling xmake program...")

..\__src\_build.bat

Log-Message("Called xmake program")

Log-Message("Rendering final video...")

ffmpeg -hide_banner -y -hwaccel cuda -hwaccel_output_format cuda `
    -r $fps -i __pictures\%d.png -max_size "$bitrate / 8 / $fps" -r $fps -i audio.wav -shortest `
    -filter_complex "yadif, format=pix_fmts=yuv420p : color_spaces=bt709 [VIDEO_OUT]" `
    -c:a aac -ac 2 -ar 48000 `
        -aac_coder fast -b:a 512K `
    -c:v libx264 `
        -profile:v high `
        -bf 2 `
        -g $half_fps -keyint_min $half_fps `
        -coder ac `
        -crf 1 -b:v 20M `
    -use_editlist 0 -movflags +faststart `
    -r $fps -framerate $fps `
    -strict very `
    -map "1:a" `
    -map "[VIDEO_OUT]" `
    -map_metadata -1 -map_chapters -1 `
z_custom.mp4 #>_video_rendering.log 2>&1

Log-Message("Rendered final video")
