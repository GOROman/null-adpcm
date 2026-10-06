# Clean-room policy

null-adpcm is written from the OKI MSM6258 datasheet (ADPCM algorithm, step
table, command/status bits) and X68000 hardware documentation (PPI port C
assignments for clock, divider and pan). Contributors must not read, copy or
paraphrase the source of any MSM6258 / X68000 emulator (XM6, px68k, MAME
`okim6258`, etc.). Comparing audio output with other emulators or real hardware
is allowed. Constants marked `TUNED:` were fitted against output rather than
taken from a document.
