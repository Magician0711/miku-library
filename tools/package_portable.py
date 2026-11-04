from pathlib import Path
import ast,datetime,hashlib,json,os,shutil,struct,subprocess,time,zipfile

PROJECT=Path(__file__).resolve().parents[1]
OUT=PROJECT.parent/'release_packages'
OUT.mkdir(exist_ok=True)
now=datetime.datetime.now(datetime.timezone.utc).astimezone(datetime.timezone(datetime.timedelta(hours=9)))
NAME='AnimeNexus-Windows-x64-Portable-'+now.strftime('%Y%m%d-%H%M%S')
STAGE=OUT/NAME/'AnimeNexus'
ARCHIVE=OUT/(NAME+'.zip')
VERIFY=OUT/(NAME+'-verification')
CRT=Path(r'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Redist\MSVC\14.51.36231\x64\Microsoft.VC145.CRT')
QT=Path(r'C:\Qt\6.8.3\msvc2022_64')
OLD=PROJECT
if STAGE.exists() or ARCHIVE.exists() or VERIFY.exists():raise RuntimeError('Use a fresh output name')
STAGE.mkdir(parents=True)

def copy(src,dst):
    dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
def write(dst,text):
    dst.parent.mkdir(parents=True,exist_ok=True);dst.write_text(text,encoding='utf-8',newline='\n')
def digest(path):
    with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()

# Keep a clean distributable, with only the current executable and usable plugins.
for file in (PROJECT/'dist/AnimeNexus').rglob('*'):
    if not file.is_file():continue
    rel=file.relative_to(PROJECT/'dist/AnimeNexus')
    if file.suffix.lower() not in ('.dll','.json') and rel.as_posix()!='AnimeNexus.exe':continue
    if rel.parts[0]=='sqldrivers' and file.name!='qsqlite.dll':continue
    copy(file,STAGE/rel)
for file in CRT.glob('*.dll'):copy(file,STAGE/file.name)
write(STAGE/'qt.conf','[Paths]\nPrefix=.\nPlugins=.\n')

# Include the matching source and notices, never the user's profile or history.
for directory in ('src','tests','assets','config','tools'):
    shutil.copytree(PROJECT/directory,STAGE/'Source'/directory)
for name in ('CMakeLists.txt','build.bat','README.md','UI_REWRITE.md','LICENSE','.gitignore','.gitattributes'):
    copy(PROJECT/name,STAGE/'Source'/name)
copy(PROJECT/'LICENSE',STAGE/'LICENSE')
# The README links into docs/; ship all of it so those links work offline.
shutil.copytree(PROJECT/'docs',STAGE/'Source'/'docs')
readme=(STAGE/'Source/README.md').read_text(encoding='utf-8-sig')
write(STAGE/'Source/README.md','> 此目录为对应源码。无需编译即可使用压缩包上层的 `AnimeNexus.exe`。\n\n'+readme)
for path in (STAGE/'config/anime_sites.json',STAGE/'Source/config/anime_sites.json'):
    catalog=json.loads(path.read_text(encoding='utf-8-sig'));catalog.pop('origin_conversation_id',None)
    write(path,json.dumps(catalog,ensure_ascii=False,indent=2)+'\n')
for package in (OLD/'licenses').iterdir():
    if package.is_dir() and package.name not in {'DB-IP','NaturalEarth','libmaxminddb'}:
        shutil.copytree(package,STAGE/'licenses'/package.name)
write(STAGE/'licenses/Microsoft/NOTICE.txt','Microsoft Visual C++ Runtime (x64)\nApp-local runtime DLLs are included beside AnimeNexus.exe.\nOriginal Microsoft redistribution terms apply.\nhttps://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files\n')
write(STAGE/'THIRD_PARTY_NOTICES.txt','Qt 6.8.3 (including Concurrent), libtorrent 2.1.2, OpenSSL, Boost and their runtime dependencies are bundled as dynamically linked libraries. Original third-party licenses and Qt SPDX notices are in licenses/. Corresponding application source is in Source/.\n')
write(STAGE/'使用说明.txt','''Anime Nexus · Windows x64 便携版

1. 将压缩包完整解压到任意文件夹。
2. 双击同目录 AnimeNexus.exe，首次运行会自动建立下载目录。
3. 不要只复制主程序，DLL 和插件文件夹需要一起保留。

已包含 Qt、内置 libtorrent 下载引擎、OpenSSL 和 VC++ 本地运行库。
无需安装 Python、Qt SDK 或其他下载器。
默认使用内置下载器；上传总限速 1024 KiB/s，其余速率和任务数量不限。
默认下载完成后停止传输。

可在侧栏底部、下载任务页或设置中切换到已安装的外部下载器。
外部软件需自行安装；内置下载器可以直接使用。
搜索、封面和资源检索需要网络。

个人配置、追番和下载记录由软件在当前用户的 .anime-nexus 文件夹中创建。
本包不含打包者的个人配置、下载记录、影片、缓存或旧版程序。
Source/ 为对应源码和构建说明，日常运行不需要打开该目录。
''')

# Audit normal and delay-load imports. VC runtime imports must be bundled,
# even if this build machine already has a global redistributable installed.
from pe_imports import imported_dlls as imports
system=Path(os.environ.get('SystemRoot',r'C:\Windows'))/'System32'
root_dlls={p.name.lower():p for p in STAGE.glob('*.dll')}
missing=set();binaries=[]
for file in STAGE.rglob('*'):
    if file.suffix.lower() not in ('.exe','.dll'):continue
    resolutions={}
    for name in imports(file):
        if name in root_dlls:resolutions[name]='bundled-root'
        elif (file.parent/name).is_file():resolutions[name]='bundled-neighbor'
        elif name.startswith(('api-ms-','ext-ms-')):resolutions[name]='windows-api-set'
        elif name.startswith(('msvcp','vcruntime','concrt','vccorlib')):
            resolutions[name]='MISSING';missing.add(name)
        elif (system/name).is_file():resolutions[name]='windows-system'
        else:resolutions[name]='MISSING';missing.add(name)
    binaries.append({'binary':file.relative_to(STAGE).as_posix(),'imports':resolutions})
