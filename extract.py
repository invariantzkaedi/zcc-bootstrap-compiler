import sys

html_path = 'H:/html/3d-dom-security-analyzer-html (3).html'
js_path = 'H:/html/security_analyzer_core.js'

with open(html_path, 'r', encoding='utf-8') as f:
    lines = f.readlines()

start_idx = -1
end_idx = -1

for i, line in enumerate(lines):
    if '<script>' in line and start_idx == -1 and i > 900:
        start_idx = i
    if '</script>' in line and start_idx != -1 and i > start_idx + 3000:
        end_idx = i
        break

if start_idx != -1 and end_idx != -1:
    js_lines = lines[start_idx+1:end_idx]
    with open(js_path, 'w', encoding='utf-8') as f:
        f.writelines(js_lines)
    
    new_html = lines[:start_idx] + ['    <script src="security_analyzer_core.js"></script>\n'] + lines[end_idx+1:]
    with open(html_path, 'w', encoding='utf-8') as f:
        f.writelines(new_html)
    print(f'Success: Extracted {len(js_lines)} lines of JavaScript to {js_path}')
    print(f'Original HTML file updated. New size: {len(new_html)} lines.')
else:
    print('Error: Could not find script boundaries.')
