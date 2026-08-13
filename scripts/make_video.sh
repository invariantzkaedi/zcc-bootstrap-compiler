#!/bin/bash
# Convert exported PNG frames to MP4 video using ffmpeg
ffmpeg -y -framerate 12 -i artifacts/frames/frame_%04d.png \
  -c:v libx264 -pix_fmt yuv420p artifacts/living_tensor_heatmap.mp4
echo "[+] Video compilation finished: artifacts/living_tensor_heatmap.mp4"
