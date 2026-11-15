import os
import re
from convert_code_to_latex import convert_code_to_tex
def chapter_key(name):
    m = re.match(r'(\d+)', name)
    return int(m.group(1)) if m else float('inf')

base = os.getcwd()
chap_dir = os.path.join(base, "chaps")
chapters_tex = os.path.join(base, "chapters.tex")
     
files = [
    f for f in os.listdir(chap_dir)
    if f.endswith(".tex")
    and f[0].isnumeric()
    and not f.endswith("~")
    and os.path.isfile(os.path.join(chap_dir, f))
]

files.sort(key=chapter_key)

with open(chapters_tex, "w") as out:
    for f in files:
        out.write(f"\\input{{chaps/{f}}}\n")

#convert all code to latex in the /code/ dir
convert_code_to_tex()
