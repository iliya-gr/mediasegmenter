# Mediasegmenter

An attempt to create replacement for mediafilesegmenter and mediastreamsegmenter for Linux operation system.

## Usage

### Video on demand

To create VOD playlist with default parameters:

```bash
mediasegmenter -f /output_path source.mp4
```

### Live

Create html page with video tag:

```html
<video src="path_to_video_directory/prog_index.m3u8" controls="controls" autoplay="autoplay"></video>
```

```bash
mkfifo stream
ffmpeg -re -y -i big_buck_bunny_1080p_h264.mov -c copy -bsf h264_mp4toannexb -f mpegts stream
mediasegmenter -f /var/www/path_to_video_directory --live -w 5 --delete-files stream 
```


