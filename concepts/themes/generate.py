#!/usr/bin/env python3
"""Local palette concepts derived from the existing v2 SVG; no firmware changes."""
from copy import deepcopy
import json
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET

HERE = Path(__file__).resolve().parent
NS = 'http://www.w3.org/2000/svg'
ET.register_namespace('', NS)
T = lambda name: '{'+NS+'}'+name
source = ET.parse(HERE.parent/'home-screen-v2.svg').getroot()
base = deepcopy(next(g for g in source.iter(T('g')) if g.get('transform','').startswith('translate(32 169)')))
base.attrib.pop('transform')
base.set('font-family','DejaVu Sans,sans-serif')
for node in list(base):
    if node.tag != T('text'): continue
    value = node.text or ''
    if value == 'Sound on': base.remove(node); continue
    changes = {
        'WEATHER LINK': ('WX LINK', {}),
        '3:42 PM': ('10/21 3:42 PM', {'font-size':'9'}),
        'Atlas · 2s ago': ('Atlas · Current · 2s', {'font-size':'9'}),
        '· RF': ('RF +', {'x':'204','font-size':'7'}),
        '· Wi-Fi': ('WiFi +', {'x':'232','font-size':'7'}),
        '· SD': ('APRS +', {'x':'267','font-size':'7'}),
        '24 klx · Sky est. mixed': ('24.0 klx · UV 3 Moderate', {'font-size':'9'}),
        'Gust 14 · 10 min': ('Gust 14.0 · 5 min', {}),
        'SEA LEVEL': ('BARO', {}),
    }
    if value in changes:
        node.text, attrs = changes[value]; node.attrib.update(attrs)
    if node.get('x')=='223' and node.get('y')=='175': node.set('y','171')
    if node.get('x')=='282' and node.get('y')=='175': node.set('y','170')

def text(parent, value, **attrs):
    node=ET.SubElement(parent,T('text'),{k.replace('_','-'):str(v) for k,v in attrs.items()});node.text=value

text(base,'N0CALL-1',x=160,y=17,font_size=14,font_weight=700,text_anchor='middle',fill='#61d8ed')
text(base,'NWS',x=140,y=62,font_size=6,fill='#a2b4c9')
text(base,'NWS',x=223,y=182,font_size=7,fill='#a2b4c9')
ET.SubElement(base,T('path'),{'d':'M300 30 L302 30 L305 27 L305 37 L302 34 L300 34 Z M307 29 L313 35 M313 29 L307 35','fill':'none','stroke':'#a2b4c9','stroke-width':'1'})
for i in range(3): ET.SubElement(base,T('circle'),{'cx':str(153+i*7),'cy':'186','r':'1','fill':'#61d8ed' if i==0 else '#233246'})

# All positions, sample readings, typography and paths stay identical.
THEMES = [
 {'id':'matrix','name':'Matrix','note':'Green phosphor on near-black.','colors':['#030805','#0b180f','#c8ffd0','#6bd17c','#65ff8a','#153d20','#83ff9d','#0a2112','#245c31']},
 {'id':'amber','name':'Amber','note':'Warm amber with an old radio feel.','colors':['#130d07','#261b0f','#fff0d2','#d7b68a','#ffbc5c','#574027','#9ce2b2','#15271b','#355b40']},
 {'id':'ice-blue','name':'Ice Blue','note':'Cool blue on deep slate.','colors':['#08131e','#142737','#eff8ff','#a9c5dd','#79baff','#2d485f','#8bdfc1','#102b2a','#2e5650']},
 {'id':'violet','name':'Violet','note':'Soft lavender on dark plum.','colors':['#120d1c','#24192f','#f7f0ff','#bfaed0','#c29aff','#493455','#96e2bd','#142923','#355947']},
 {'id':'paper','name':'Paper','note':'Light gray with crisp blue accents.','colors':['#eef1f4','#ffffff','#172536','#526478','#145aa0','#bdc9d4','#226d49','#e0eee5','#a5c8b2']},
]
KEYS=['background','card','text','muted','accent','divider','connected','alert_card','alert_border']
OLD=['#080f1b','#111e2d','#f0f5fb','#a2b4c9','#61d8ed','#233246','#83e2b1','#102d29','#244a40']

def screen(theme):
    group=deepcopy(base)
    mapping=dict(zip(OLD,theme['colors']))
    mapping.update({'#314c63':theme['colors'][5], '#203e52':theme['colors'][1]})
    if theme['id']=='matrix':mapping['#ffd079']=theme['colors'][4]
    for node in group.iter():
        for attr in ('fill','stroke'):
            if node.get(attr) in mapping:node.set(attr,mapping[node.get(attr)])
    return group

def svg_root(width,height):
    return ET.Element(T('svg'),{'width':str(width),'height':str(height),'viewBox':f'0 0 {width} {height}'})

for theme in THEMES:
    root=svg_root(320,240)
    title=ET.SubElement(root,T('title'));title.text=theme['name']+' — color-only Home concept'
    root.append(screen(theme));ET.ElementTree(root).write(HERE/(theme['id']+'.svg'),encoding='unicode')

