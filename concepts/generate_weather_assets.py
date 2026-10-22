"""Generate original vector icons and concept previews; requires rsvg-convert."""
from pathlib import Path
from copy import deepcopy
from html import escape
import math
import subprocess
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / 'assets' / 'weather'
CONCEPTS = ROOT / 'concepts'
NS = 'http://www.w3.org/2000/svg'
ET.register_namespace('', NS)


def svg(body, width=48, height=48, title='Weather icon'):
    return (f'<svg xmlns="{NS}" width="{width}" height="{height}" '
            f'viewBox="0 0 {width} {height}" role="img">'
            f'<title>{escape(title)}</title>{body}</svg>')


def path(d, stroke='#72def0', fill='none', width=2):
    return (f'<path d="{d}" fill="{fill}" stroke="{stroke}" '
            f'stroke-width="{width}" stroke-linecap="round" stroke-linejoin="round"/>')


def sun(cx=24, cy=24, radius=9):
    body = f'<circle cx="{cx}" cy="{cy}" r="{radius}" fill="#ffcd70"/>'
    for i in range(8):
        angle = math.pi * i / 4
        coords = [cx + math.cos(angle) * (radius + 4), cy + math.sin(angle) * (radius + 4),
                  cx + math.cos(angle) * (radius + 7), cy + math.sin(angle) * (radius + 7)]
        body += path(f'M{coords[0]:.2f} {coords[1]:.2f} L{coords[2]:.2f} {coords[3]:.2f}', '#ffcd70')
    return body


def cloud():
    return path('M12 33 C2 33 2 20 12 20 C12 8 30 6 33 19 C45 15 49 33 36 33 Z', fill='#244359')


def moon():
    return path('M29 7 C11 4 4 28 20 36 C28 40 38 37 42 28 C26 33 18 18 29 7 Z', '#b6c8ff', '#344866')


def rain(heavy=False):
    body = cloud()
    for x in ([12, 20, 28, 36] if heavy else [15, 26, 37]):
        body += path(f'M{x} 37 l-3 6', width=2.6 if heavy else 2)
    return body


def snow():
    body = cloud()
    for x in [14, 27, 39]:
        body += path(f'M{x} 36 v8 M{x-3} 38 l6 4 M{x-3} 42 l6 -4', '#d9eaff', width=1.5)
    return body


def icons():
    return {
        'sunny': ('Sunny / bright estimate', sun()),
        'partly-cloudy': ('Partly cloudy / mixed estimate', sun(15, 15, 7) + cloud()),
        'cloudy': ('Cloudy / dim estimate', cloud()),
        'rain': ('Rain', rain()),
        'heavy-rain': ('Heavy rain', rain(True)),
        'thunderstorm': ('Thunderstorm / lightning', cloud() + path('M25 28 L17 39 H24 L21 46 L34 34 H27 L31 28 Z', '#ffcd70', '#ffcd70', 1)),
        'night': ('Night', moon()),
        'partly-cloudy-night': ('Cloudy night', '<g transform="translate(0 -1) scale(.8)">' + moon() + '</g>' + cloud()),
        'snow': ('Snow', snow()),
        'fog': ('Fog', cloud() + path('M6 38 H33 M16 43 H43', '#c7d8e9')),
        'windy': ('Windy', path('M5 16 H30 C42 16 39 5 32 8 M5 24 H39 C48 24 45 34 39 32 M5 32 H22 C33 32 30 43 24 40')),
        'unknown': ('Unknown / stale', cloud() + '<text x="24" y="29" fill="#f0f5fb" font-family="DejaVu Sans,sans-serif" font-size="17" text-anchor="middle">?</text>'),
    }


def render(source, target, width):
    subprocess.run(['rsvg-convert', '-w', str(width), '-o', str(target), str(source)], check=True)


