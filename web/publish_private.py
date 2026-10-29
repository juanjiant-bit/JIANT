#!/usr/bin/env python3
"""Replace only preview/ in a known published Pages artifact; preserve every public byte."""
import hashlib
import json
import shutil
import sys
from pathlib import Path
import make_site

PUBLIC = {
    'felucca-v0.9.9-par10-30f05b3.fwsc':'f8aeb1bdcd77320fc2773231e348e33bdfe46e2c8a85027336155c560d88fb7c',
    'felucca-v0.9.9-par13b-3641c02-preview.fwsc':'b64cbbf3b99c40c92aec51e2441ade9fadc0ce2e4a65ef2420684e74ec28257f',
    'felucca-v0.9.9-par12-527c7e4.fwsc':'f30772c976f23ea8fb0fd6c4630a4e0a868f629ac5daf0fa2e5ac95e0f332e5b',
}
def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def public_tree(root):
    return {p.relative_to(root).as_posix():digest(p) for p in root.rglob('*')
            if p.is_file() and p.relative_to(root).parts[0] != 'preview'}

def main(manifest_path, pages, package):
    manifest=json.loads(Path(manifest_path).read_text());pages=Path(pages);package=Path(package)
    sha=manifest['source'];version=manifest['version']
    if make_site.HERE.joinpath('PREVIEW').read_text().strip()!=sha:
        raise SystemExit('PREVIEW does not match the validated source')
    if not version.endswith('-'+sha[:7]+'-preview'):
        raise SystemExit('version does not identify the validated source')
    if digest(package)!=manifest['package_sha256']:
        raise SystemExit('candidate package digest mismatch')
    for name,want in PUBLIC.items():
        if digest(pages/'firmware'/name)!=want:
            raise SystemExit('frozen public package mismatch: '+name)
    before=public_tree(pages)
    output=pages.parent/'new-private-site'
    if output.exists():shutil.rmtree(output)
    make_site.main(package,version,output,pages/'preview/firmware')
    versions=json.loads((output/'firmware/versions.json').read_text())
    if versions[0]['version']!=version or not versions[0].get('edition','').startswith('ADDITIVE') or versions[0]['status']!='beta':
        raise SystemExit('new private edition must be ADDITIVE beta')
    if any('par14' in v['version'] or v.get('edition','').startswith('SAMPLE') for v in versions):
        raise SystemExit('unexpected SAMPLE/par14 firmware in private installer')
    destination=pages/'preview'
    if destination.exists():shutil.rmtree(destination)
    destination.mkdir()
    for name in ['webapp','firmware']:shutil.copytree(output/name,destination/name)
    fw=destination/'firmware';(fw/'SOURCE_COMMIT').write_text(sha+'\n')
    (fw/'SHA256SUMS').write_text(''.join(f'{digest(p)}  {p.name}\n' for p in sorted(fw.glob('*.fwsc'))))
    if public_tree(pages)!=before:
        raise SystemExit('files outside preview changed: refusing deployment')
    print('Verified: public site, public firmware and frozen SAMPLE emulator are byte-identical.')
    print('Private ADDITIVE:',version)

if __name__=='__main__':
    main(*sys.argv[1:])
