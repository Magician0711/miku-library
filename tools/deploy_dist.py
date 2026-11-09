# Miku Library - Copyright (C) 2026 Magician0711
# SPDX-License-Identifier: GPL-3.0-only

"""Builds dist/MikuLibrary from the current build: the program and only what it loads.

Usage: python tools/deploy_dist.py [target folder]
"""

from pathlib import Path
import os, shutil, subprocess, sys, tempfile

PROJECT = Path(__file__).resolve().parents[1]
TARGET = Path(sys.argv[1]) if len(sys.argv) > 1 else PROJECT / 'dist/MikuLibrary'
QT = Path(os.environ.get('QT_DIR', r'C:\Qt\6.8.3\msvc2022_64'))
VCPKG = Path(os.environ.get('VCPKG_ROOT', r'C:\vcpkg')) / 'installed/x64-windows/bin'
CRT = Path(r'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Redist\MSVC\14.51.36231\x64\Microsoft.VC145.CRT')

# Direct3D 12 shader compilers: the interface runs on Direct3D 11 with precompiled shaders.
UNUSED_FILES = {'dxcompiler.dll', 'dxil.dll', 'Qt6Svg.dll', 'qsvg.dll', 'qsvgicon.dll', 'd3dcompiler_47.dll', 'opengl32sw.dll'}
UNUSED_FOLDERS = {'qmltooling', 'generic', 'iconengines'}
# Beside the program so a clean Windows runs it without the Visual C++ installer.
RUNTIME = ['msvcp140.dll', 'msvcp140_1.dll', 'msvcp140_2.dll', 'vcruntime140.dll', 'vcruntime140_1.dll']
VCPKG_PATTERNS = ['torrent-rasterbar.dll', 'boost_*.dll', 'libcrypto-3-x64.dll', 'libssl-3-x64.dll', 'datachannel.dll', 'juice.dll', 'legacy.dll']


def main():
    exe = PROJECT / 'build/MikuLibrary.exe'
    with tempfile.TemporaryDirectory() as scratch:
        stage = Path(scratch) / 'MikuLibrary'
        stage.mkdir()
        shutil.copy2(exe, stage)
        env = dict(os.environ, PATH=f'{QT / "bin"};{VCPKG};{os.environ["PATH"]}')
        subprocess.run([str(QT / 'bin/windeployqt.exe'), '--release', '--qmldir', str(PROJECT / 'qml'), '--no-translations',
                        '--no-opengl-sw', '--no-system-d3d-compiler', '--no-compiler-runtime', str(stage / 'MikuLibrary.exe')],
                       check=True, env=env, stdout=subprocess.DEVNULL)
        for folder in UNUSED_FOLDERS:
            shutil.rmtree(stage / folder, ignore_errors=True)
        for file in list(stage.rglob('*')):
            # Type descriptions only serve design tools.
            if file.is_file() and (file.name in UNUSED_FILES or file.suffix == '.qmltypes'):
                file.unlink()
        for pattern in VCPKG_PATTERNS:
            for dll in VCPKG.glob(pattern):
                shutil.copy2(dll, stage)
        legacy = VCPKG.parent / 'bin/legacy.dll'
        if legacy.is_file():
            shutil.copy2(legacy, stage)
        for name in RUNTIME:
            shutil.copy2(CRT / name, stage)
        shutil.copytree(PROJECT / 'config', stage / 'config')
        shutil.copy2(PROJECT / 'LICENSE', stage)
        (stage / 'qt.conf').write_text('[Paths]\nPrefix=.\nPlugins=.\nQmlImports=qml\n', encoding='utf-8')

        if TARGET.exists():
            shutil.rmtree(TARGET)
        shutil.copytree(stage, TARGET)
    total = sum(f.stat().st_size for f in TARGET.rglob('*') if f.is_file())
    print(f'{TARGET}: {total / 1024 ** 2:.1f} MiB in {sum(1 for f in TARGET.rglob("*") if f.is_file())} files')


if __name__ == '__main__':
    main()
