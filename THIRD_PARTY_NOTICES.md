# Componenti di terzi

Swingers è distribuito con licenza **AGPL-3.0** ([LICENSE](LICENSE)). Questo file elenca i componenti di terzi usati, con la licenza con cui li usiamo e il punto in cui sta il testo della licenza. I submodule in `vendors/` sono fissati a una versione e non sono modificati.

## Nel plugin e nello standalone

| Componente | Versione | Licenza con cui è usato | Testo della licenza |
|---|---|---|---|
| [JUCE](https://juce.com) | 8.0.4 | AGPLv3 (JUCE è a doppia licenza AGPLv3 / licenza commerciale JUCE; qui si usa l'opzione AGPLv3) | `vendors/JUCE/LICENSE.md` |
| VST3 SDK (copia inclusa in JUCE) | quella di JUCE 8.0.4 | GPLv3 (il SDK è a doppia licenza: Steinberg VST3 proprietaria oppure GPLv3; si usa la GPLv3, compatibile con l'AGPLv3 del progetto) | `vendors/JUCE/modules/juce_audio_processors/format_types/VST3_SDK/LICENSE.txt` |
| [clap-juce-extensions](https://github.com/free-audio/clap-juce-extensions) | commit `7adee3a` (ramo `main`) | MIT, Copyright 2019-2020 Paul Walker | `vendors/clap-juce-extensions/LICENSE.md` |
| [CLAP](https://github.com/free-audio/clap) (header, submodule di clap-juce-extensions) | quello fissato da clap-juce-extensions | MIT, Copyright (c) 2021 Alexandre Bique | `vendors/clap-juce-extensions/clap-libs/clap/LICENSE` |
| [clap-helpers](https://github.com/free-audio/clap-helpers) (submodule di clap-juce-extensions) | quello fissato da clap-juce-extensions | MIT, Copyright (c) 2021 Alexandre Bique | `vendors/clap-juce-extensions/clap-libs/clap-helpers/LICENSE` |
| [Homemade Apple](https://fonts.google.com/specimen/Homemade+Apple) (font della scritta sul nastro, incorporato nel binario) | `HomemadeApple-Regular.ttf` | Apache-2.0, di Font Diner | `resources/fonts/LICENSE-HomemadeApple-Apache-2.0.txt` |

### Componenti dentro JUCE

I moduli di JUCE contengono a loro volta codice di terzi (zlib, libpng, jpeglib, FLAC, Ogg Vorbis, HarfBuzz, SheenBidi, CHOC e altri). L'elenco completo, con licenza e percorso del testo di ciascuno, è nella sezione "The JUCE Framework Dependencies" di `vendors/JUCE/LICENSE.md`. Entra nel binario solo ciò che i moduli JUCE usati da Swingers compilano.

### Note

- Il VST3 SDK della copia di JUCE dichiara, nel proprio `LICENSE.txt`, la scelta tra la licenza proprietaria Steinberg e la GPLv3. Swingers sceglie la GPLv3, come previsto per i progetti open source; "VST" è un marchio di Steinberg Media Technologies GmbH.
- Le librerie di sistema usate dalla grafica e dall'audio (per esempio ALSA, freetype e X11 su Linux) non sono incluse nel repository né nei bundle.
- Il carattere del testo serigrafato nell'interfaccia è quello di sistema, non è incorporato.

## Solo per i test (non distribuito)

| Componente | Versione | Licenza | Testo della licenza |
|---|---|---|---|
| [Catch2](https://github.com/catchorg/Catch2) | v3.5.2 | BSL-1.0 (Boost Software License 1.0) | `vendors/Catch2/LICENSE.txt` |

Catch2 si compila solo nel target `swingers_tests`: non fa parte di plugin né standalone.

## Non coperto dalla AGPL

Il logo Sweetspot Tools mostrato nel README non è nel repository (collegamento al repository [sweetspot-tools/brand](https://github.com/sweetspot-tools/brand)) ed è sotto **CC BY-ND 4.0**, non sotto la AGPL.
