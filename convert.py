import os
from PIL import Image

_dir = os.path.dirname(os.path.abspath(__file__))

def main():
    try:
        img = Image.open(os.path.join(_dir, 'out.ppm'))
        img.save(os.path.join(_dir, 'out.webp'), 'WEBP')
        print('Converted to WEBP successfully')
    except Exception as e:
        print('Conversion failed:', e)

if __name__ == '__main__':
    main()
