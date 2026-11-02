"""Build inventory icon tiles from retail item textures; never write the archive."""
from pathlib import Path
import json, struct, zlib
from PIL import Image, ImageDraw, ImageOps

ROOT = Path(__file__).resolve().parents[1]
WORKSPACE = ROOT.parents[1]
OUT = ROOT/'assets/inventory/key-icons'
OUT.mkdir(exist_ok=True)
index = json.loads((WORKSPACE/'work/archive-index.json').read_text())
archive = Path('C:/Program Files (x86)/Steam/steamapps/common/Dementium - The Ward/rom.zar')
sources = {32:'keys01',8:'notepad',33:'keycard1',34:'keys01',36:'keys02',37:'keys03',38:'keys04',40:'record1',45:'glove',42:'photo01',43:'photo02',44:'photo03',41:'knob01',46:'notes01'}
provenance = []
with archive.open('rb') as stream:
    for item, name in sources.items():
        offset, length = index[f'3d/tex/{name}.tex']
        stream.seek(offset)
        data = stream.read(length)
        dimensions = struct.unpack_from('<I',data,8)[0]
        w,h = dimensions&65535,dimensions>>16
        pixels = zlib.decompress(zlib.decompress(data[40:])[12:])
        assert len(pixels)>=w*h*4
        icon = Image.frombytes('RGBA',(w,h),pixels[:w*h*4])
        treatment = 'retail texture, aspect preserved'
        if name.startswith('keys'):
            # Texture is a flattened key face, not an alpha-cutout sprite.
            mask=Image.new('L',(w,h));d=ImageDraw.Draw(mask)
            d.ellipse((1,1,14,10),fill=255);d.rectangle((7,7,10,29),fill=255)
            d.rectangle((2,19,8,22),fill=255);d.rectangle((3,26,8,29),fill=255)
            d.ellipse((4,4,11,7),fill=0)
            icon.putalpha(mask)
            treatment='retail key face with silhouette mask; small key uses smaller iron-key icon'
        elif name=='record1':
            mask=Image.new('L',(w,h));ImageDraw.Draw(mask).ellipse((1,1,w-2,h-2),fill=255);icon.putalpha(mask)
        elif name=='knob01':
            # The archive holds UV patches, not a finished inventory sprite.
            # Construct a knob silhouette from its actual round brass mounting plate.
            plate=icon.crop((0,32,32,64)).resize((48,48),Image.Resampling.LANCZOS)
            mask=Image.new('L',(48,48));ImageDraw.Draw(mask).ellipse((1,1,46,46),fill=255);plate.putalpha(mask)
            icon=Image.new('RGBA',(80,64));icon.alpha_composite(plate,(2,8))
            d=ImageDraw.Draw(icon);d.polygon([(24,29),(54,21),(64,30),(31,40)],fill=(93,84,48,255))
            for r in range(18,0,-1):
                v=(18-r)/18
                d.ellipse((58-r,28-r,58+r,28+r),fill=(int(83+64*v),int(78+57*v),int(43+31*v),255))
            treatment='constructed door-knob icon using retail brass plate and shaded knob; not a recovered native sprite'
        elif name=='notes01':
            icon=icon.crop((0,0,32,64))
        bbox=icon.getbbox()
        assert bbox
        icon=ImageOps.contain(icon.crop(bbox),(82,40 if item==32 else 50),Image.Resampling.LANCZOS)
        tile=Image.new('RGBA',(100,56));tile.alpha_composite(icon,((100-icon.width)//2,(56-icon.height)//2))
        tile.save(OUT/f'{item}.png')
        provenance.append({'id':item,'source':f'rom.zar/3d/tex/{name}.tex','treatment':treatment})
(OUT/'provenance.json').write_text(json.dumps(provenance,indent=2)+'\n')
print('Built fourteen icon-only key-item tiles')

