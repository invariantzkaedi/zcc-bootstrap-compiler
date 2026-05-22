#!/usr/bin/env bash
# ==============================================================================
# compile_ppm.sh - Automated High-Fidelity Ray Tracer & Image Compiler Pipeline
# ==============================================================================
set -e

# Configuration
FRAME="${FRAME:-100}"
WIDTH="${WIDTH:-1280}"
HEIGHT="${HEIGHT:-720}"
CC="gcc"
CFLAGS="-O3 -pthread"
LDFLAGS="-lm"

echo "🔱 [1/3] Compiling Ray Tracer Engine..."
$CC $CFLAGS raytracer.c models_data.c -o raytracer $LDFLAGS
echo "  ↳ Ray Tracer compiled successfully!"

echo "🔱 [2/3] Rendering Universe Frame ($FRAME) at ${WIDTH}x${HEIGHT} (16x SSAA)..."
FRAME=$FRAME WIDTH=$WIDTH HEIGHT=$HEIGHT ./raytracer > out.ppm
echo "  ↳ Frame rendered successfully to out.ppm!"

echo "🔱 [3/3] Compiling PPM image to compressed WebP..."
python3 -c "from PIL import Image; Image.open('out.ppm').save('out.webp', 'WEBP')"
echo "  ↳ Compilation complete! Output saved to out.webp"
echo "=============================================================================="
ls -lh out.ppm out.webp
