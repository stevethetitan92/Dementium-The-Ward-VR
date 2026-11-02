"""Build reference-layout artwork from retained game icons and HelpMe type.

Live map, item contents, counters and focus are deliberately runtime layers.
This image is not a mock claim that key items or map data have been connected.
"""
from pathlib import Path
import json
import re
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageOps, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / 'assets/inventory'
header = (ROOT / 'src/inventory_layout.hpp').read_text()
body = header.split('slots{{', 1)[1].split('}};', 1)[0]
map_rect = tuple(map(int, re.search(r'Rect map\{([^}]+)', header)[1].split(',')))
rects = []
for entry in re.finditer(r'\{([\d,]+)\}|\bmap\b', body):
    rects.append(tuple(map(int, entry[1].split(','))) if entry[1] else map_rect)
assert len(rects) == 24
random = np.random.default_rng(271028)
y, x = np.mgrid[:900, :1000]
noise = random.normal(0, 2.3, (900, 1000))
# Brushed, scratched steel with irregular dirt; no radial black vignette.
coarse=Image.fromarray(random.integers(0,255,(28,32),dtype='uint8')).resize((1000,900),Image.Resampling.BICUBIC).filter(ImageFilter.GaussianBlur(9))
stains=(np.asarray(coarse).astype(float)-128)*.25
brush=random.normal(0,1.5,(900,1))
base=np.clip(112+noise+stains+brush,0,255)
canvas=Image.fromarray(np.stack([base*.99,base,base*1.015],axis=2).clip(0,255).astype('uint8')).convert('RGBA')
wear=ImageDraw.Draw(canvas)
for i in range(500):
 sx=int(random.integers(0,1000));sy=int(random.integers(0,900));length=int(random.integers(2,40));c=int(random.integers(65,150))
 wear.line((sx,sy,sx+length,sy+int(random.integers(-2,3))),fill=(c,c,c,255))

draw = ImageDraw.Draw(canvas)
font = lambda size: ImageFont.truetype(str(ASSETS/'HelpMe.ttf'), size)
def label(text, xy, size=25, fill=(226,222,213,255)):
    draw.text(xy, text, font=font(size), fill=fill, stroke_width=0)
label('Inventory', (385, 9), 32)
label('Pills', (30, 55), 19)
label('Map', (180, 48), 26)
label('Key items', (760, 48), 26)
label('Weapons', (30, 582), 26)
for index, (rx,ry,rw,rh) in enumerate(rects):
    draw.rounded_rectangle((rx,ry,rx+rw-1,ry+rh-1), radius=8,
                           fill=(27,30,31,255), outline=(17,19,19,255), width=2)
    draw.line((rx+10,ry+2,rx+rw-10,ry+2), fill=(91,91,87,255), width=1)
names = ['revolver','pistol','shotgun','rifle','sniper','saw','nightstick','flashlight']
titles = ['Revolver','Pistol','Shotgun','M16','Hunting rifle','Buzzsaw','Nightstick','Flashlight']
for index, name in enumerate(names):
    rx,ry,rw,rh = rects[index]
    icon = Image.open(ASSETS/'original-icons'/f'{name}-normal.png').convert('RGBA')
    icon = ImageOps.contain(icon.crop(icon.getbbox()),(rw-22,rh-45),Image.Resampling.LANCZOS)
    canvas.alpha_composite(icon,(rx+(rw-icon.width)//2,ry+28))
    label(titles[index], (rx+10,ry+7), 19)
# Render the same white bottle/red cap mesh used in game at its true aspect.
mesh=np.fromfile(ROOT/'assets/hands/PillBottle.mesh',dtype='<f4').reshape(-1,12).reshape(-1,3,12)
icon=Image.new('RGBA',(320,520));pen=ImageDraw.Draw(icon)
# Body axis X is upright; slight elevated view reveals the red cap.
for tri in sorted(mesh,key=lambda t:float(t[:,2].mean()))[::-1]:
 points=[(160+(v[1]+.032)*8000,270-v[0]*6800+(v[2]+.072)*1700) for v in tri]
 n=tri[:,3:6].mean(axis=0);light=.65+.35*max(0,float(n@np.array([.4,-.3,-.85])))
 rgb=np.clip(tri[:,6:9].mean(axis=0)*light*255,0,255).astype(int)
 pen.polygon(points,fill=tuple(rgb)+(255,))
bottle=ImageOps.contain(icon.crop(icon.getbbox()),(75,105),Image.Resampling.LANCZOS)
canvas.alpha_composite(bottle,(90-bottle.width//2,92))
label('Y Close   Stick Navigate   A Select', (225, 868), 20)
target = ASSETS/'inventory-reference-background.png'
canvas.save(target)
(ASSETS/'inventory-reference-layout.json').write_text(json.dumps({
    'size':[1000,900], 'slots':rects, 'source':'src/inventory_layout.hpp',
    'runtime_layers':['live map','owned key items','pills count','weapon ammo','equipped','selection'],
    'font':'HelpMe.ttf','icons':'original-icons/*-normal.png'
}, indent=2)+'\n')
print(target)
# Keep original numeric glyphs and selection artwork in the right half.
packed = Image.new('RGBA',(2048,1024))
packed.paste(canvas,(0,0))
# Icon-only key items; ownership remains a runtime mask.
items=json.loads((ASSETS/'key-items.json').read_text())['items']
for i,item in enumerate(items):
 tile=Image.open(ASSETS/'key-icons'/f"{item['id']}.png").convert('RGBA')
 assert tile.size==(100,56)
 packed.alpha_composite(tile,((i%7)*140,900+(i//7)*56))

packed.paste(Image.open(ASSETS/'inventory-atlas.png').convert('RGBA'),(1024,0))
pixels = np.asarray(packed).astype('uint32')
words = (pixels[:,:,0] | pixels[:,:,1]<<8 | pixels[:,:,2]<<16 | pixels[:,:,3]<<24).ravel()
starts = np.r_[0,np.flatnonzero(words[1:]!=words[:-1])+1]
ends = np.r_[starts[1:],len(words)]
with (ROOT/'src/inventory_reference_art.hpp').open('w') as out:
    out.write('#pragma once\n#include <cstdint>\nnamespace inventoryReferenceArt {\nstruct Run{unsigned count;std::uint32_t rgba;};\ninline constexpr Run runs[]={\n')
    for start,end in zip(starts,ends): out.write('{%d,0x%08xu},\n'%(end-start,words[start]))
    out.write('};\n}\n')
