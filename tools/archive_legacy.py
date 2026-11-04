# Anime Nexus - Copyright (C) 2026 Magician0711
# SPDX-License-Identifier: GPL-3.0-only

"""Archive old work before cleanup. Never deletes source files."""
from pathlib import Path
import datetime, hashlib, json, zipfile

root=Path(__file__).resolve().parents[2]
out=root/'archives';out.mkdir(exist_ok=True)
stamp=datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
archive=out/f'AnimeNexus-Legacy-{stamp}.zip'
targets=[root/'anime_nexus',root/'backups',*list((root/'release_packages').iterdir()),
         *(root/name for name in ('anime_sites_README.md','anime_sites.json','anime_sites.jsonc','vcpkg_libtorrent.log')),
         root/'anime_nexus_cpp/tools/legacy_package_helper.py']
targets=[p.resolve() for p in targets if p.exists()]
files=[]
for target in targets:
    assert target.is_relative_to(root) and target!=root
    files.extend(sorted(p for p in (target.rglob('*') if target.is_dir() else [target]) if p.is_file()))
manifest={'archive':str(archive),'targets':[str(p) for p in targets],'files':[]}
with zipfile.ZipFile(archive,'x',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
    for p in files:
        relative=p.relative_to(root).as_posix()
        digest=hashlib.sha256()
        with p.open('rb') as src,z.open(relative,'w',force_zip64=True) as dst:
            while data:=src.read(1024*1024):digest.update(data);dst.write(data)
        manifest['files'].append({'path':relative,'size':p.stat().st_size,'sha256':digest.hexdigest()})
    z.writestr('ARCHIVE_MANIFEST.json',json.dumps(manifest,ensure_ascii=False,indent=2))
with zipfile.ZipFile(archive) as z:
    for f in manifest['files']:
        with z.open(f['path']) as stream:
            assert hashlib.file_digest(stream,'sha256').hexdigest()==f['sha256'],f['path']
with archive.open('rb') as f:sha=hashlib.file_digest(f,'sha256').hexdigest()
archive.with_suffix('.zip.sha256').write_text(sha+'  '+archive.name+'\n',encoding='utf-8')
manifest['sha256']=sha;manifest['verified']=True
report=archive.with_suffix('.json');report.write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'archive':str(archive),'report':str(report),'files':len(files),'bytes':archive.stat().st_size,'verified':True},ensure_ascii=False),flush=True)