sheet=svg_root(1160,780)
ET.SubElement(sheet,T('rect'),{'width':'1160','height':'780','fill':'#e8ecf0'})
text(sheet,'CYD WEATHER LINK · COLOR STUDIES',x=34,y=35,font_family='DejaVu Sans,sans-serif',font_size=13,fill='#506277')
text(sheet,'Five palettes. One familiar screen.',x=34,y=72,font_family='DejaVu Sans,sans-serif',font_size=28,font_weight=700,fill='#172536')
text(sheet,'Local concepts · Sample readings · Color changes only',x=34,y=99,font_family='DejaVu Sans,sans-serif',font_size=13,fill='#506277')
for i,theme in enumerate(THEMES):
    x=34+(i%3)*380;y=135+(i//3)*310
    text(sheet,theme['name'],x=x,y=y,font_family='DejaVu Sans,sans-serif',font_size=17,font_weight=700,fill='#172536')
    group=screen(theme);group.set('transform',f'translate({x} {y+15})');sheet.append(group)
    text(sheet,theme['note'],x=x,y=y+275,font_family='DejaVu Sans,sans-serif',font_size=11,fill='#506277')
ET.ElementTree(sheet).write(HERE/'comparison.svg',encoding='unicode')
subprocess.run(['rsvg-convert',str(HERE/'comparison.svg'),'-o',str(HERE/'comparison.png')],check=True)

payload=[dict(id=t['id'],name=t['name'],note=t['note'],palette=dict(zip(KEYS,t['colors'])),svg=ET.tostring(screen(t),encoding='unicode')) for t in THEMES]
html='''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>CYD Weather Link · palette concepts</title>
<style>body{margin:0;background:#e8ecf0;color:#172536;font:16px system-ui,sans-serif}main{max-width:1120px;margin:auto;padding:30px 22px}h1{margin:6px 0 12px;font-size:32px}p{color:#526478;line-height:1.5}.eyebrow{font-size:12px;letter-spacing:2px}.choices{display:flex;flex-wrap:wrap;gap:10px;margin:24px 0}button{font:inherit;background:white;border:2px solid #bdc9d4;border-radius:9px;padding:12px 18px;cursor:pointer}button[aria-pressed=true]{border-color:#145aa0;box-shadow:0 0 0 1px #145aa0}.dot{display:inline-block;width:13px;height:13px;margin-right:9px;border-radius:50%;border:1px solid #526478}.focus{display:grid;grid-template-columns:minmax(320px,640px) 1fr;gap:30px;align-items:center;margin-bottom:40px}.display{display:block;width:100%;height:auto;box-shadow:0 7px 24px #17253624}.swatches{display:flex;gap:12px;flex-wrap:wrap}.swatch{display:flex;gap:8px;align-items:center;font-size:12px}.swatch i{width:22px;height:22px;border:1px solid #8797a8;border-radius:5px}.grid{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:28px}.card{padding:0;border:0;background:none;text-align:left}.card h3{font-size:18px;margin:0 0 12px}.card p{font-size:13px}.card[aria-pressed=true]{box-shadow:none}.card[aria-pressed=true] .display{outline:3px solid #145aa0;outline-offset:4px}.notice{font-size:13px;margin-top:30px}button:focus-visible{outline:3px solid #145aa0;outline-offset:4px}@media(max-width:800px){.focus{grid-template-columns:1fr}.grid{grid-template-columns:repeat(2,minmax(0,1fr))}}@media(max-width:520px){.grid{grid-template-columns:1fr}h1{font-size:26px}.focus{gap:12px}main{padding:22px 16px}}</style>
<main><div class="eyebrow">CYD WEATHER LINK / LOCAL CONCEPT</div><h1>Five palettes. Same Home screen.</h1><p>Choose a palette to enlarge it. Layout, labels, readings and controls stay the same.</p><div class="choices" id="choices" aria-label="Theme choices"></div><section class="focus"><svg id="preview" class="display" viewBox="0 0 320 240" role="img" aria-label="Selected theme preview"></svg><div><h2 id="name"></h2><p id="note"></p><div class="swatches" id="swatches"></div></div></section><div class="grid" id="grid"></div><p class="notice">Sample readings. Local concepts only; nothing installed on the device. Existing watch/warning colors and stale-data indicators remain separate from these everyday palettes.</p></main><script>
const themes=THEMES;
function select(id){const t=themes.find(t=>t.id===id);document.querySelector('#preview').innerHTML=t.svg;document.querySelector('#preview').setAttribute('aria-label',t.name+' theme preview');document.querySelector('#name').textContent=t.name;document.querySelector('#note').textContent=t.note;document.querySelectorAll('button[data-id]').forEach(b=>b.setAttribute('aria-pressed',String(b.dataset.id===id)));document.querySelector('#swatches').replaceChildren();for(const key of ['background','card','text','accent']){const row=document.createElement('span');row.className='swatch';const swatch=document.createElement('i');swatch.style.background=t.palette[key];row.append(swatch,key+' '+t.palette[key]);document.querySelector('#swatches').append(row);}}
for(const t of themes){const b=document.createElement('button');b.dataset.id=t.id;b.type='button';const dot=document.createElement('span');dot.className='dot';dot.style.background=t.palette.accent;b.append(dot,t.name);b.onclick=()=>select(t.id);document.querySelector('#choices').append(b);const card=document.createElement('button');card.type='button';card.className='card';card.dataset.id=t.id;card.setAttribute('aria-label','Enlarge '+t.name);card.innerHTML='<h3>'+t.name+'</h3><svg class="display" viewBox="0 0 320 240" aria-hidden="true">'+t.svg+'</svg><p>'+t.note+'</p>';card.onclick=()=>{select(t.id);document.querySelector('#choices').scrollIntoView({behavior:'smooth',block:'start'});};document.querySelector('#grid').append(card);}select('matrix');
</script></html>'''.replace('const themes=THEMES;', 'const themes='+json.dumps(payload)+';')
(HERE/'index.html').write_text(html)
(HERE/'palettes.json').write_text(json.dumps({t['id']:dict(zip(KEYS,t['colors'])) for t in THEMES},indent=2)+'\n')
print('Generated five SVG palette concepts, comparison SVG/PNG, local HTML preview and palette JSON.')
