#!/bin/bash
# Convert PPM files to PNG for viewing
cd /mnt/g/zccMAIN/zcc
OUTDIR="/mnt/c/Users/zkaed/.gemini/antigravity-ide/brain/1dd7c79a-0678-4b0b-8702-176cf0b019cb/scratch"
mkdir -p "$OUTDIR"

# Convert key PPMs to PNG
for f in exp1_output.ppm exp2_output.ppm exp3_output.ppm exp4_output.ppm exp5_output.ppm out_zcc.ppm unknowns.ppm; do
    if [ -f "$f" ]; then
        out="$OUTDIR/${f%.ppm}.png"
        python3 -c "
from PIL import Image
im = Image.open('$f')
im.save('$out')
print('OK: $f -> ${f%.ppm}.png (%dx%d)' % (im.width, im.height))
"
    fi
done

# Copy key SVGs
for f in test_raymarch.svg test_dos.svg test_attractor.svg test_anim.svg test_loading_bars.svg proof_of_computation.svg omni_node_v3.svg test_evm_topology.svg zcc_sprites.svg the_omni_node_anthem.svg matrix_battle_simulator.svg attractor.svg; do
    if [ -f "$f" ]; then
        cp "$f" "$OUTDIR/$f"
        echo "COPIED: $f"
    fi
done

echo "DONE"
