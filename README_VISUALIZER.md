# ZKAEDI Cyberpunk Neon Creature Visualizer

An audio-reactive visualizer that uses the native HTML5 Web Audio API to analyze frequency spectrums of local MP3 tracks and drive a vector-art animation rig in real-time.

## Features
- **Zero Dependencies**: Pure HTML, Vanilla CSS, and native JavaScript. No external libraries, node packages, CDNs, or internet access required.
- **Dynamic Spectral Analysis**: Maps real-time audio frequencies to distinct mechanical and biological bones of the creature:
  - **Sub Bass & Bass**: Torso squash-and-stretch, pupil dilation, and radial background glow bloom.
  - **Low Mids**: Head nod and bob.
  - **Mids & High Mids**: Mirror sways of left/right ears, sinusoidal tail wag.
  - **Treble**: Dynamic eye sparkles, pupil highlight pulsations, and occasional blinking.
- **Bass energy flux beat detector**: An onset tracker matching rolling averages to identify strong beats, firing outward ear-flicks and scaling bounces.
- **Sleek HUD Controller Panel**: Customize Volume, Sensitivity, Motion Scale, and Glow Blur. Toggle between dynamic particle emissions, rig anchor overlays, and live telemetry statistics.

## Project Structure
- `audio_reactive_creature.html`: The main dashboard page containing the embedded runtime SVG vector artwork.
- `audio_reactive_creature.js`: Main audio analysis pipeline and transformation animation runtime.
- `neon_creature_runtime.svg`: Standalone vector cyberpunk creature asset with normalized rig group IDs.
- `neon_creature_manifest.json`: Bone-mapping and audio frequency bounds configuration.

## How to Run
1. Open `audio_reactive_creature.html` directly in any modern web browser (e.g. Google Chrome, Microsoft Edge, Brave, Firefox) by double-clicking it or dragging it into a browser tab.
2. Drag and drop any `.mp3` file into the upload zone, or click **Choose Local File** to select a song.
3. Click the **Play** button on the interface.
4. (Optional) Turn on **Particles** or **Show Anchors** to see the particle emitter and rotation pivots.
