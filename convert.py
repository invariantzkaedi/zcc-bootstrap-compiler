import sys
from PIL import Image

def main():
    try:
        img = Image.open('/mnt/h/__DOWNLOADS/zcc_github_upload/out.ppm')
        img.save('/mnt/h/__DOWNLOADS/zcc_github_upload/out.webp', 'WEBP')
        print('Converted to WEBP successfully')
    except Exception as e:
        print('Conversion failed:', e)

if __name__ == '__main__':
    main()
