import re

svg_template = """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1600 1600">
<style>
    path {
        fill: rgba(0, 255, 255, 0.7);
        stroke: #ff00ff;
        stroke-width: 5px;
        filter: drop-shadow(0px 10px 10px rgba(0, 255, 255, 0.5));
    }
    .extruded_1 {
        fill: #050b14;
        stroke: #00ffff;
        stroke-width: 2px;
        transform: translate(5px, 5px);
    }
    .extruded_2 {
        fill: #050b14;
        stroke: #ff00ff;
        stroke-width: 2px;
        transform: translate(10px, 10px);
    }
    .extruded_3 {
        fill: #050b14;
        stroke: #00ffff;
        stroke-width: 2px;
        transform: translate(15px, 15px);
    }
    .extruded_4 {
        fill: #050b14;
        stroke: #ff00ff;
        stroke-width: 2px;
        transform: translate(20px, 20px);
    }
</style>
{paths}
</svg>"""

with open(r'g:\zccMAIN\zcc\YETIIII.txt', 'r', encoding='utf-8') as f:
    data = f.read()

# Extract paths
paths = re.findall(r'<path d="([^"]+)"', data)

output = ""
if len(paths) > 6:
    # Use the cog (index 6) as an example to pseudo-extrude
    p1 = paths[6]
    
    # Generate the 3D depth by layering the path with progressive translation
    output += f'<path class="extruded_4" d="{p1}" />\n'
    output += f'<path class="extruded_3" d="{p1}" />\n'
    output += f'<path class="extruded_2" d="{p1}" />\n'
    output += f'<path class="extruded_1" d="{p1}" />\n'
    
    # Foreground face
    output += f'<path d="{p1}" />\n'

with open(r'g:\zccMAIN\zcc\extruded_symbols.svg', 'w', encoding='utf-8') as f:
    f.write(svg_template.replace('{paths}', output))
print('Generated extruded_symbols.svg')
