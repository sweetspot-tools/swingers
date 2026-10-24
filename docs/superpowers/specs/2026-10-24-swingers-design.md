# Swingers — progetto (spec)

Data: 2026-10-24. Stato: **implementato (prima versione, branch `juce-rewrite`)**. Verificato con test automatici,
validatore VST3 di Steinberg e rendering fuori schermo dell'interfaccia; non ancora provato a mano negli host (vedi §7 e §9).
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
riscrive da zero; dal prototipo si tiene solo l'idea del General MIDI come tappa intermedia. Le vecchie mappe XML di
`maps/` non si usano (deciso dall'utente il 2026-10-24): le mappe si rifanno nuove con l'editor, e saranno molte.

## 3. Decisioni prese

- **Tecnologia: JUCE 8, licenza AGPL-3.0** (licenza gratuita di JUCE; il repo è a parte dai Fairy Tapes, che restano
  GPL-3.0 su VST3 SDK + VSTGUI). C++20, CMake. CLAP con `clap-juce-extensions`.
- **Formati del plugin:** VST3 (Cubase, Reaper, Studio One, Bitwig, FL Studio), AU come MIDI FX (Logic), CLAP (categorie `note-effect`, `utility`). Obiettivo:
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
  
  Se A o B è la mappa GM, la voce GM della nota GM è la nota stessa. **Se A e B sono la stessa mappa (stesso id)** la
  regola 2 è l'identità: ogni nota va su sé stessa (anche le note senza voce GM e le note diverse con la stessa voce),
  poi si applicano le eccezioni della coppia; così GM → GM non scarta le note fuori da 35-81.
- **`LiveRemapper`** (tempo reale): applica la tabella a note-on, note-off e aftertouch polifonico del canale scelto
  (predefinito: tutti). Ricorda per ogni nota accesa (canale, nota d'ingresso) dove è stata mandata, così il note-off va
  alla nota giusta anche se nel frattempo cambiano le mappe. Tutto il resto (CC, pitch bend, program change, clock, SysEx,
  altri canali) passa intatto. Nessuna allocazione nel thread audio; la tabella nuova si pubblica con uno scambio atomico.
  Se il filtro del canale si allarga mentre una nota è tenuta, il suo note-off esce comunque sulla nota giusta.
  **Bypass del host:** il MIDI passa invariato, ma il note-off (e l'aftertouch) di una nota accesa prima del bypass va
  sulla nota d'arrivo in cui era stata mandata; una nota accesa durante il bypass è ricordata su sé stessa, quindi il suo
  note-off esce invariato anche dopo la fine del bypass. Nessuna nota resta appesa.
  **Caso limite accettato:** un secondo note-on per lo stesso (canale, nota d'ingresso) con un'altra nota d'arrivo, senza
  note-off in mezzo (possibile solo se tabella o filtro cambiano durante due colpi sovrapposti), sovrascrive la memoria
  della nota tenuta e può lasciare appesa la prima nota d'arrivo. Rarissimo; la correzione (un note-off sintetico
  restituito da `route()`) è rimandata.
- **`FileConverter`**: legge il file con `juce::MidiFile` (tick e formato del file conservati), cambia solo il numero di
  nota degli eventi di nota e aftertouch polifonico sul canale scelto, scarta gli eventi delle note scartate (note-on e il
  loro note-off), scrive un nuovo file. Ritorna un resoconto: tracce toccate, note convertite, note scartate per numero e
  motivo. Sulle sequenze di uscita **non** si chiama `updateMatchedPairs()`: JUCE inserirebbe note-off sintetici quando due
  note d'origine confluiscono su una sola nota d'arrivo, cambiando il numero di eventi e rompendo l'esattezza al tick; chi
  usa le sequenze in memoria non deve contare sugli accoppiamenti note-on/note-off. Rifiuta di scrivere sul file
  d'origine (stesso percorso, o stesso file raggiunto da cartelle con link simbolici o da hard link, con
  `std::filesystem::equivalent`).

### 4.2 Plugin

- Bus: MIDI in, MIDI out, nessun audio (`isMidiEffect`), così in Logic è un MIDI FX e negli altri DAW un effetto MIDI.
- **Stato nel progetto:** id delle due mappe, canale, e una copia delle due mappe e delle eccezioni in uso. Se una mappa
  non c'è più sul computer, il plugin usa la copia e lo segnala; il progetto suona uguale su un altro computer.
  **La copia del progetto vince** anche quando sul computer c'è una mappa con lo stesso id ma contenuto diverso (avviso
  "Map '<nome>' on this computer differs from the copy saved in the project: using the project's copy"); stessa regola per
  le eccezioni (file di coppia diverso dalla copia → la copia, con avviso). Se manca la mappa e manca la copia: General
  MIDI con avviso "Map '<id>' not found and no copy in the project: using General MIDI". Si passa alla versione del
  computer solo con una scelta esplicita: scegliere una mappa nella lista From/To, **anche quella già mostrata**, scarta
  la copia di quel lato e la copia delle eccezioni (si usano mappa e file di coppia del computer, se c'è); salvare la
  mappa dall'editor scarta la copia dei lati che la usano; modificare le eccezioni sostituisce la copia (come prima).
- **Thread:** `getStateInformation` legge solo uno snapshot serializzato, protetto da un lock e aggiornato a ogni
  ricostruzione, quindi è sicuro da qualsiasi thread del host; `setStateInformation` memorizza subito i byte come
  snapshot e applica lo stato sul thread dei messaggi (subito, se ci si è già, altrimenti con `callAsync` e una guardia
  sulla vita del processore). Tutto il resto è documentato come solo thread dei messaggi.
- Un cambio del seme dell'usura avvisa il host (`updateHostDisplay`). Il programma unico si chiama "Default" (il
  validatore VST3 rifiuta i programmi senza nome).

### 4.3 Dati su disco

- Formato proprio: **un file JSON per mappa** (`<id>.swmap.json`) con nome, libreria e le note usate; **un file JSON per
  coppia con eccezioni** (`<idA>__<idB>.swpair.json`).
- Mappe incluse nella prima versione: solo **General MIDI** (note 35-81 con i nomi dello standard), scritta da zero. Le
  mappe delle librerie si creano nuove con l'editor (decisione dell'utente del 2026-10-24).
