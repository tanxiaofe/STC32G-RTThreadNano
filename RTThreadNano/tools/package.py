"""Package reviewable project sources and current firmware, excluding build debris."""
from pathlib import Path
import xml.etree.ElementTree as ET
from zipfile import ZipFile, ZIP_DEFLATED

root=Path(__file__).resolve().parents[1]
archive=root/'RTThreadNano-STC32G-USB-CDC-FinSH.zip'
assert (root/'build/RTThreadNano.hex').read_bytes()==(root/'release/RTThreadNano-IPC-GUI.hex').read_bytes()
assert (root/'build/RTThreadNano.hex').read_bytes()==(root/'release/RTThreadNano-CDC-FinSH.hex').read_bytes()
project=ET.parse(root/'RTThreadNano.uvproj')
for entry in project.findall('.//FilePath'):
    assert (root/entry.text.replace('\\','/')).is_file(),entry.text

files=[root/name for name in ['README.md','THIRD_PARTY.md','rtconfig.h','RTThreadNano.uvproj']]
for folder in ['app','bsp','components','kernel','port','release','tests','tools']:
    files.extend(p for p in (root/folder).rglob('*') if p.is_file()
                 and '__pycache__' not in p.parts
                 and p.suffix.lower() not in {'.obj','.crf','.lst','.src','.pyc','.exe','.zip','.png'})
files.append(root/'build/build.log')
if (root/'build/.gitkeep').exists():files.append(root/'build/.gitkeep')
with ZipFile(archive,'w',ZIP_DEFLATED,compresslevel=9) as package:
    for p in sorted(set(files)):package.write(p,'RTThreadNano/'+p.relative_to(root).as_posix())
with ZipFile(archive) as package:
    assert package.testzip() is None
    assert len(package.namelist())==len(set(package.namelist()))
    assert package.read('RTThreadNano/release/RTThreadNano-IPC-GUI.hex')==(root/'build/RTThreadNano.hex').read_bytes()
    for entry in project.findall('.//FilePath'):
        path=Path(entry.text.replace('\\','/')).as_posix().removeprefix('./')
        assert 'RTThreadNano/'+path in package.namelist(),path
    print(f'PASS: {len(package.namelist())} files; project references and packaged HEX match current build')
print(f'{archive} ({archive.stat().st_size} bytes)')
