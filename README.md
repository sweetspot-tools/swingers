# Swingers

Plugin MIDI (VST3, AU, CLAP) e applicazione standalone che converte le note di batteria da una drum map a un'altra, passando per General MIDI.

Fa parte di [Sweetspot Tools](https://github.com/sweetspot-tools).

## Compilazione

Richiede CMake 3.22+, un compilatore C++20 e le dipendenze di JUCE per la piattaforma. Le librerie sono submodule in `vendors/` (JUCE 8.0.4, Catch2 v3.5.2, clap-juce-extensions).

```bash
git submodule update --init --recursive
cmake -S . -B /tmp/swingers-build -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/swingers-build -j2 --target swingers_tests Swingers_VST3 Swingers_Standalone Swingers_CLAP
ctest --test-dir /tmp/swingers-build --output-on-failure
```

Si compila fuori dal repo (la cartella di build è a scelta). Con CMake 4 una dipendenza con un `cmake_minimum_required` molto vecchio può fallire: in tal caso aggiungere `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` alla configurazione.

## Licenza

AGPL-3.0, vedi [LICENSE](LICENSE).