if missing:raise RuntimeError('Missing runtime DLLs: '+', '.join(sorted(missing)))
write(STAGE/'DEPENDENCY_REPORT.json',json.dumps({'platform':'Windows x64','unresolved':[],'binaries':binaries},indent=2)+'\n')
assert digest(PROJECT/'build/AnimeNexus.exe')==digest(STAGE/'AnimeNexus.exe')
for file in STAGE.rglob('*'):
    if file.is_file() and file.suffix.lower() in ('.cpp','.h','.md','.json','.txt','.bat'):
        content=file.read_text(encoding='utf-8-sig',errors='replace')
        if os.environ.get('USERNAME') and os.environ['USERNAME'] in content:
            raise RuntimeError('Personal identifier in package: '+str(file.relative_to(STAGE)))
manifest={'name':NAME,'created':now.isoformat(),'platform':'Windows x64','files':[]}
for file in sorted(STAGE.rglob('*')):
    if file.is_file():manifest['files'].append({'path':file.relative_to(STAGE).as_posix(),'bytes':file.stat().st_size,'sha256':digest(file)})
write(STAGE/'PACKAGE_MANIFEST.json',json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
with zipfile.ZipFile(ARCHIVE,'x',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for file in sorted(STAGE.rglob('*')):
        if file.is_file():z.write(file,'AnimeNexus/'+file.relative_to(STAGE).as_posix())
VERIFY.mkdir()
with zipfile.ZipFile(ARCHIVE) as z:
    if z.testzip() is not None:raise RuntimeError('ZIP integrity check failed')
    z.extractall(VERIFY)
unpacked=VERIFY/'AnimeNexus'
for item in manifest['files']:
    p=unpacked/item['path']
    if p.stat().st_size!=item['bytes'] or digest(p)!=item['sha256']:raise RuntimeError('Extracted content mismatch: '+item['path'])

# Start the actual unpacked executable using only Windows PATH entries, a clean
# per-child profile, and the bundled offscreen platform. No real task is loaded.
profile=VERIFY/'全新 用户配置'
state=profile/'.anime-nexus';state.mkdir(parents=True)
downloads=profile/'Downloads';downloads.mkdir()
# The first-run guide is modal; accept it so startup reaches the main window.
config={'disclaimer_accepted':1,'engine':{'save_path':str(downloads),'bind_address':'127.0.0.1','listen_port':0,'dht':False,'lsd':False,'upnp':False},'paths':{'download_dir':str(downloads)}}
write(state/'config.json',json.dumps(config,ensure_ascii=False))
env=os.environ.copy()
for key in list(env):
    if key.upper().startswith(('QT_','QML')) or key.upper() in {'QTDIR','CMAKE_PREFIX_PATH','VCPKG_ROOT'}:env.pop(key,None)
env['PATH']=str(system)+';'+str(system.parent)
env['ANIME_NEXUS_DATA_DIR']=str(state)
env['QT_QPA_PLATFORM']='windows'
env['QT_PLUGIN_PATH']=str(unpacked)
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
stderr=VERIFY/'startup-stderr.txt'
with stderr.open('wb') as output:
    process=subprocess.Popen([str(unpacked/'AnimeNexus.exe')],cwd=unpacked,env=env,stdout=output,stderr=output,startupinfo=startup)
    try:
        limit=time.monotonic()+15
        while time.monotonic()<limit:
            if process.poll() is not None:raise RuntimeError('Unpacked application exited: '+str(process.returncode)+' '+stderr.read_text(errors='replace')[:1000])
            if (state/'local-library.json').is_file() and (state/'anime_nexus.db').is_file():break
            time.sleep(.2)
        else:raise RuntimeError('Isolated startup did not reach the main window')
        time.sleep(1)
        if process.poll() is not None:raise RuntimeError('Unpacked application crashed during startup')
        log=(state/'anime_nexus_cpp.log').read_text(encoding='utf-8-sig',errors='replace')
        if 'catalog loaded:' not in log:raise RuntimeError('Packaged site catalog did not load')
        startup_result={'status':'PASS','profile':'isolated','path':'Windows system directories only','platform':'bundled native Windows Qt plugin','catalog_loaded':True,'database_created':True,'local_inventory_created':True}
    finally:
        if process.poll() is None:process.terminate()
        process.wait(timeout=10)
write(OUT/(NAME+'-startup.json'),json.dumps(startup_result,ensure_ascii=False,indent=2)+'\n')
sha=digest(ARCHIVE);write(OUT/(ARCHIVE.name+'.sha256'),sha+'  '+ARCHIVE.name+'\n')
result={'archive':str(ARCHIVE),'bytes':ARCHIVE.stat().st_size,'MiB':round(ARCHIVE.stat().st_size/1024**2,2),'files':len(manifest['files'])+1,'sha256':sha,'dependency_audit':'PASS','zip_and_extracted_hashes':'PASS','isolated_startup':'PASS','startup_report':str(OUT/(NAME+'-startup.json'))}
write(OUT/(NAME+'-report.json'),json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(result,ensure_ascii=False,indent=2),flush=True)
