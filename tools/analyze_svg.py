import re

svg_file = r'g:\zccMAIN\zcc\YETIIII.txt'

try:
    with open(svg_file, 'r', encoding='utf-8') as f:
        data = f.read()

    paths = re.findall(r'<path d="([^"]+)"', data)

    if paths:
        path_data = paths[6] if len(paths) > 6 else paths[0]
        
        m_count = path_data.lower().count('m')
        l_count = path_data.lower().count('l')
        q_count = path_data.lower().count('q')
        c_count = path_data.lower().count('c')
        z_count = path_data.lower().count('z')

        print('SVG Path Complexity Analysis:')
        print(f'- MoveTo (M): {m_count}')
        print(f'- LineTo (L): {l_count}')
        print(f'- Quadratic Bezier (Q): {q_count}')
        print(f'- Cubic Bezier (C): {c_count}')
        print(f'- ClosePath (Z): {z_count}')
        print(f'Total vertices approximated: {(m_count + l_count + q_count*2 + c_count*3)}')
        
    else:
        print('No paths found.')

except Exception as e:
    print(f'Error: {e}')
