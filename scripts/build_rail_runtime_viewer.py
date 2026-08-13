import json
from pathlib import Path

def main():
    release = Path("output/releases/the-red-door")

    assembly = json.loads((release / "universal_scene_assembly.json").read_text())
    director = json.loads((release / "director_plan.json").read_text())
    rails = json.loads((release / "camera_rails.json").read_text())
    triggers = json.loads((release / "trigger_volumes.json").read_text())
    sequence = director.get("sequence", {}).get("sequence", [])

    html = f"""<!doctype html>
<html>
<head>
<meta charset="utf-8">
<title>The Red Door — Live Rail Runtime</title>
<style>
html,body{{margin:0;width:100%;height:100%;overflow:hidden;background:#05030a;color:#e9faff;font-family:system-ui}}
#hud,#timeline,#controls{{
  position:absolute;z-index:10;background:rgba(0,0,0,.68);
  border:1px solid rgba(127,252,255,.28);border-radius:14px;padding:14px
}}
#hud{{top:16px;left:16px;width:420px}}
#timeline{{top:16px;right:16px;width:360px;max-height:80vh;overflow:auto}}
#controls{{bottom:16px;left:16px}}
button{{padding:10px 16px;border:0;border-radius:10px;background:#7ffcff;color:#061014;font-weight:800;margin-right:8px;cursor:pointer}}
.item{{padding:10px;margin:8px 0;border-left:4px solid #334;background:rgba(255,255,255,.05);border-radius:10px}}
.active{{border-left-color:#7ffcff;box-shadow:0 0 18px rgba(127,252,255,.3)}}
.trigger{{color:#ffd37f;font-weight:bold;margin-top:6px;}}
a{{color:#7ffcff}}
</style>
</head>
<body>
<div id="hud">
<h1>🎥 Rail-Driven Cinematic Runtime</h1>
<div id="shot">Connecting to live runtime_state.json...</div>
<div id="trigger" class="trigger"></div>
<p><a href="index.html">Production Dashboard</a> · <a href="omega_command_center.html">Omega Center</a></p>
</div>

<div id="timeline"></div>

<div id="controls">
<button id="btn-live" onclick="toggleLive()">Live Mode: ON</button>
<button onclick="play()">▶ Play</button>
<button onclick="pause()">⏸ Pause</button>
<span id="time">0.0s</span>
</div>

<script type="module">
import * as THREE from 'https://unpkg.com/three@0.160.0/build/three.module.js';
import {{ GLTFLoader }} from 'https://unpkg.com/three@0.160.0/examples/jsm/loaders/GLTFLoader.js';
import {{ OrbitControls }} from 'https://unpkg.com/three@0.160.0/examples/jsm/controls/OrbitControls.js';

const assembly = {json.dumps(assembly)};
const director = {json.dumps(director)};
const rails = {json.dumps(rails.get("rails", []))};
const triggers = {json.dumps(triggers.get("triggers", []))};
const sequence = director.sequence.sequence;

const scene = new THREE.Scene();
scene.background = new THREE.Color(0x05030a);
scene.fog = new THREE.Fog(0x05030a, 14, 55);

const camera = new THREE.PerspectiveCamera(60, innerWidth/innerHeight, 0.1, 1000);
camera.position.set(7,5,10);

const renderer = new THREE.WebGLRenderer({{antialias:true}});
renderer.setSize(innerWidth, innerHeight);
renderer.outputColorSpace = THREE.SRGBColorSpace;
document.body.appendChild(renderer.domElement);

const controls = new OrbitControls(camera, renderer.domElement);
controls.target.set(0,1,-1);
controls.update();

scene.add(new THREE.HemisphereLight(0x7ffcff,0x220018,1.7));

const key = new THREE.DirectionalLight(0xffffff,2.4);
key.position.set(5,9,5);
scene.add(key);

const redGlow = new THREE.PointLight(0xff244f,4.5,24);
redGlow.position.set(0,2,-1);
scene.add(redGlow);

const blueGlow = new THREE.PointLight(0x00ccff,2.5,20);
blueGlow.position.set(-4,3,4);
scene.add(blueGlow);

scene.add(new THREE.GridHelper(50,50,0x335566,0x112233));

const loader = new GLTFLoader();

for (const node of assembly.world_graph.nodes) {{
  if (!node.local_path) continue;
  const placement = assembly.placements.find(p => p.node_id === node.id);
  if (!placement) continue;

  loader.load(node.local_path, gltf => {{
    const obj = gltf.scene;
    obj.position.set(placement.x, placement.y, placement.z);
    obj.rotation.y = THREE.MathUtils.degToRad(placement.rotation_y);
    obj.scale.setScalar(placement.scale);
    scene.add(obj);
  }});
}}

const timeline = document.getElementById("timeline");

function renderTimeline(active=-1) {{
  timeline.innerHTML = "<h2>Rail Timeline</h2>";
  sequence.forEach((s,i) => {{
    const rail = rails.find(r => r.shot_id === s.shot_id);
    const d = document.createElement("div");
    d.className = "item" + (i===active ? " active" : "");
    d.innerHTML = `<b>${{s.shot_id}}</b> ${{s.start_seconds}}-${{s.end_seconds}}s<br>${{s.type}} · ${{s.lens}} · ${{s.movement}}<br>Rail: ${{rail ? rail.rail_id : "none"}}`;
    timeline.appendChild(d);
  }});
}}

let playing=false,startTime=0,elapsed=0,liveMode=true;

window.toggleLive=()=>{{
  liveMode = !liveMode;
  const btn = document.getElementById("btn-live");
  btn.textContent = liveMode ? "Live Mode: ON" : "Live Mode: OFF";
  btn.style.background = liveMode ? "#7ffcff" : "#333";
  btn.style.color = liveMode ? "#061014" : "#ccc";
}};

window.play=()=>{{
  if(liveMode) toggleLive();
  if(!playing){{playing=true;startTime=performance.now()-elapsed*1000;}}
}};
window.pause=()=>{{playing=false;}};
window.reset=()=>{{playing=false;elapsed=0;setShot(0,0);}};

function lerp(a,b,t){{return a+(b-a)*t}}

function railPoint(path,t){{
  if(!path || path.length===0) return null;
  if(path.length===1) return path[0];

  if(t<=0.5){{
    const local=t/0.5;
    return interpolatePoint(path[0],path[1],local);
  }}
  const local=(t-0.5)/0.5;
  return interpolatePoint(path[1],path[2],local);
}}

function interpolatePoint(a,b,t){{
  return {{
    position:[
      lerp(a.position[0],b.position[0],t),
      lerp(a.position[1],b.position[1],t),
      lerp(a.position[2],b.position[2],t)
    ],
    target:[
      lerp(a.target[0],b.target[0],t),
      lerp(a.target[1],b.target[1],t),
      lerp(a.target[2],b.target[2],t)
    ]
  }};
}}

function activeShot(time){{
  return sequence.findIndex(s=>time>=s.start_seconds && time<s.end_seconds);
}}

function nearestTrigger(pos){{
  let best=null,dist=Infinity;
  for(const t of triggers){{
    const p=t.position || [0,0,0];
    const d=Math.hypot(pos[0]-p[0],pos[1]-p[1],pos[2]-p[2]);
    if(d<dist){{dist=d;best=t;}}
  }}
  return best && dist <= (best.radius || 2)+6 ? best : null;
}}

function setShot(index,localT){{
  if(index<0) index=sequence.length-1;
  const shot=sequence[index];
  if(!shot) return;

  const rail=rails.find(r=>r.shot_id===shot.shot_id);
  if(rail){{
    const pt=railPoint(rail.path,localT);
    if(pt){{
      camera.position.set(pt.position[0],pt.position[1],pt.position[2]);
      controls.target.set(pt.target[0],pt.target[1],pt.target[2]);
      controls.update();

      const trig = nearestTrigger(pt.target);
      document.getElementById("trigger").textContent = trig ? `Trigger: ${{trig.event_type}} · ${{trig.zone_id}}` : "";
    }}
  }}

  document.getElementById("shot").innerHTML = `${{shot.shot_id}} · ${{shot.type}}<br>${{shot.subject}}`;
  renderTimeline(index);
}}

async function syncLiveState() {{
  if (!liveMode) return;
  try {{
    const res = await fetch('runtime_state.json');
    if (!res.ok) return;
    const state = await res.json();
    
    elapsed = state.elapsed_time;
    
    if (state.camera) {{
      camera.position.set(state.camera.position[0], state.camera.position[1], state.camera.position[2]);
      controls.target.set(state.camera.target[0], state.camera.target[1], state.camera.target[2]);
      controls.update();
    }}
    
    document.getElementById("trigger").textContent = state.fired_trigger ? `🔥 Live Trigger Fired: ${{state.fired_trigger}}` : "";
    
    const activeIdx = sequence.findIndex(s => s.shot_id === state.active_shot);
    if (activeIdx !== -1) {{
      document.getElementById("shot").innerHTML = `<b>${{state.active_shot}} (LIVE STATE)</b><br>Weather: ${{state.world.weather}} | Stage: ${{state.world.state}}`;
      renderTimeline(activeIdx);
    }}
  }} catch (err) {{
    // Fail silently
  }}
}}

// Sync live state every 100ms
setInterval(syncLiveState, 100);

function animate() {{
  requestAnimationFrame(animate);

  if(!liveMode && playing){{
    elapsed=(performance.now()-startTime)/1000;
    const runtime=director.sequence.runtime_seconds;
    if(elapsed>runtime){{elapsed=runtime;playing=false;}}
    
    const idx=activeShot(elapsed);
    const shot=sequence[Math.max(0,idx)];
    if(shot){{
      const localT=Math.min(1,Math.max(0,(elapsed-shot.start_seconds)/shot.duration_seconds));
      setShot(Math.max(0,idx),localT);
    }}
  }}

  redGlow.intensity=3.5+Math.sin(performance.now()/400)*1.2;
  blueGlow.intensity=2.0+Math.sin(performance.now()/700)*.7;

  document.getElementById("time").textContent=elapsed.toFixed(1)+"s";
  renderer.render(scene,camera);
}}

renderTimeline();
animate();

addEventListener("resize",()=>{{
  camera.aspect=innerWidth/innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(innerWidth,innerHeight);
}});
</script>
</body>
</html>
"""

    (release / "rail_cinematic_runtime.html").write_text(html)
    print(release / "rail_cinematic_runtime.html")

if __name__ == "__main__":
    main()
