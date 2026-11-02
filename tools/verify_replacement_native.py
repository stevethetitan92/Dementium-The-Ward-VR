"""Verify new hook evidence against the installed executable as inert bytes."""
from pathlib import Path
import hashlib,json,struct
exe=Path('C:/Program Files (x86)/Steam/steamapps/common/Dementium - The Ward/Dementium.exe')
data=exe.read_bytes();pe=struct.unpack_from('<I',data,60)[0];count=struct.unpack_from('<H',data,pe+6)[0];optional=struct.unpack_from('<H',data,pe+20)[0]
sections=[struct.unpack_from('<8sIIII',data,pe+24+optional+40*i)for i in range(count)]
def offset(rva):return next(raw+rva-va for name,virtual,va,size,raw in sections if va<=rva<va+size)
guards={0x80110:'40 53 56 57 48 83 ec 20 8d 41 9c 8b f1 3d f3 01',
        0x807b0:'48 89 5c 24 08 48 89 6c 24 10 48 89 74 24 18 57',
        0xa7ce0:'48 89 5c 24 08 57 48 83 ec 20 48 8b 05 3f d6 2f'}
for rva,expected in guards.items():
    expected=bytes.fromhex(expected);assert data[offset(rva):offset(rva)+len(expected)]==expected,hex(rva)
assert struct.unpack_from('<Q',data,offset(0x30bc78+0x30))[0]==0x1400807b0
assert data[offset(0xa809e):offset(0xa809e)+7]==bytes.fromhex('8b c8 e8 6b 80 fd ff')
assert data[offset(0xa80ad):offset(0xa80ad)+8]==bytes.fromhex('48 8b cb e8 2b fc ff ff')
report={'executable_sha256':hashlib.sha256(data).hexdigest(),'guards':{hex(k):v for k,v in guards.items()},
        'pickup_render_vtable':'0x30bc78 + 0x30 -> 0x807b0','native_apply_remove_pair':'0xa809e..0xa80b0',
        'verification':'Static executable evidence only; no game process or native game function was executed'}
out=Path(__file__).resolve().parents[1]/'weapon-audit/Replacement-native-evidence.json';out.write_text(json.dumps(report,indent=2));print('PASS: pickup adapter signatures, vtable and original apply/remove call sites; game not launched')