- **Id e nomi dei file (regola unica, per avere nomi iniettivi):** l'id di una mappa usa solo `[a-z0-9-]` e il file si chiama
  esattamente `<id>.swmap.json`; il file di coppia `<idA>__<idB>.swpair.json`. Un file che non rispetta la regola non si
  carica (segnalato nel selettore). Un id uguale a una mappa inclusa non la sostituisce mai.
- **Id delle mappe nuove (deciso il 2026-10-24, revisione finale):** una mappa nuova o duplicata non ha id finché non si
  salva; al primo salvataggio prende lo slug del nome (stesse regole dell'import, `[a-z0-9-]`, al massimo 40 caratteri,
  `map` se vuoto) + `-` + 4 cifre esadecimali minuscole casuali (es. `studio-kit-7f3a`). Prima di salvare la libreria si
  ricarica dal disco e, se l'id è già preso (in libreria o come file su disco), si estrae di nuovo (`-2`, `-3`… solo come
  ultima risorsa). Anche le mappe importate da Cubase prendono il suffisso casuale. Così due computer difficilmente
  danno lo stesso id a mappe diverse, e un'altra istanza che salva nel frattempo non viene mai sovrascritta.
- **Mai sovrascrivere ciò che non si è caricato:** un salvataggio rifiuta di rimpiazzare un file di mappa o di coppia
  che non è riuscito a caricarsi (illeggibile o con id sbagliato), per non distruggere dati dell'utente.
- Cartella dell'utente: `Documents/Swingers/` con `maps/` e `pairs/`, la stessa per plugin e standalone.
- **Più istanze sulla stessa cartella:** ogni istanza ricarica la libreria dal disco quando si apre l'editor, appena prima
  che la lista From/To (o il selettore dell'editor delle mappe) mostri le voci, e prima di *New*, *Duplicate*, *Save* e
  *Import* nell'editor delle mappe; così una mappa salvata da un'altra istanza compare senza riaprire il progetto.
- Import da Cubase: dall'XML si prendono nome della mappa, e per ogni voce `INote` (nota), nome e `ONote` (voce GM
  proposta); le voci senza nome, con nome `---` o con `ONote` 0 restano senza voce GM. L'import tollera gli XML che
  Cubase produce con `<`, `>` o `&` non escapati dentro i nomi (es. `[Ride ><]`). L'import crea una mappa JSON nella
  cartella dell'utente, poi rivedibile nell'editor.

## 5. Interfaccia

**Layout approvato dall'utente il 2026-10-24** sulla preview `docs/mockups/swingers_preview.html`: pannello compatto
520 × 140 (FROM → TO in riquadri col titolo sul bordo basso, **canale come pulsante CH che cicla** (ALL, 1-16) con LED
giallo, al posto della ComboBox del piano; LED MIDI ambra, EDIT MAPPING, riga avvisi in ambra); patchbay ed editor delle mappe a 900 × 600 (la finestra del plugin si
allarga); standalone 520 × 330 con riquadri FILE e REPORT.

