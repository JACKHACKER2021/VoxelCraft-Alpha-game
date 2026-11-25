# Otevřený obzor – sandboxový soundtrack VoxelCraftu

Klidná skladba na stavění a toulání po světě: C dur, 72 BPM, asi 2 minuty 8 sekund.
Celá je složená a syntetizovaná v Pythonu (`tools/sandbox_soundtrack.py`, jen numpy a ffmpeg),
bez samplů a bez čehokoli z jiných her. Do hry zatím zapojená není.

| Část | Takty | Co hraje |
|---|---|---|
| Úvod | 1–4 | smyčcový pad, řídké zvonky (Cmaj9 – Fmaj7 – Cmaj9 – Gsus) |
| Téma A | 5–12 | měkký piano rozklad, melodie na piano, basa |
| Téma B | 13–20 | melodie výš na zvonky zdvojená padem (Fmaj7 – Em7 – Dm9 – Gsus …) |
| Přestávka | 21–24 | jen tiché piano akordy a pad |
| Téma A znovu | 25–32 | piano melodie, nad ní zvonky o oktávu výš |
| Závěr | 33–36 | utichající akordy, závěrečné Cmaj9 se dvěma zvonky |

Nástroje: plstěné piano (pár harmonických, vyšší rychleji doznívají), skleněné FM zvonky,
teplý pad z rozladěných pil, měkká sinusová basa, dozvuk velkého sálu (konvoluce).

Hru stačí zkusit tak, že se `otevreny_obzor.mp3` zkopíruje do složky hry `music/`
jako `overworld.mp3` (nebo `plains.mp3`, `forest.mp3` …).

    python3 tools/sandbox_soundtrack.py   # znovu vytvoří otevreny_obzor.mp3
