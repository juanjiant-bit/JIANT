"""Replace only emu/ after checking the matching ADD source and edition."""
import hashlib
import json
import re
import shutil
import sys
from pathlib import Path

HERE=Path(__file__).resolve().parent
def protected_tree(root):
    return {p.relative_to(root).as_posix():hashlib.sha256(p.read_bytes()).hexdigest()
            for p in root.rglob('*') if p.is_file() and p.relative_to(root).parts[0]!='emu'}

def main(manifest_path,pages,compiled):
    manifest=json.loads(Path(manifest_path).read_text());pages=Path(pages);compiled=Path(compiled)
    sha=manifest['emulator_source']
    if sha!=manifest['source'] or (HERE/'EMULATOR').read_text().strip()!=sha:
        raise SystemExit('emulator and validated beta sources differ')
    if (HERE/'EMULATOR_EDITION').read_text().strip()!='ADDITIVE':
        raise SystemExit('expected ADDITIVE edition')
    source=(compiled.parents[1]/'firmware/src/felucca.c').read_text()
    version=re.search(r'^#define FELUCCA_VERSION "([^"]+)"',source,re.M).group(1)
    if not manifest['version'].startswith(version+'-'):
        raise SystemExit('emulator version does not match the beta')
    before=protected_tree(pages)
    shell=(HERE/'emu/index-additive.html').read_text().replace('@VERSION@',version).replace('@PREVIEW@',' (ADDITIVE beta)')
    (compiled/'index.html').write_text(shell)
    shutil.copy(HERE/'emu/worklet.js',compiled/'worklet.js')
    destination=pages/'emu'
    if destination.exists():shutil.rmtree(destination)
    shutil.copytree(compiled,destination)
    (destination/'SOURCE_COMMIT').write_text(sha+'\n')
    (destination/'EDITION').write_text('ADDITIVE\n')
    if before!=protected_tree(pages):raise SystemExit('files outside emu changed')
    print('Verified: only emulator files changed; installers and firmware preserved.')
    print('Emulator:',version,sha)

if __name__=='__main__':main(*sys.argv[1:])
