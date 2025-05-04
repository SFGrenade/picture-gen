#!/bin/bash

time="$(ffprobe -select_streams a:0 -show_entries "stream=duration" -of compact audio.wav | sed 's!.*=\(.*\)!\1!g')";

# tmp: -movflags +cgop -g 30 -keyint_min 30 \
ffmpeg -hide_banner -y -hwaccel cuda -hwaccel_output_format cuda \
    -loop 1 -i ../bg.old.png -i audio.wav -loop 1 -i ../circle.png -loop 1 -i art.png -shortest \
    -filter_complex "[0:v] copy [BG_IN]; [2:v] copy [CIRCLE_IN]; [3:v] copy [ART_IN]; [1:a] aformat=channel_layouts=mono, showwaves=size=917x387:rate=60:mode=p2p:draw=full:colors=#FF0000, format=pix_fmts=rgb24, dilation [waves]; [1:a] showfreqs=size=917x387:rate=60:fscale=log:win_size=4096:colors=#400000|#600000, format=pix_fmts=rgb24 [freqs]; [BG_IN][freqs] overlay=format=auto:alpha=premultiplied:eval=init:x=979:y=24, drawtext=fontfile=C\\:/Windows/Fonts/NotoSans-Regular.ttf:fontsize=60:textfile=title.txt:fix_bounds=true:fontcolor=#FFFFFF:x=979+(917/2)-(text_w/2):y=24+(387/2)-(text_h/2) [bg+freqs+title]; [bg+freqs+title][waves] overlay=format=auto:alpha=premultiplied:eval=init:x=979:y=449 [bg+title+waves]; [bg+title+waves][CIRCLE_IN] overlay=format=auto:alpha=premultiplied:eval=frame:y=874+((182-overlay_h)/2):x=24+(t*((1872-overlay_w)/$time)) [bg+title+waves+circle]; [bg+title+waves+circle][ART_IN] overlay=format=auto:alpha=premultiplied:eval=init:x=24:y=24 [bg+title+waves+circle+art]; [bg+title+waves+circle+art] yadif, format=pix_fmts=yuv420p:color_spaces=bt709 [VIDEO_OUT]" \
    -c:a aac -ac 2 -ar 48000 \
        -aac_coder fast -b:a 512K \
    -c:v libx264 \
        -profile:v high \
        -bf 2 \
        -g 30 -keyint_min 30 \
        -coder ac \
        -crf 1 -b:v 20M \
    -use_editlist 0 -movflags +faststart \
    -r 60 -framerate 60 \
    -strict very \
    -map 1:a \
    -map "[VIDEO_OUT]" \
youtube.mp4
