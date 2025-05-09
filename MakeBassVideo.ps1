$fps = 60
$half_fps = 30

$time = $(ffprobe -select_streams a:0 -show_entries "stream=duration" -of compact audio.wav 2>$null) -replace '^[^=]+=(.*)$','$1';
$bitrate = $(ffprobe -select_streams a:0 -show_entries "stream=bit_rate" -of compact audio.wav 2>$null) -replace '^[^=]+=(.*)$','$1';

ffmpeg -hide_banner -y -hwaccel cuda -hwaccel_output_format cuda `
    -r $fps -max_size "$bitrate / 8 / $fps" -i audio.wav -shortest `
    -filter_complex "[0:a] lowpass=frequency=80 : poles=2 : width_type=q : width=0.707 : precision=auto, astats=length=1/$fps : metadata=1 : reset=1 : measure_perchannel=none : measure_overall=Peak_level, ametadata=print : key=lavfi.astats.Overall.Peak_level : file='../_internal_levels.log' ;" `
    -f null `
- #>_audio_analysis.log 2>&1

python ../RmsLogParser.py #>../_python_script.log 2>&1

ffmpeg -hide_banner -y -hwaccel cuda -hwaccel_output_format cuda `
    -loop 1 -r $fps -i ..\bg.old.png -r $fps -max_size "$bitrate / 8 / $fps" -i audio.wav -loop 1 -r $fps -i ..\circle.png -loop 1 -r $fps -i art.png -shortest `
    -filter_complex "[0:v] format=rgba, scale=flags=16388 : width=1920 : height=1080 [BG_IN]; [2:v] format=rgba, sendcmd=filename='../_internal_ball_scale.log', scale@circle=flags=16388 : width=in_w : height=in_h [CIRCLE_IN]; [3:v] format=rgba, scale=flags=16388 : width=917 : height=812 [ART_IN]; [1:a] showwaves=size=917x387 : rate=$fps : mode=p2p : draw=full : colors=#FF0000FF|#AA0000FF, format=rgba, dilation, scale=flags=16388 : width=917 : height=387 [waves]; [1:a] showfreqs=size=917x387 : rate=$fps : fscale=log : win_size=4096 : win_func=flattop : colors=#600000FF|#400000FF, format=rgba, scale=flags=16388 : width=917 : height=387 [freqs]; [BG_IN][freqs] overlay=format=auto : alpha=straight : eval=init : x=979 : y=24, drawtext=fontfile=C\\:/Windows/Fonts/NotoSans-Regular.ttf : fontsize=60 : textfile=title.txt : fix_bounds=true : fontcolor=#FFFFFFFF : x=1437-(text_w/2) : y=217-(text_h/2) [bg+freqs+title]; [bg+freqs+title][waves] overlay=format=auto : alpha=straight : eval=init : x=979 : y=449 [bg+title+waves]; [bg+title+waves][ART_IN] overlay=format=auto : alpha=straight : eval=init : x=24 : y=24 [bg+title+waves+art]; [bg+title+waves+art][CIRCLE_IN] overlay=format=auto : alpha=straight : eval=frame : y=964.5-(overlay_h/2) : x=114.5+((1804.5-114.5)*(t/$time))-(overlay_w/2) [ready-for-output]; [ready-for-output] yadif, format=pix_fmts=yuv420p : color_spaces=bt709 [VIDEO_OUT]" `
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
z_bass.mp4 #>_video_rendering.log 2>&1
