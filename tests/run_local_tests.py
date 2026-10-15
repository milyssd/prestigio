"""Local regression checks, including builds with NDEBUG (Release assertions disabled)."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
cxx=os.environ.get('CXX','g++')
cases=[('equipment',['tests/equipment_storage_test.cpp']),('loader',['tests/module_loader_test.cpp','src/PrestigioHollowLoader.cpp']),('languages',['tests/languages_test.cpp']),('skills',['tests/permanent_skills_test.cpp']),('npc',['-Itests/npc_stubs','tests/npc_navigation_test.cpp'])]
with tempfile.TemporaryDirectory(prefix='prestigio-tests-') as temp:
    for release in (False,True):
        for name,inputs in cases:
            target=Path(temp)/(name+('-release' if release else '-debug'))
            subprocess.run([cxx,'-std=c++20','-Wall','-Wextra','-Werror']+(['-DNDEBUG'] if release else [])+inputs+['-o',str(target)],cwd=root,check=True)
            subprocess.run([str(target)],check=True)
    for script in ['tests/starting_outfit_test.py','tests/prestige_flow_test.py']:
        subprocess.run(['python3',script],cwd=root,check=True)
