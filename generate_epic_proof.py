import math
import os

def generate_svg(filename="/mnt/g/zccMAIN/zcc/proof_of_computation.svg"):
    width, height = 1920, 1080
    cx, cy = width // 2, height // 2

    # SVG Header with premium glow filters and neon styling
    svg = []
    svg.append(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" width="100%" height="100%" style="background-color: #03010a;">')
    
    # SVG Definitions (Glow Filters, Gradients, and Styling)
    svg.append('  <defs>')
    # Cyberpunk color gradients
    svg.append('    <linearGradient id="cyan-magenta" x1="0%" y1="0%" x2="100%" y2="100%">')
    svg.append('      <stop offset="0%" stop-color="#00ffff" stop-opacity="0.8"/>')
    svg.append('      <stop offset="100%" stop-color="#ff007f" stop-opacity="0.8"/>')
    svg.append('    </linearGradient>')
    
    svg.append('    <linearGradient id="gold-orange" x1="0%" y1="0%" x2="100%" y2="100%">')
    svg.append('      <stop offset="0%" stop-color="#ffaa00" stop-opacity="0.9"/>')
    svg.append('      <stop offset="100%" stop-color="#ff3c00" stop-opacity="0.9"/>')
    svg.append('    </linearGradient>')

    svg.append('    <linearGradient id="neon-glow" x1="0%" y1="100%" x2="0%" y2="0%">')
    svg.append('      <stop offset="0%" stop-color="#000" stop-opacity="0"/>')
    svg.append('      <stop offset="50%" stop-color="#00f0ff" stop-opacity="0.5"/>')
    svg.append('      <stop offset="100%" stop-color="#ff007f" stop-opacity="0.9"/>')
    svg.append('    </linearGradient>')

    svg.append('    <radialGradient id="core-glow" cx="50%" cy="50%" r="50%">')
    svg.append('      <stop offset="0%" stop-color="#00f0ff" stop-opacity="0.4"/>')
    svg.append('      <stop offset="50%" stop-color="#1a0033" stop-opacity="0.2"/>')
    svg.append('      <stop offset="100%" stop-color="#03010a" stop-opacity="0"/>')
    svg.append('    </radialGradient>')

    svg.append('    <radialGradient id="shield-radial" cx="50%" cy="50%" r="50%">')
    svg.append('      <stop offset="0%" stop-color="#ff007f" stop-opacity="0.15"/>')
    svg.append('      <stop offset="70%" stop-color="#00f0ff" stop-opacity="0.03"/>')
    svg.append('      <stop offset="100%" stop-color="#03010a" stop-opacity="0"/>')
    svg.append('    </radialGradient>')

    # Premium high-intensity glow filter
    svg.append('    <filter id="glow-heavy" x="-50%" y="-50%" width="200%" height="200%">')
    svg.append('      <feGaussianBlur stdDeviation="6" result="blur1"/>')
    svg.append('      <feGaussianBlur stdDeviation="15" result="blur2"/>')
    svg.append('      <feMerge>')
    svg.append('        <feMergeNode in="blur2"/>')
    svg.append('        <feMergeNode in="blur1"/>')
    svg.append('        <feMergeNode in="SourceGraphic"/>')
    svg.append('      </feMerge>')
    svg.append('    </filter>')

    # Subtle holographic drop shadow
    svg.append('    <filter id="shadow-holo" x="-20%" y="-20%" width="140%" height="140%">')
    svg.append('      <feDropShadow dx="0" dy="0" stdDeviation="8" flood-color="#00ffff" flood-opacity="0.6"/>')
    svg.append('    </filter>')
    svg.append('  </defs>')

    # Background Matrix Matrix Layer (Warping Grid)
    svg.append('  <!-- Warping spacetime computation grid -->')
    svg.append('  <g opacity="0.15">')
    grid_lines = 32
    for i in range(grid_lines + 1):
        # Horizontal lines wrapping around cybernetic vortex
        y_val = 50 + (height - 100) * i / grid_lines
        d_path = f"M 50 {y_val} Q {cx} {y_val + 60 * math.sin(i*0.2)} {width - 50} {y_val}"
        svg.append(f'    <path d="{d_path}" fill="none" stroke="#5b2c9e" stroke-width="0.7" />')
        # Vertical lines warping inward
        x_val = 50 + (width - 100) * i / grid_lines
        d_path_v = f"M {x_val} 50 Q {x_val + 60 * math.cos(i*0.2)} {cy} {x_val} {height - 50}"
        svg.append(f'    <path d="{d_path_v}" fill="none" stroke="#5b2c9e" stroke-width="0.7" />')
    svg.append('  </g>')

    # Ambient core radial light
    svg.append(f'  <circle cx="{cx}" cy="{cy}" r="650" fill="url(#core-glow)" />')

    # Cryptographic Group Law Layout: Weierstrass Elliptic Curve
    svg.append('  <!-- Weierstrass Curve Group Law Visualization -->')
    # Curve equation: y^2 = x^3 - 4x + 4 (shifted and scaled)
    # We plot it in a beautiful frame on the left side of the canvas
    scale_x, scale_y = 60, 60
    origin_x, origin_y = cx - 450, cy + 50
    curve_points = []
    
    # Generate curve paths
    path_pos = []
    path_neg = []
    
    for x_step in range(-120, 200):
        x = x_step / 50.0
        # Weierstrass evaluation
        y_sq = x**3 - 3.0*x + 3.0
        if y_sq >= 0:
            y = math.sqrt(y_sq)
            px = origin_x + x * scale_x
            py_pos = origin_y - y * scale_y
            py_neg = origin_y + y * scale_y
            path_pos.append(f"{px:.2f},{py_pos:.2f}")
            path_neg.insert(0, f"{px:.2f},{py_neg:.2f}")
            
    curve_path = "M " + " L ".join(path_pos + path_neg)
    svg.append(f'  <path d="{curve_path}" fill="none" stroke="#00ffff" stroke-width="1.8" filter="url(#glow-heavy)" opacity="0.8" />')

    # Plot Group Addition: P + Q = R
    # Point P: x = -1.2, Q: x = 0.8
    xp, yp = -1.2, math.sqrt((-1.2)**3 - 3*(-1.2) + 3)
    xq, yq = 0.6, math.sqrt((0.6)**3 - 3*(0.6) + 3)
    # Slope s = (yq - yp) / (xq - xp)
    s = (yq - yp) / (xq - xp)
    # R intersection: xr = s^2 - xp - xq
    xr = s**2 - xp - xq
    yr = s * (xr - xp) + yp
    # Reflected point R = P + Q: (xr, -yr)
    xrpq, yrpq = xr, -yr

    # Screen coordinates for geometric addition nodes
    px_s, py_s = origin_x + xp * scale_x, origin_y - yp * scale_y
    qx_s, pyq_s = origin_x + xq * scale_x, origin_y - yq * scale_y
    rx_s, pyr_s = origin_x + xr * scale_x, origin_y - yr * scale_y
    rpq_s, pyrpq_s = origin_x + xrpq * scale_x, origin_y - yrpq * scale_y

    # Draw Secant line intersecting curve at three points, and reflection line
    svg.append(f'  <line x1="{px_s - 150*scale_x/60}" y1="{py_s - 150*s*scale_y/60}" x2="{rx_s + 100*scale_x/60}" y2="{pyr_s + 100*s*scale_y/60}" stroke="#ff007f" stroke-width="1" stroke-dasharray="4,4" opacity="0.6"/>')
    svg.append(f'  <line x1="{rx_s}" y1="{pyr_s}" x2="{rpq_s}" y2="{pyrpq_s}" stroke="#ffaa00" stroke-width="1.2" stroke-dasharray="2,2" opacity="0.8"/>')

    # Draw labeled mathematical points
    for point_name, px, py, color in [("P", px_s, py_s, "#00ffff"), ("Q", qx_s, pyq_s, "#00ffff"), ("P*Q", rx_s, pyr_s, "#ff007f"), ("P+Q", rpq_s, pyrpq_s, "#ffaa00")]:
        svg.append(f'  <circle cx="{px}" cy="{py}" r="5.5" fill="{color}" filter="url(#glow-heavy)"/>')
        svg.append(f'  <text x="{px + 12}" y="{py - 12}" fill="{color}" font-family="monospace" font-size="12" font-weight="bold">{point_name}</text>')

    # The Zero-Knowledge Core: Orthographic 3D Projections (Hyper-dimensional crystalline structure)
    # The structure will rotate dynamically via nested SMIL groups
    svg.append('  <!-- Programmatic 3D Crystalline Lattice (ZK-Core Oracle) -->')
    svg.append(f'  <g transform="translate({cx}, {cy})">')
    
    # Core pulsating aura
    svg.append('    <circle cx="0" cy="0" r="160" fill="url(#shield-radial)" filter="url(#glow-heavy)">')
    svg.append('      <animate attributeName="r" values="140;180;140" dur="4s" repeatCount="indefinite" />')
    svg.append('    </circle>')

    # Compute a 3D dual-icosidodecahedron frame projecting to 2D
    # Vertex coordinates (3D geometry computed statically for render speed)
    phi = (1.0 + math.sqrt(5.0)) / 2.0
    vertices3d = []
    # Icosahedron vertices
    for i in [-1, 1]:
        for j in [-1, 1]:
            vertices3d.append((0, i * phi, j / phi))
            vertices3d.append((i / phi, 0, j * phi))
            vertices3d.append((i * phi, j / phi, 0))

    # Project vertices to 2D using isometric transformation
    projected = []
    scale_3d = 120.0
    for x3, y3, z3 in vertices3d:
        # Rotate around y and x axes to orient beautifully
        theta, psi = 0.5, 0.6
        x_rot = x3 * math.cos(theta) - z3 * math.sin(theta)
        z_rot = x3 * math.sin(theta) + z3 * math.cos(theta)
        
        y_rot = y3 * math.cos(psi) - z_rot * math.sin(psi)
        z_final = y3 * math.sin(psi) + z_rot * math.cos(psi)
        
        px_2d = x_rot * scale_3d
        py_2d = y_rot * scale_3d
        projected.append((px_2d, py_2d))

    # Draw edges between projected 3D nodes (connect vertices within distance thresholds)
    svg.append('    <!-- Outer Crystal Shell -->')
    svg.append('    <g stroke="url(#cyan-magenta)" stroke-width="1.2" opacity="0.65" fill="none">')
    for i in range(len(vertices3d)):
        for j in range(i + 1, len(vertices3d)):
            dx = vertices3d[i][0] - vertices3d[j][0]
            dy = vertices3d[i][1] - vertices3d[j][1]
            dz = vertices3d[i][2] - vertices3d[j][2]
            dist = math.sqrt(dx*dx + dy*dy + dz*dz)
            if dist < 1.95:  # Valid edge threshold in an icosahedron
                svg.append(f'      <line x1="{projected[i][0]:.2f}" y1="{projected[i][1]:.2f}" x2="{projected[j][0]:.2f}" y2="{projected[j][1]:.2f}">')
                # Inject morph scale animation to make the core breathe organically
                svg.append('        <animateTransform attributeName="transform" type="scale" values="1;1.08;1" dur="6s" repeatCount="indefinite"/>')
                svg.append('      </line>')
    svg.append('    </g>')

    # Draw Inner Hyper-Core (Dense Golden Core Node)
    inner_projected = []
    scale_3d_inner = 65.0
    for x3, y3, z3 in vertices3d:
        # Opposite rotation for counter-dimensional look
        theta, psi = -0.4, -0.5
        x_rot = x3 * math.cos(theta) - z3 * math.sin(theta)
        z_rot = x3 * math.sin(theta) + z3 * math.cos(theta)
        
        y_rot = y3 * math.cos(psi) - z_rot * math.sin(psi)
        px_2d = x_rot * scale_3d_inner
        py_2d = y_rot * scale_3d_inner
        inner_projected.append((px_2d, py_2d))

    svg.append('    <!-- Golden Inner Core Shield -->')
    svg.append('    <g stroke="url(#gold-orange)" stroke-width="1.4" opacity="0.85" fill="none">')
    for i in range(len(vertices3d)):
        for j in range(i + 1, len(vertices3d)):
            dx = vertices3d[i][0] - vertices3d[j][0]
            dy = vertices3d[i][1] - vertices3d[j][1]
            dz = vertices3d[i][2] - vertices3d[j][2]
            dist = math.sqrt(dx*dx + dy*dy + dz*dz)
            if dist < 1.95:
                svg.append(f'      <line x1="{inner_projected[i][0]:.2f}" y1="{inner_projected[i][1]:.2f}" x2="{inner_projected[j][0]:.2f}" y2="{inner_projected[j][1]:.2f}">')
                svg.append('        <animateTransform attributeName="transform" type="scale" values="1;0.92;1" dur="4s" repeatCount="indefinite"/>')
                svg.append('      </line>')
    svg.append('    </g>')

    # Central Core Node
    svg.append('    <circle cx="0" cy="0" r="14" fill="#ffffff" filter="url(#glow-heavy)">')
    svg.append('      <animate attributeName="r" values="12;16;12" dur="2s" repeatCount="indefinite"/>')
    svg.append('    </circle>')
    
    # Closing ZK-Core coordinate translation group
    svg.append('  </g>')

    # The Rings of Verification (SMIL-Animated concentric metadata layers)
    svg.append('  <!-- Concentric Orbital Verification Rings -->')
    
    # Ring 1: OPCODES & Cryptographic prime numbers
    svg.append(f'  <g transform="translate({cx}, {cy})">')
    svg.append('    <circle cx="0" cy="0" r="280" fill="none" stroke="url(#cyan-magenta)" stroke-width="1" stroke-dasharray="5,20" opacity="0.5" filter="url(#glow-heavy)">')
    svg.append('      <animateTransform attributeName="transform" type="rotate" from="0" to="360" dur="40s" repeatCount="indefinite"/>')
    svg.append('    </circle>')
    
    # Ring 1 text layout: programmatically rotating equations
    opcodes = ["PUSH32", "JUMPDEST", "MSTORE", "SSTORE", "DELEGATECALL", "STATICCALL", "REENTRANCY_GUARD", "HALO2_PROOF", "PLONKISH", "SNARK_VERIFY", "R1CS_CONSTRAINT", "KECCAK256"]
    for idx, op in enumerate(opcodes):
        angle = (idx * 360) / len(opcodes)
        rad = math.radians(angle)
        tx, ty = 280 * math.cos(rad), 280 * math.sin(rad)
        svg.append(f'    <g transform="translate({tx:.2f}, {ty:.2f}) rotate({angle + 90})">')
        svg.append(f'      <text x="0" y="0" fill="#00ffff" font-family="monospace" font-size="9" font-weight="bold" text-anchor="middle" opacity="0.75">{op}</text>')
        svg.append('    </g>')
    svg.append('  </g>')

    # Ring 2: The Outer Golden Orbit (Euler Totient, Weierstrass, ZK-SNARK parameters)
    svg.append(f'  <g transform="translate({cx}, {cy})">')
    svg.append('    <circle cx="0" cy="0" r="380" fill="none" stroke="url(#gold-orange)" stroke-width="1.5" stroke-dasharray="10, 40, 30, 20" opacity="0.4">')
    svg.append('      <animateTransform attributeName="transform" type="rotate" from="360" to="0" dur="60s" repeatCount="indefinite"/>')
    svg.append('    </circle>')
    
    # Cryptographic metadata equations
    equations = [
        "y^2 = x^3 + ax + b (mod p)",
        "L(x)R(x) - O(x) = h(x)t(x)",
        "e(P, Q) = e(P+R, S)",
        "T = g^(a*b) mod p",
        "pi = (A, B, C)",
        "H(m) || H(R)",
        "ECDSA_VERIFY",
        "ZCC_COMPILED_PROVE"
    ]
    for idx, eq in enumerate(equations):
        angle = (idx * 360) / len(equations)
        rad = math.radians(angle)
        tx, ty = 380 * math.cos(rad), 380 * math.sin(rad)
        svg.append(f'    <g transform="translate({tx:.2f}, {ty:.2f}) rotate({angle + 90})">')
        svg.append(f'      <text x="0" y="0" fill="#ffaa00" font-family="Courier New" font-size="10" font-weight="bold" text-anchor="middle" opacity="0.8">{eq}</text>')
        svg.append('    </g>')
    svg.append('  </g>')

    # Ring 3: Turing State Cells (Concentric geometric tick arches on outer boundary)
    svg.append(f'  <g transform="translate({cx}, {cy})">')
    turing_cells = 96
    for i in range(turing_cells):
        angle = (i * 360) / turing_cells
        rad = math.radians(angle)
        # Alternate cell sizes for high complexity
        h_len = 16 if i % 4 == 0 else (10 if i % 2 == 0 else 6)
        color = "#ff007f" if i % 4 == 0 else "#00f0ff"
        opacity = 0.8 if i % 4 == 0 else 0.4
        
        x1_t, y1_t = 430 * math.cos(rad), 430 * math.sin(rad)
        x2_t, y2_t = (430 + h_len) * math.cos(rad), (430 + h_len) * math.sin(rad)
        svg.append(f'    <line x1="{x1_t:.2f}" y1="{y1_t:.2f}" x2="{x2_t:.2f}" y2="{y2_t:.2f}" stroke="{color}" stroke-width="1.2" opacity="{opacity}" />')
    svg.append('    <!-- Outer tracking dial rotation animation -->')
    svg.append('    <animateTransform attributeName="transform" type="rotate" from="0" to="360" dur="120s" repeatCount="indefinite" />')
    svg.append('  </g>')

    # Mathematical Proof streams rising from bottom: Opcode streams along bezier channels
    svg.append('  <!-- Floating Mathematical Proof Streams -->')
    streams = [
        # (Start x, Start y, Ctrl1 x, Ctrl1 y, Ctrl2 x, Ctrl2 y, End x, End y, delay, opcodes)
        (250, height + 50, 200, cy + 200, 300, cy - 200, 250, -50, "0s", ["PUSH32", "0xef32ab91", "ADD", "SSTORE"]),
        (width - 250, height + 50, width - 200, cy + 200, width - 300, cy - 200, width - 250, -50, "2.5s", ["CALL", "0x00...00", "GAS", "RETURN"]),
        (450, height + 50, 500, cy + 300, 400, cy - 100, 600, -50, "1.2s", ["JUMP", "JUMPDEST", "DUP1", "SWAP1"]),
        (width - 450, height + 50, width - 500, cy + 300, width - 400, cy - 100, width - 600, -50, "3.7s", ["MLOAD", "MSTORE", "0x20", "SHA3"])
    ]

    for idx, (sx, sy, c1x, c1y, c2x, c2y, ex, ey, delay, ops) in enumerate(streams):
        # Curved path for proof stream
        d_path = f"M {sx} {sy} C {c1x} {c1y}, {c2x} {c2y}, {ex} {ey}"
        # Render invisible path for motion alignment (or direct drawing with animated glow offsets)
        svg.append(f'  <path id="path-stream-{idx}" d="{d_path}" fill="none" stroke="none"/>')
        
        # Render actual flowing text
        svg.append(f'  <text font-family="monospace" font-size="11" font-weight="bold" fill="url(#cyan-magenta)">')
        svg.append(f'    <textPath href="#path-stream-{idx}" startOffset="100%">')
        svg.append(' &gt;&gt; ' + ' &gt;&gt; '.join(ops))
        svg.append(f'      <animate attributeName="startOffset" from="100%" to="-100%" dur="20s" begin="{delay}" repeatCount="indefinite"/>')
        svg.append('    </textPath>')
        svg.append('  </text>')

    # Legendary Masterwork Foreground Frame and Cryptographic Sigils
    svg.append('  <!-- Cyber-Sigils of Compile Mastery (ZK-Snark / EVM Gates) -->')
    
    # Left Sigil: Constraint System Gate
    sigil_x, sigil_y = 150, 150
    svg.append(f'  <g transform="translate({sigil_x}, {sigil_y})">')
    svg.append('    <circle cx="0" cy="0" r="45" fill="none" stroke="#00ffff" stroke-width="1" stroke-dasharray="3,3" opacity="0.6"/>')
    svg.append('    <polygon points="0,-40 35,20 -35,20" fill="none" stroke="#ff007f" stroke-width="1.5" opacity="0.8" filter="url(#glow-heavy)"/>')
    svg.append('    <circle cx="0" cy="0" r="10" fill="none" stroke="#ffaa00" stroke-width="1.8"/>')
    svg.append('    <text x="0" y="4" fill="#ffffff" font-family="monospace" font-size="10" font-weight="bold" text-anchor="middle">R1CS</text>')
    svg.append('    <animateTransform attributeName="transform" type="rotate" from="0" to="360" dur="25s" repeatCount="indefinite"/>')
    svg.append('  </g>')

    # Right Sigil: Polynomial Commitment Gate
    sigil2_x, sigil2_y = width - 150, 150
    svg.append(f'  <g transform="translate({sigil2_x}, {sigil2_y})">')
    svg.append('    <circle cx="0" cy="0" r="45" fill="none" stroke="#ffaa00" stroke-width="1" stroke-dasharray="3,3" opacity="0.6"/>')
    # Rotating outer octagram
    svg.append('    <g>')
    svg.append('      <rect x="-28" y="-28" width="56" height="56" fill="none" stroke="#00ffff" stroke-width="1.2" opacity="0.8" />')
    svg.append('      <rect x="-28" y="-28" width="56" height="56" transform="rotate(45)" fill="none" stroke="#ff007f" stroke-width="1.2" opacity="0.8" />')
    svg.append('      <animateTransform attributeName="transform" type="rotate" from="360" to="0" dur="30s" repeatCount="indefinite"/>')
    svg.append('    </g>')
    svg.append('    <circle cx="0" cy="0" r="12" fill="none" stroke="#ffffff" stroke-width="1.8" filter="url(#glow-heavy)"/>')
    svg.append('    <text x="0" y="4" fill="#ffffff" font-family="monospace" font-size="10" font-weight="bold" text-anchor="middle">KZG</text>')
    svg.append('  </g>')

    # Add floating mathematical variables as "data embers" in background
    svg.append('  <!-- Ambient Background Mathematical Symbols -->')
    sigil_positions = [
        ("α", 300, 250, "#00ffff"), ("β", 350, 800, "#ff007f"), ("γ", 1500, 300, "#ffaa00"),
        ("δ", 1600, 850, "#00ffff"), ("λ", 800, 200, "#ff007f"), ("H", 1100, 220, "#ffaa00"),
        ("Σ", 500, 900, "#00ffff"), ("t(x)", 1350, 920, "#ff007f")
    ]
    for symbol, x_p, y_p, col in sigil_positions:
        svg.append(f'  <text x="{x_p}" y="{y_p}" fill="{col}" font-family="serif" font-size="28" font-style="italic" opacity="0.25" font-weight="bold">')
        svg.append(f'    <animate attributeName="opacity" values="0.1;0.4;0.1" dur="{7 + (x_p%5)}s" repeatCount="indefinite"/>')
        svg.append(f'    <animate attributeName="y" values="{y_p};{y_p-30};{y_p}" dur="{10 + (y_p%7)}s" repeatCount="indefinite"/>')
        svg.append(f'    {symbol}')
        svg.append('  </text>')

    # Epic center metadata text: The Proof Certificate
    svg.append('  <!-- Masterwork Inscription Badge -->')
    svg.append(f'  <g transform="translate({cx}, {height - 85})">')
    # Framed box
    svg.append('    <rect x="-240" y="-35" width="480" height="60" rx="6" fill="#040112" stroke="url(#cyan-magenta)" stroke-width="1.5" opacity="0.9" filter="url(#glow-heavy)"/>')
    svg.append('    <text x="0" y="-12" fill="#ffffff" font-family="monospace" font-size="14" font-weight="bold" text-anchor="middle" letter-spacing="4">PROOF OF COMPUTATION</text>')
    svg.append('    <text x="0" y="12" fill="#ff007f" font-family="monospace" font-size="11" font-weight="bold" text-anchor="middle" letter-spacing="2">ZKAEDI PRIME // ZERO-KNOWLEDGE COMPILED V3</text>')
    svg.append('  </g>')

    svg.append('</svg>')

    # Write out file
    with open(filename, "w", encoding="utf-8") as f:
        f.write("\n".join(svg))
    print(f"Legendary Proof of Computation SVG successfully compiled to: {filename}")

if __name__ == "__main__":
    generate_svg()
