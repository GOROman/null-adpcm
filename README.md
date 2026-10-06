# null-adpcm

Clean-room emulator of the **OKI MSM6258** ADPCM chip, the X68000's ADPCM sound source, in C++17.

- Integer only, allocation-free, no exceptions / RTTI; multiple independent instances (copy = save state)
- 4-bit OKI ADPCM decoder and encoder, 12-bit output, 49-entry step table generated at compile time
- Register interface as seen on the X68000 (`$E92001` command/status, `$E92003` data), low nibble first,
  data-request callback for DMA feeding, recording path
- `X68kAdpcm` glue: PPI port C bits select 8/4 MHz, 1/1024 / 1/768 / 1/512 (3.9 – 15.6 kHz) and L/R mute
- Tools: `adpcm2wav` (X68000 `.pcm` → WAV, optional resampling), `wav2adpcm` (encoder)
- Companion of [null-opm](https://github.com/GOROman/null-opm) (YM2151). MIT licensed; see `docs/CLEANROOM.md`.

## Build

```sh
cmake --preset default && cmake --build --preset default && ctest --preset default
./build/tools/adpcm2wav --rate 15600 --out-rate 44100 voice.pcm voice.wav
./build/tools/wav2adpcm --rate 15600 voice.wav voice.pcm
```

## Use

```cpp
#include "nulladpcm/msm6258.hpp"
nulladpcm::X68kAdpcm x;                 // or nulladpcm::Msm6258 chip(8000000, Divider::Div512)
x.write_ppi_c(0x08);                     // 8 MHz, 1/512 -> 15625 Hz, both channels
nulladpcm::Callbacks cb; cb.data_request = my_dma_feed; cb.user = &dma;
x.chip.set_callbacks(cb);
x.chip.write_command(nulladpcm::cmd::kPlay);
int16_t lr[2 * 256];
x.generate_stereo(lr, 256);              // at x.chip.sample_rate()
```

`write_data()` supplies one byte (two nibbles); the chip asks for the next byte via
`data_request` when both have been played. A C API (`nulladpcm/msm6258_c.h`) and
pico-sdk / ESP-IDF / Arduino manifests are included.

## Sample rates

| PPI C bit4 (clock) | bits 3..2 (divider) | rate |
|---|---|---|
| 1 (4 MHz) | 00 (1/1024) | 3906 Hz |
| 1 (4 MHz) | 01 (1/768) | 5208 Hz |
| 1 (4 MHz) | 10 (1/512) | 7812 Hz |
| 0 (8 MHz) | 00 (1/1024) | 7812 Hz |
| 0 (8 MHz) | 01 (1/768) | 10416 Hz |
| 0 (8 MHz) | 10 (1/512) | 15625 Hz |

Bit assignments of PPI port C are `TUNED:` assumptions from X68000 documentation; see `docs/CLEANROOM.md`.