**Usura come nei Fairy Tapes** (richiesta dell'utente del 2026-10-24): grana fine del pannello, graffi chiari, macchie opache
a bordo netto, pezzi di nastro di carta scritti a mano (Homemade Apple, Apache-2.0, incluso nel plugin) con residui di
colla, viti agli angoli. Il nastro ha un'ombra morbida come nella preview di The Stripper (`stripper_rack.html`): nera al
50 %, spostata di 0,8 / 1,6 px, sfocatura leggera (deciso nella revisione finale: il "niente ombre" dei Fairy Tapes
riguarda le macchie). Tutto da un seme salvato nel progetto; nastro e macchie non coprono mai i controlli e nessun
elemento dell'usura cattura i clic. Nella prima versione l'intensità è fissa (scelta Off/Light/Heavy: decisione aperta). I generatori sono per categoria, ognuno
con il proprio flusso casuale, così un elemento non sposta gli altri. Un cambio di seme avvisa il host.

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
  *Duplicate*, *Import from Cubase*, *Save*. Le mappe incluse non si modificano: si duplicano. Le modifiche non salvate
  sono tenute in memoria dal processore (non nel progetto) e ricompaiono quando la finestra si riapre, anche se il host la
  chiude; non sopravvivono alla chiusura del progetto (salvare è esplicito). *Duplicate* è disabilitato su una mappa nuova
  non salvata.
- **Dimensioni della finestra:** 520 × 140 compatto, 520 × 330 con la barra file (standalone), 900 × 600 per patchbay ed
  editor delle mappe; il plugin ridimensiona la finestra al cambio di vista.
- **Standalone:** sotto il pannello compatto la barra *Open MIDI → Convert → Save* e il resoconto. Non sovrascrive mai il
  file d'origine; propone `<nome>-<MappaArrivo>.mid`. *Convert* tiene il risultato in memoria e *Save* lo scrive; se dopo
  la conversione cambiano mappe, eccezioni o canale il risultato è **invalidato** (va rifatto *Convert*), così non si salva
  mai una conversione vecchia. Un'apertura fallita conserva il file precedente. Se il nome scelto nel salvataggio non ha
  estensione si scrive `<nome>.mid`; se `<nome>.mid` esiste già il salvataggio si rifiuta con un messaggio ambra
  ("<nome>.mid already exists: choose another name"), perché la conferma nativa riguardava `<nome>`, non `<nome>.mid`.

## 6. Errori

- Un file di mappa o di coppia illeggibile non blocca mai l'audio: viene saltato e segnalato nel selettore.
- Mappa scelta mancante: si usa la copia salvata nel progetto, con avviso; senza copia, General MIDI con avviso.
- Mappa (o file di coppia) presente ma diversa dalla copia del progetto: si usa la copia, con avviso (§4.2).
- File MIDI illeggibile o senza note sul canale scelto: lo standalone lo dice nel resoconto e non salva.

## 7. Test

Il codice nuovo nasce con un test che prima fallisce.

- **Motore:** tabella di conversione (standard, preferita, prima per numero, eccezione, scartata dall'utente, nessuna
  corrispondenza, mappa GM a un capo); `LiveRemapper` (note-off dopo cambio di mappa, canali esclusi intatti, eventi non di
  nota intatti).
- **File:** un MIDI convertito ha gli stessi eventi negli stessi tick, con lo stesso formato e la stessa risoluzione; cambiano
  solo i numeri di nota e mancano solo le note scartate.
- **Import Cubase:** piccoli XML di prova (nome, note con e senza voce GM, `---`, `ONote` 0, caratteri non escapati)
  si leggono con note, nomi e voci GM attese.
- **Plugin:** validatore VST3 di Steinberg, `auval` (AU, su Mac), `clap-validator`, lanciati da `tools/validate.sh`.
  Stato della prima versione: VST3 validato (47 test passati; il validatore ha trovato un programma senza nome, corretto);
  CLAP non validato (`clap-validator` non installato, non scaricato); AU solo su macOS, non eseguito.
- **Interfaccia e host:** al posto delle prove manuali nei DAW, test automatici (editor creato con
  `ScopedJuceInitialiser_GUI`) e rendering fuori schermo in PNG (`swingers_render`, immagini in `docs/screenshots/`).
  La prova negli host (carica, cambia mappe, salva e riapri il progetto) e il confronto di un file convertito in un DAW
  restano da fare a mano.

## 8. Prima versione e fuori campo

**Prima versione:** plugin VST3, AU MIDI FX e CLAP; standalone con conversione di file; patchbay con eccezioni di coppia;
editor delle mappe; import da Cubase; mappa General MIDI inclusa.

**Fuori dalla prima versione:** Ableton Live (non fa uscire il MIDI dai plugin VST3/AU: servirebbe un dispositivo Max for
Live che legga le stesse mappe), export verso Cubase, varianti numerate di una stessa voce GM (open 1/2/3), altri formati
di mappa (Reaper, Logic, Studio One, Superior Drummer, EZdrummer, GGD…), AAX.

## 9. Decisioni aperte

- Intensità dell'usura regolabile (Off, Light, Heavy) come nei Fairy Tapes: dove metterla, visto che Swingers non ha un
  retro con Settings.
- Come si distribuiscono i binari (firma e notarizzazione su Mac, installer): da decidere prima del primo rilascio.
- Font stretto incorporato per il testo serigrafato: oggi si usa il sans di sistema (Liberation Sans Bold su Linux), che
  cambia da un sistema operativo all'altro, quindi larghezze e resa del testo non sono identiche ovunque.
- Dispositivo Max for Live per Ableton Live (fuori dalla prima versione, vedi §8).
- Prova negli host reali (Cubase, Reaper, Studio One, Bitwig, FL Studio, Logic) e validazione di CLAP e AU.

### Limiti noti della prima versione (da valutare)

- Il caso limite del note-on ripetuto (§4.1).