def icon_sheet(entries):
    body = '<rect width="1000" height="680" fill="#eef1f4"/>'
    body += '<g font-family="DejaVu Sans,sans-serif">'
    body += '<text x="30" y="38" fill="#536579" font-size="11" letter-spacing="2">CYD WEATHER LINK / WEATHER GRAPHICS</text>'
    body += '<text x="30" y="80" fill="#12283a" font-size="30" font-weight="600">A consistent visual language.</text>'
    body += '<text x="30" y="110" fill="#536579" font-size="13">Original 48×48 vectors · Transparent PNG exports · Source-aware condition selection</text>'
    for i, (name, (label, shape)) in enumerate(entries.items()):
        x, y = 30 + (i % 4) * 242, 140 + (i // 4) * 160
        body += f'<rect x="{x}" y="{y}" width="214" height="100" rx="9" fill="#080f1b"/>'
        body += f'<g transform="translate({x+71} {y+13}) scale(1.5)">{shape}</g>'
        body += f'<text x="{x}" y="{y+122}" fill="#12283a" font-size="13" font-weight="600">{escape(name.replace("-", " ").title())}</text>'
    body += '<text x="30" y="647" fill="#536579" font-size="12">Asset availability does not establish sensing capability. Sky inferred from lux is labeled an estimate.</text></g>'
    dest = CONCEPTS / 'weather-icons.svg'
    dest.write_text(svg(body, 1000, 680, 'Weather Link weather graphics'))
    render(dest, dest.with_suffix('.png'), 1500)


def alert_preview():
    # Preserve the original concept; reuse its exact 320×240 layout.
    original = ET.parse(CONCEPTS / 'home-screen.svg').getroot()
    font_group = original.find(f'{{{NS}}}g')
    screens = [e for e in font_group if e.tag == f'{{{NS}}}g' and e.get('transform', '').startswith('translate(')]
    assert len(screens) == 3, 'Expected three original home screen states'
    doc = ET.fromstring(svg('', 1200, 620, 'Normal, watch, and warning home screen backgrounds'))
    sheet = ET.SubElement(doc, f'{{{NS}}}g', {'font-family': 'DejaVu Sans,sans-serif'})
    def add_rect(x, y, w, h, fill, rx=0):
        ET.SubElement(sheet, f'{{{NS}}}rect', dict(x=str(x), y=str(y), width=str(w), height=str(h), fill=fill, rx=str(rx)))
    def text(x, y, content, size=12, fill='#536579', weight='400'):
        node = ET.SubElement(sheet, f'{{{NS}}}text', {'x': str(x), 'y': str(y), 'font-size': str(size), 'fill': fill, 'font-weight': weight})
        node.text = content
    add_rect(0, 0, 1200, 620, '#eef1f4')
    text(32, 36, 'CYD WEATHER LINK / INTERFACE STUDY 02', 11, weight='700')
    text(30, 79, 'A watch you can see. A warning you can hear.', 32, '#12283a', '600')
    text(32, 106, 'Full-screen alert colors · Optional sound · Your local readings stay in place.', 15)
    states = [
        (32, 'normal', '01 / NORMAL', 'Your everyday weather', 'Navy background while no alerts are active.', 'Sound status remains visible in the header.'),
        (426, 'watch', '02 / NWS WATCH', 'Orange means watch', 'A watch changes the entire screen theme.', 'A short chime announces a new watch.'),
        (820, 'warning', '03 / NWS WARNING', 'Red means warning', 'A warning overrides every concurrent watch.', 'A distinct tone sounds; tap to read or silence.'),
    ]
    for x, kind, label, heading, line1, line2 in states:
        text(x, 147, label, 11, weight='700')
        add_rect(x-5, 164, 358, 271, '#1c2b3b', 9)
        panel = deepcopy(screens[1] if kind == 'warning' else screens[0])
        panel.set('transform', f'translate({x} 169) scale(1.0875)')
        colors = ({'#080f1b': '#944000', '#111e2d': '#793400', '#a2b4c9': '#ffe5cf', '#233246': '#bc7950', '#314c63': '#d28c5b', '#61d8ed': '#b8f1ff', '#83e2b1': '#c4ffd6', '#102d29': '#622a00', '#244a40': '#cf8b53'} if kind == 'watch' else
                  {'#080f1b': '#981f32', '#111e2d': '#7d192b', '#a2b4c9': '#ffe1e5', '#233246': '#b96877', '#314c63': '#d18390', '#61d8ed': '#b8f1ff', '#83e2b1': '#c4ffd6', '#592525': '#621223', '#a04b43': '#d18390', '#ffb4a7': '#ffe1e5'} if kind == 'warning' else {})
        for node in panel.iter():
            for attr in ['fill', 'stroke']:
                if node.get(attr) in colors:
                    node.set(attr, colors[node.get(attr)])
            if node.tag == f'{{{NS}}}text' and kind == 'watch':
                if node.text == 'NWS · No active alerts':
                    node.text = '! NWS WATCH · 1 ACTIVE'
                    node.set('font-size', '11')
                    node.set('y', '203')
                elif node.text == 'Checked 42s ago':
                    node.text = 'Severe Thunderstorm Watch'
                    node.set('font-size', '12')
                    node.set('y', '219')
                    node.set('fill', '#fff3e5')
        audio = ET.SubElement(panel, f'{{{NS}}}text', {'x': '150', 'y': '15', 'font-size': '8', 'fill': '#ffe5cf' if kind == 'watch' else '#ffe1e5' if kind == 'warning' else '#a2b4c9'})
        audio.text = 'Sound on'
        if kind == 'watch':
            age = ET.SubElement(panel, f'{{{NS}}}text', {'x': '16', 'y': '230', 'font-size': '8', 'fill': '#ffe5cf'})
            age.text = 'Checked 42s ago · Tap for details'
        sheet.append(panel)
        text(x, 473, heading, 18, '#12283a', '600')
        text(x, 500, line1)
        text(x, 520, line2)
    add_rect(32, 548, 1136, 1, '#ccd5df')
    text(32, 575, '320 × 240 LANDSCAPE · ORIGINAL WEATHER GRAPHICS · OPTIONAL SPEAKER + AMPLIFIER', 10, weight='600')
    text(32, 597, 'Fictional alerts and readings · Proposed behavior only · Silence stops sound; the warning remains visible.', 11)
    dest = CONCEPTS / 'home-screen-v2.svg'
    ET.ElementTree(doc).write(dest, encoding='unicode')
    render(dest, dest.with_suffix('.png'), 1800)


if __name__ == '__main__':
    ASSETS.mkdir(parents=True, exist_ok=True)
    entries = icons()
    for name, (label, shape) in entries.items():
        source = ASSETS / f'{name}.svg'
        source.write_text(svg(shape, title=label))
        render(source, source.with_suffix('.png'), 48)
    icon_sheet(entries)
    alert_preview()
    print('Generated 12 SVG/PNG icon pairs and two concept sheets.')
