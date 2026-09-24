#!/usr/bin/env python3

import re
from pathlib import Path

CHAPS_DIR = Path("chaps")
OUTPUT_FILE = Path("table-of-contents.tex")

chapter_pattern = re.compile(
    r"\\newsection\s*\{([^}]*)\}"
)

chapters = []

for tex_file in sorted(CHAPS_DIR.glob("*.tex")):
    content = tex_file.read_text(encoding="utf-8")

    for match in chapter_pattern.finditer(content):
        chapter_name = match.group(1).strip()
        chapters.append(chapter_name)

with OUTPUT_FILE.open("w", encoding="utf-8") as f:
    f.write("% Auto-generated. Do not edit.\n\n")
    f.write(r"""
    \texthg{\textsc{\underline{Table of contents}}}







    
    \begin{enumerate}
    """)
    
    for chapter in chapters:
        f.write(r"\item \underline{\textlg{\hyperlink{%s}{%s}}}" % (chapter, chapter))
    f.write(r"\end{enumerate}"
  )

with open(Path("main.tex"), 'w') as f:
  f.write(r"""
  %-*-latex-*-
\input{thispreamble.tex}
\input{table-of-contents.tex}
\input{chapters.tex}
\input{thispostamble.tex}

  """)
print(
    f"Wrote {len(chapters)} chapter links to "
    f"{OUTPUT_FILE}"
)
