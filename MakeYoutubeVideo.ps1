#ffmpeg -hide_banner -r 60 -i lossless.mp4 -vf yadif,format=yuv422p `
#-force_key_frames "expr:gte(t,n_forced/2)" `
#-c:v libx264 -b:v 60M -bf 2 -c:a flac -ac 2 -ar 44100 `
#-use_editlist 0 -movflags +faststart -framerate 60 -strict -2 youtube.mp4
#
# youtube recommendations:
# container mp4, no edit lists, fast start => `... -use_editlist 0 -movflags +faststart ... out.mp4`
# audio aac, stereo, 48khz => `... -c:a aac -ac 2 -ar 48000 ...`
# h.264 codec => `... -c:v libx264 ...`
#     progressive scan => `... the yadif thing ...`
#     high profile => `... -profile high ...`
#     2 consecutive b frames => `... -bf 2 ...`
#     closed gop, gop of half framerate => `... -movflags +cgop -g 30 -keyint_min 30 ...` (adjust numbers to half frame rate)
#     cabac => `... -coder ac ...`
#     variable bitrate, no limit required => `... -crf 0 -b:b 20M ...` (who knows if crf 0 works that way)
#     chroma subsample 4:2:0 => `... filter: "... format=pix_fmts=yuv420p" ...`
# frame rate 60 => `... -r 60 -framerate 60 ...`
# color space bt709 => `... fitler: "... format=color_spaces=bt709" ...`
# testing:
# try very strict => `... -strict very ...`

$time = $(ffprobe -select_streams a:0 -show_entries "stream=duration" -of compact audio.wav 2>$null) -replace '^[^=]+=(.*)$','$1';
$fps = 60
$half_fps = 30

# orig:
#ffmpeg -hide_banner -y -hwaccel cuda -hwaccel_output_format cuda `
#-loop 1 -i bg.old.png -i audio.wav -loop 1 -i circle.png -loop 1 -i art.png `
#-filter_complex "[1:a]aformat=channel_layouts=mono,showwaves=size=909x375:mode=p2p:colors=#FF0000FF,dilation=threshold0=65535:threshold1=65535:threshold2=65535:threshold3=65535[waves];[0:v]drawtext=fontfile=C\\:/Windows/Fonts/NotoSans-Regular.ttf:fontsize=60:textfile=title.txt:fix_bounds=true:fontcolor=#FFFFFFFF:x=983+(909/2)-(text_w/2):y=28+(379/2)-(text_h/2)[title];[title][waves]overlay=eval=init:x=983:y=453[titleAfter];[titleAfter][2:v]overlay=y=878:x=28+(t*((1864-174)/$time))[titleWithKnob];[titleWithKnob][3:v]overlay=eval=init:x=28:y=28[last];[last]yadif,format=yuv422p" `
#-shortest -force_key_frames "expr:gte(t,n_forced/2)" -c:v libx264 -b:v 60M -bf 2 -c:a flac -ac 2 -ar 44100 `
#-use_editlist 0 -movflags +faststart -r 60 -framerate 60 -strict -2 youtube.mp4

# new:
# tmp: -movflags +cgop -g 30 -keyint_min 30 `
ffmpeg -hide_banner -y -hwaccel cuda -hwaccel_output_format cuda `
    -loop 1 -r $fps -i ..\bg.old.png -r $fps -i audio.wav -loop 1 -r $fps -i ..\circle.png -loop 1 -r $fps -i art.png -shortest `
    -filter_complex "[0:v] copy [BG_IN]; [2:v] copy [CIRCLE_IN]; [3:v] copy [ART_IN]; [1:a] aformat=channel_layouts=mono, showwaves=size=917x387:rate=60:mode=p2p:draw=full:colors=#FF0000, format=pix_fmts=rgb24, dilation [waves]; [1:a] showfreqs=size=917x387:rate=60:fscale=log:win_size=4096:colors=#400000|#600000, format=pix_fmts=rgb24 [freqs]; [BG_IN][freqs] overlay=format=auto:alpha=premultiplied:eval=init:x=979:y=24, drawtext=fontfile=C\\:/Windows/Fonts/NotoSans-Regular.ttf:fontsize=60:textfile=title.txt:fix_bounds=true:fontcolor=#FFFFFF:x=979+(917/2)-(text_w/2):y=24+(387/2)-(text_h/2) [bg+freqs+title]; [bg+freqs+title][waves] overlay=format=auto:alpha=premultiplied:eval=init:x=979:y=449 [bg+title+waves]; [bg+title+waves][CIRCLE_IN] overlay=format=auto:alpha=premultiplied:eval=frame:y=874+((182-overlay_h)/2):x=24+(t*((1872-overlay_w)/$time)) [bg+title+waves+circle]; [bg+title+waves+circle][ART_IN] overlay=format=auto:alpha=premultiplied:eval=init:x=24:y=24 [bg+title+waves+circle+art]; [bg+title+waves+circle+art] yadif, format=pix_fmts=yuv420p:color_spaces=bt709 [VIDEO_OUT]" `
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
youtube.mp4
