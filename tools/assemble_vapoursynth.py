# Miku Library - Copyright (C) 2026 Magician0711
# SPDX-License-Identifier: GPL-3.0-only

"""Builds third_party/vapoursynth: the motion interpolation the Windows player ships with.

Usage: python tools/assemble_vapoursynth.py <folder with the three archives>

The archives, all official builds, checked against the hashes their publishers list:
  https://github.com/vapoursynth/vapoursynth/releases/download/R82/VapourSynth64-Portable-R82.zip
  https://www.python.org/ftp/python/3.13.15/python-3.13.15-embed-amd64.zip
  https://pypi.org/project/vapoursynth-mvtools/29/  (vapoursynth_mvtools-29-py3-none-win_amd64.whl)

The result is VapourSynth's own portable layout: embedded Python at the top, VapourSynth
and MVTools under Lib/site-packages, where VapourSynth autoloads plugins. The player
points libmpv at its vsscript.dll through VSSCRIPT_PATH. Debug symbols are left out.
"""

from pathlib import Path
import hashlib, io, shutil, sys, zipfile

PROJECT = Path(__file__).resolve().parents[1]
TARGET = PROJECT / 'third_party/vapoursynth'
ARCHIVES = {
    'VapourSynth64-Portable-R82.zip': 'f97ea50c4c9f778eeac4bc4e3e43f86fb3b71787371d06e584e0c47f396874ca',
    'python-3.13.15-embed-amd64.zip': 'd1f04d990aee1253d8569e8e5104e30fa9f5fa830899f14843448872d936a2cf',
    'vapoursynth_mvtools-29-py3-none-win_amd64.whl': 'd64d6525d88ce52572f93e04598083c7cff2a92bf6bc269168130fd7a5227746',
}
WHEEL = 'wheel/vapoursynth-82-cp312-abi3-win_amd64.whl'
# Only what playback needs: no debug symbols, C headers, type stubs or build metadata.
SKIP_SUFFIXES = ('.pdb', '.pyi')
SKIP_PARTS = ('/include/', '/pkgconfig/')


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    source = Path(sys.argv[1])
    data = {}
    for name, expected in ARCHIVES.items():
        blob = (source / name).read_bytes()
        actual = hashlib.sha256(blob).hexdigest()
        if actual != expected:
            sys.exit(f'{name}: sha256 {actual}, expected {expected}')
        data[name] = zipfile.ZipFile(io.BytesIO(blob))

    if TARGET.exists():
        shutil.rmtree(TARGET)
    TARGET.mkdir(parents=True)
    data['python-3.13.15-embed-amd64.zip'].extractall(TARGET)
    packages = TARGET / 'Lib/site-packages'
    wheels = [zipfile.ZipFile(io.BytesIO(data['VapourSynth64-Portable-R82.zip'].read(WHEEL))),
              data['vapoursynth_mvtools-29-py3-none-win_amd64.whl']]
    for wheel in wheels:
        for item in wheel.infolist():
            if item.filename.endswith(SKIP_SUFFIXES) or any(part in item.filename for part in SKIP_PARTS):
                continue
            wheel.extract(item, packages)
    # The embedded interpreter reads its module path from this file and nothing else.
    (TARGET / 'python313._pth').write_text('python313.zip\n.\nLib\\site-packages\n', encoding='ascii', newline='\r\n')
    total = sum(f.stat().st_size for f in TARGET.rglob('*') if f.is_file())
    print(f'{TARGET}: {total / 1048576:.1f} MiB')


if __name__ == '__main__':
    main()
