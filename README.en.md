# Genumi

Local chatbot for the **NumWorks** calculator (`.nwa` app), written in C for Epsilon's EADK.

Genumi answers questions from a knowledge base (science, history, geography, literature, STI2D, middle and high school), does step-by-step calculations, percentages, French-English translation, and offers a typing practice mode.

## Installation

1. Download the `.nwa` file from the [Releases](../../releases) section.
2. Open *Extra apps* on the NumWorks website or on the calculator and select the file.

Two variants are provided:
- **Genumi_visible.nwa**: with the name and the coloured icon;
- **Genumi_invisible.nwa**: with a white icon and no name.

## Quick use

- Type your question and press **EXE**: `c'est quoi une molécule ?`
- Calculations with steps: `(2+3)*4`, `2(3+4)`, `7/2`
- Percentages: `15 % de 240`, `36 sur 240`
- Translation: `traduis bonjour en anglais`
- Multiplication table: `table de 7`
- Typing practice: press the **×10^x** key (the input bar must be empty), type the displayed sentence, then EXE. Press ×10^x again to quit.

## Build

The `.nwa` is built automatically by **GitHub Actions** (workflow `build-nwa`).

Locally, with the ARM toolchain and `nwlink`:

```sh
python3 tools/build_kb.py kb/qa.txt
make VARIANT=visible      # output/visible/Genumi_visible.nwa
make VARIANT=invisible    # output/invisible/Genumi_invisible.nwa
```

## Knowledge base

- `kb/qa.txt` and `kb/extra*.txt`: one entry per line, format `question | answer | keywords`.
- `kb/dict_fr_en.tsv`: French-English dictionary, one pair per line (`french|english`).
- `tools/build_kb.py` generates `src/kb_data.h`, `src/kb_words.h` and `src/dict_data.h`.

## Limits

Genumi does not understand the meaning of sentences: it answers from keywords and its knowledge base. It does not solve new exercises and does not replace a teacher.

## Credits

See [CREDITS.md](CREDITS.md).

## License

GPL-3.0: Genumi reuses code from NumPlay, which is distributed under this license. See the LICENSE file.
