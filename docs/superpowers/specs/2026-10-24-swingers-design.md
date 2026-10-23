# Swingers — progetto (spec)

Data: 2026-10-24. Stato: approvato in chat, da rivedere per iscritto.
Repo: `sweetspot-tools/swingers` (checkout locale `/srv/http/midiconverter`). Organizzazione: Sweetspot Tools, la stessa dei
Fairy Tapes; il nome segue il loro tono (gli "swingers" si scambiano i partner come Swingers scambia le note tra drum map;
lo swing è anche un groove di batteria).

## 1. Scopo

Convertire le parti di batteria MIDI da una drum map a un'altra: un groove scritto per una libreria (Addictive Drums 2)
deve suonare con un'altra (SDX Metal Foundry) con i pezzi giusti, senza riscrivere le note a mano.

Due usi, stesso motore:

- **Plugin MIDI** nel DAW, che converte in tempo reale.
- **Standalone** che apre un file MIDI, lo converte e salva un nuovo file.

Più la creazione e la modifica di drum map personalizzate.

**Criterio di riuscita:** un groove convertito suona identico all'originale, con le note giuste sul nuovo strumento e il
timing invariato al tick, sia nel plugin sia nel file salvato.

## 2. Da dove si parte

Il prototipo del 2018 (4 commit, circa 340 righe: proton-native, Babel 6, `midiconvert`, `xml2js`) non è recuperabile:
librerie abbandonate, timing perso nella conversione (la libreria passava per i secondi), crash sulle note non mappate,
mappe di arrivo con più note sulla stessa voce GM che si sovrascrivevano, file d'origine modificato, GUI incompleta. Si
riscrive da zero; dal prototipo si tengono l'idea del General MIDI come tappa intermedia e le tre mappe XML di Cubase in
`maps/`.

## 3. Decisioni prese

- **Tecnologia: JUCE 8, licenza AGPL-3.0** (licenza gratuita di JUCE; il repo è a parte dai Fairy Tapes, che restano
  GPL-3.0 su VST3 SDK + VSTGUI). C++20, CMake. CLAP con `clap-juce-extensions`.
- **Formati del plugin:** VST3 (Cubase, Reaper, Studio One, Bitwig, FL Studio), AU come MIDI FX (Logic), CLAP. Obiettivo:
  il maggior numero possibile di DAW.
- **Standalone = lo stesso plugin** dentro l'app autonoma di JUCE, con in più la conversione di file.
- **Modello di conversione:** il **GM è il pivot**. Ogni drum map dice, una volta sola, a quale voce GM corrisponde ogni
  sua nota (molti a uno ammesso: "HH open" e "HH open 2" possono andare entrambe su GM 46). Conversione A → B: nota di A →
  voce GM → nota di B per quella voce. Con N mappe servono N descrizioni, non N×N tabelle.
- **Eccezioni di coppia:** l'utente può collegare a mano una nota di A direttamente a una nota di B, scavalcando il GM, o
  staccare una nota per scartarla di proposito. Valgono solo per quella coppia e si salvano in un file creato alla prima
  eccezione: le coppie mai ritoccate non hanno file.
- **Le voci GM di una mappa si cambiano solo nell'editor della mappa**, non nella patchbay: trascinare verso la colonna GM
  non ha senso, a meno che una delle due mappe sia il GM stesso, e in quel caso la colonna centrale non si disegna.
- **Note senza corrispondenza:** si scartano sempre (nessuna opzione). Si vedono grigie nella patchbay; lo standalone le
  elenca nel resoconto.
- **Nel DAW il plugin si mostra compatto** (From → To e pulsante Edit mapping); la patchbay si apre solo su richiesta.

## 4. Architettura

Tre parti, con confini netti:

| Parte | Cosa fa | Dipende da |
|---|---|---|
| `core/` (libreria) | drum map, librerie di mappe, eccezioni, tabella di conversione, processore in tempo reale, conversione di file | solo `juce_core` (file, JSON, XML, `MidiFile`, `MidiMessage`); niente grafica |
| `plugin/` | `AudioProcessor` JUCE: bus MIDI, stato nel progetto, editor | `core/`, moduli JUCE audio e GUI |
| `ui/` | pannello compatto, patchbay, editor delle mappe, barra file dello standalone, aspetto rack | `core/`, `juce_gui_basics` |

Target CMake: `Swingers` (VST3, AU, CLAP, Standalone) e `swingers_tests`.

### 4.1 Motore (`core/`)

- **`DrumMap`**: id, nome, libreria; per ognuna delle 128 note facoltative: nome, voce GM (0-127 o nessuna), flag
  *preferita*.
- **`MapLibrary`**: carica le mappe incluse (risorse binarie, sola lettura) e quelle dell'utente; importa l'XML di Cubase.
- **`PairOverrides`**: per una coppia (id A, id B), le eccezioni: nota di A → nota di B oppure nota di A → scartata.
- **`ConversionTable`**: per le 128 note di partenza, il risultato: nota di arrivo oppure *scartata*, con il motivo
  (*standard*, *eccezione*, *scartata dall'utente*, *nessuna corrispondenza*). Calcolata da (A, B, eccezioni) con queste
  regole, nell'ordine:
  1. eccezione della coppia, se c'è;
  2. altrimenti voce GM della nota in A → nota di B con quella voce: se ce n'è più di una vince la *preferita*, altrimenti
     la prima per numero di nota;
  3. altrimenti scartata (*nessuna corrispondenza*).
  
  Se A o B è la mappa GM, la voce GM della nota GM è la nota stessa.
- **`LiveRemapper`** (tempo reale): applica la tabella a note-on, note-off e aftertouch polifonico del canale scelto
  (predefinito: tutti). Ricorda per ogni nota accesa (canale, nota d'ingresso) dove è stata mandata, così il note-off va
  alla nota giusta anche se nel frattempo cambiano le mappe. Tutto il resto (CC, pitch bend, program change, clock, SysEx,
  altri canali) passa intatto. Nessuna allocazione nel thread audio; la tabella nuova si pubblica con uno scambio atomico.
- **`FileConverter`**: legge il file con `juce::MidiFile` (tick e formato del file conservati), cambia solo il numero di
  nota degli eventi di nota e aftertouch polifonico sul canale scelto, scarta gli eventi delle note scartate (note-on e il
  loro note-off), scrive un nuovo file. Ritorna un resoconto: tracce toccate, note convertite, note scartate per numero e
  motivo.

### 4.2 Plugin

- Bus: MIDI in, MIDI out, nessun audio (`isMidiEffect`), così in Logic è un MIDI FX e negli altri DAW un effetto MIDI.
- **Stato nel progetto:** id delle due mappe, canale, e una copia delle due mappe e delle eccezioni in uso. Se una mappa
  non c'è più sul computer, il plugin usa la copia e lo segnala; il progetto suona uguale su un altro computer.

### 4.3 Dati su disco

- Formato proprio: **un file JSON per mappa** (`<id>.swmap.json`) con nome, libreria e le note usate; **un file JSON per
  coppia con eccezioni** (`<idA>__<idB>.swpair.json`).
- Mappe incluse: General MIDI, Addictive Drums 2, SDX Metal Foundry (dalle tre XML di `maps/`, convertite e controllate).
- Cartella dell'utente: `Documents/Swingers/` con `maps/` e `pairs/`, la stessa per plugin e standalone.
- Import da Cubase: dall'XML si prendono nome della mappa, e per ogni voce `INote` (nota), nome e `ONote` (voce GM
  proposta); le voci senza nome o con nome `---` restano senza voce GM. L'import crea una mappa JSON nella cartella
  dell'utente, poi rivedibile nell'editor.

## 5. Interfaccia

Testi visibili in inglese. Aspetto: il pannello rack dei Fairy Tapes (colori, grana, serigrafia; vedi
`fairy-tapes/docs/DESIGN_SYSTEM.md`), ridisegnato in JUCE.

- **Pannello compatto** (all'apertura, nel DAW e nello standalone): selettore *From*, freccia, selettore *To*, LED che
  lampeggia quando passano note, pulsante **Edit mapping**, selettore del canale.
- **Edit mapping → patchbay** a tutta finestra dentro il plugin, con **Done** per tornare:
  - tre colonne: note della mappa di partenza, voci GM, note della mappa di arrivo; due colonne se una delle mappe è il GM;
  - percorso standard: grigio tenue, senza contorni, che passa dalla colonna GM; le note ai due estremi ben evidenti;
  - cavo diretto (trascinamento da una nota di partenza a una di arrivo): colorato, da un estremo all'altro, e nasconde il
    percorso standard di quella nota;
  - nota d'origine senza corrispondenza: grigia, senza cavo; nota staccata dall'utente: grigia con un segno diverso;
  - gesti: trascinare da nota a nota crea l'eccezione; trascinare il cavo nel vuoto o clic destro → *Disconnect* scarta
    la nota; clic destro → *Reset* torna al percorso standard;
  - per default si vedono solo le note usate dalla mappa di partenza; un interruttore mostra tutte le 128;
  - mentre suona, nota d'origine e cavo si accendono brevemente;
  - da qui si apre l'editor delle mappe.
- **Editor delle mappe:** tabella delle 128 note (numero, nome, voce GM da menu, stella *preferita*), comandi *New*,
  *Duplicate*, *Import from Cubase*. Le mappe incluse non si modificano: si duplicano.
- **Standalone:** sotto il pannello compatto la barra *Open MIDI → Convert → Save* e il resoconto. Non sovrascrive mai il
  file d'origine; propone `<nome>-<MappaArrivo>.mid`.

## 6. Errori

- Un file di mappa o di coppia illeggibile non blocca mai l'audio: viene saltato e segnalato nel selettore.
- Mappa scelta mancante: si usa la copia salvata nel progetto, con avviso.
- File MIDI illeggibile o senza note sul canale scelto: lo standalone lo dice nel resoconto e non salva.

## 7. Test

Il codice nuovo nasce con un test che prima fallisce.

- **Motore:** tabella di conversione (standard, preferita, prima per numero, eccezione, scartata dall'utente, nessuna
  corrispondenza, mappa GM a un capo); `LiveRemapper` (note-off dopo cambio di mappa, canali esclusi intatti, eventi non di
  nota intatti).
- **File:** un MIDI convertito ha gli stessi eventi negli stessi tick, con lo stesso formato e la stessa risoluzione; cambiano
  solo i numeri di nota e mancano solo le note scartate.
- **Import Cubase:** le tre mappe di `maps/` si leggono con note, nomi e voci GM attese.
- **Plugin:** validatore VST3 di Steinberg, `auval` (AU, su Mac), `clap-validator`.

## 8. Prima versione e fuori campo

**Prima versione:** plugin VST3, AU MIDI FX e CLAP; standalone con conversione di file; patchbay con eccezioni di coppia;
editor delle mappe; import da Cubase; tre mappe incluse.

**Fuori dalla prima versione:** Ableton Live (non fa uscire il MIDI dai plugin VST3/AU: servirebbe un dispositivo Max for
Live che legga le stesse mappe), export verso Cubase, varianti numerate di una stessa voce GM (open 1/2/3), altri formati
di mappa (Reaper, Logic, Studio One, Superior Drummer, EZdrummer, GGD…), AAX.

## 9. Decisioni aperte

- Grafica di dettaglio di pannello compatto, patchbay ed editor: da fissare con una preview HTML come per i Fairy Tapes.
- Come si distribuiscono i binari (firma e notarizzazione su Mac, installer): da decidere prima del primo rilascio.
