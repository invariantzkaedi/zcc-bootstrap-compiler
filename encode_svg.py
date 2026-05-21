import urllib.parse

svg_path = "C:/Users/zkaed/.gemini/antigravity/brain/7ac2ee0b-f4ce-4d33-852a-f25a6c2797ff/browser/proof_of_computation.svg"
out_path = "C:/Users/zkaed/.gemini/antigravity/brain/7ac2ee0b-f4ce-4d33-852a-f25a6c2797ff/browser/encoded_svg_url.txt"

with open(svg_path, "r", encoding="utf-8") as f:
    svg_content = f.read()

# Replace double quotes with single quotes for HTML compatibility in URLs
svg_single = svg_content.replace('"', "'")

# Wrap in clean HTML structure
html_content = f"<!DOCTYPE html><html><body style='margin:0;background:#03010a;overflow:hidden;'>{svg_single}</body></html>"

# URL encode the HTML
encoded_url = "data:text/html," + urllib.parse.quote(html_content)

with open(out_path, "w", encoding="utf-8") as f:
    f.write(encoded_url)

print("URL encoded SVG written to encoded_svg_url.txt successfully!")
