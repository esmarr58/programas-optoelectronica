"""Ejecuta el sketch real con Arduino simulado. Requiere MSVC instalado (Windows).
No simula tiempos/circuitos fisicos ni sustituye probar la tarjeta y el sensor.
"""
import json
import os
from pathlib import Path
import subprocess
import shutil
import tempfile

here = Path(__file__).resolve().parent
vswhere = Path(os.environ['ProgramFiles(x86)']) / 'Microsoft Visual Studio/Installer/vswhere.exe'
installs = json.loads(subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires',
    'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-format', 'json'], text=True))
if not installs:
    raise SystemExit('No se encontro MSVC; instalar herramientas C++ de Visual Studio.')
devcmd = Path(installs[0]['installationPath']) / 'Common7/Tools/VsDevCmd.bat'
result = subprocess.check_output(
    f'cmd.exe /d /s /c ""{devcmd}" -arch=x64 -host_arch=x64 >nul && set"', text=True)
env = {key.upper(): value for key, value in os.environ.items()}
for line in result.splitlines():
    if '=' in line and not line.startswith('='):
        key, value = line.split('=', 1)
        env[key.upper()] = value
compiler = shutil.which('cl.exe', path=env['PATH'])
if not compiler:
    raise SystemExit('VsDevCmd no expuso cl.exe en PATH.')
with tempfile.TemporaryDirectory(prefix='opto-p5-host-') as tmp:
    exe = Path(tmp) / 'verify.exe'
    subprocess.run([compiler, '/nologo', '/std:c++17', '/EHsc', '/W4', '/D_CRT_SECURE_NO_WARNINGS',
        '/I' + str(here / 'host'), str(here / 'host/verify.cpp'), '/Fe:' + str(exe),
        '/Fo:' + str(Path(tmp) / 'verify.obj')], env=env, cwd=tmp, check=True)
    subprocess.run([str(exe)], check=True)
