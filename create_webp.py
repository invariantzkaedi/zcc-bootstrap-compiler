import os
from PIL import Image

def create_animation():
    frame_dir = "frames"
    frames = []
    
    # Check if files exist and load them
    for i in range(30):
        filename = os.path.join(frame_dir, f"frame_{i:02d}.ppm")
        if os.path.exists(filename) and os.path.getsize(filename) > 0:
            print(f"Loading {filename}...")
            img = Image.open(filename)
            frames.append(img.copy())
            img.close()
    
    if len(frames) > 0:
        print(f"Creating animated WEBP with {len(frames)} frames...")
        frames[0].save('cinematic_fleet.webp', format='WebP', append_images=frames[1:], save_all=True, duration=66, loop=0)
        print("Success: saved to cinematic_fleet.webp")
    else:
        print("No valid frames found.")

if __name__ == "__main__":
    create_animation()
