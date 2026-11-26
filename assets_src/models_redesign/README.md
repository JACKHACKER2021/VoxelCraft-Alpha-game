# Redesign tvorů (pig, sheep, chicken, cow, ghost, forgotten_man)
Stejná konvence jako 2.8.0: uzly cube_N, 1 jednotka = 1 blok, Y nahoru, hlava k -Z, textura v souboru (nearest, alphaMode MASK).
- pig, sheep, chicken, cow, ghost: nový model (více krabic) + nová 128x128 textura.
- forgotten_man: původní model a UV, jen nový skin (atlas 128x128, dlaždice 64px).
Pozn.: textury u nových modelů jsou rozbalené per-box (každá krabice má vlastní oblast), ne sdílené dlaždice atlasu.
