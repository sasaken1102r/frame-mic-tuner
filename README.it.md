# Frame Mic Tuner

Un pannello della dashboard di SteamVR per Steam Frame che cambia la cancellazione dell'eco e il filtro rumore del microfono del visore mentre lo indossi. Spegni la cancellazione dell'eco con gli auricolari, così i suoni bassi come quelli della bocca passano chiari, e riaccendila con gli altoparlanti di Frame, così il loro suono non entra nel microfono. Un controllo voce integrato registra qualche secondo di ciò che le app sentono davvero, per confrontare le impostazioni sul momento.

[English](README.md) · [日本語](README.ja.md) · [简体中文](README.sc.md) · [Deutsch](README.de.md) · [Français](README.fr.md) · [Español](README.es.md)

| Rapido (preimpostazioni) | Regolazione fine |
|---|---|
| ![La scheda Rapido](docs/v12-en-quick-speaker_2026-10-18_19-50-00.png) | ![La scheda Regolazione fine](docs/v12-en-fine_2026-10-18_19-50-00.png) |

### Video dimostrativo (con audio)

Gli altoparlanti di Frame riproducono un suono di bufera mentre il controllo voce registra, e le due registrazioni vengono riprodotte. 1ª clip: filtro rumore spento (preimpostazione altoparlante). 2ª clip: anche la cancellazione dell'eco spenta (preimpostazione auricolari). Senza cancellazione dell'eco, la bufera degli altoparlanti entra nel microfono. Il pannello nel video è in giapponese.

https://github.com/user-attachments/assets/14b5e180-f417-4d90-85f5-6ba1dc573546

## Cosa fa

- **Impostazioni per ogni uscita**: il pannello ricorda le impostazioni del microfono Frame (cancellazione dell'eco, filtro rumore e la sua intensità) separatamente per ogni uscita audio: gli altoparlanti di Frame, auricolari o adattatori USB-C, cuffie Bluetooth. Quando l'uscita cambia (per esempio colleghi gli auricolari e diventano l'uscita), l'app applica da sola le impostazioni di quell'uscita, anche a pannello chiuso, e il pannello dice «%s collegato, quindi vale l'impostazione salvata» con **Annulla**. Un'uscita mai vista parte dalla preimpostazione **Altoparlanti Frame** se sono gli altoparlanti di Frame, altrimenti da **Auricolari / cuffie**.
- **Uscita e microfono**: il pulsante **Uscita** in alto elenca le uscite collegate e quelle che hai già usato. Lì puoi **Riproduci qui** (la rende l'uscita predefinita, la stessa impostazione dell'audio di SteamVR), scegliere un'uscita per vederne e cambiarne le impostazioni in anticipo, oppure **Dimentica** una che non usi più. L'elenco scorre con lo stick o trascinando. **Microfono** sceglie quale microfono usano le app. La cancellazione dell'eco e il filtro rumore valgono solo per il microfono integrato di Frame; con un altro microfono il pannello lo dice e offre **Usa il microfono integrato**.
- **Rapido e Regolazione fine**: **Rapido** ha le preimpostazioni per come ascolti. **Auricolari / cuffie** spegne la cancellazione dell'eco e il filtro rumore. **Altoparlanti Frame** accende la cancellazione dell'eco e spegne il filtro rumore. **Regolazione fine** ha gli interruttori singoli e due cursori per il filtro rumore: **Severità** (quanto un suono deve sembrare una voce per passare; più basso fa passare di più) e **Tenuta** (per quanto il suono continua dopo che smetti di parlare). Le modifiche valgono subito, anche mentre il microfono è in uso, senza tagliare l'audio. Il pannello si apre sull'ultima scheda usata.
- **Percorso del segnale**: mostra se un'app usa il microfono e da quali filtri passa davvero il suono (microfono → EQ → cancellazione eco → filtro rumore → app), letto dai collegamenti PipeWire in corso.
- **Silenziato**: l'intestazione mostra «In uso», «Non in uso» o un «Silenziato» rosso (silenziato da Steam, `wpctl`, un tasto aux o altrove), e subito accanto **Silenzia** / **Riattiva**.
- **Controllo voce**: registra fino a 10 secondi del suono finale che ricevono le app, tiene le ultime 5 registrazioni e le riproduce sull'uscita attuale. Ogni registrazione mostra l'ora, la durata, l'uscita e l'impostazione con cui è stata fatta (per esempio «AB13X · Eco spento») e una piccola forma d'onda.
- **App e aggiornamenti** (pulsante in alto a destra; un punto rosa vuol dire che c'è qualcosa da vedere): la versione in esecuzione, **Check now** e **Update**, l'interruttore **Cerca aggiornamenti** e le altre app Steam Frame dell'autore. Quella con cui questa lavora, frame-aux-shortcuts (cambia il muto del microfono con il tasto aux), è la prima; le altre sono a un tocco. Installare apre il programma in Konsole dopo averti mostrato il comando esatto.
- Interfaccia in giapponese, inglese, cinese semplificato, tedesco, francese, spagnolo e italiano. Testo e controlli rispettano il contrasto WCAG 2.x AA.

Perché serve: finché un'app usa il microfono, SteamOS lo fa passare da EQ, cancellazione dell'eco e filtro rumore. La cancellazione dell'eco riduce molto i suoni piccoli, e il filtro rumore zittisce tutto ciò che non sembra una voce. Con gli auricolari non c'è suono di altoparlante da cancellare, quindi spegnere la cancellazione dell'eco fa passare più della tua voce.

## Requisiti

- Uno Steam Frame con la modalità sviluppatore e SSH (Impostazioni > Sistema > Modalità sviluppatore, poi una password sotto Sviluppatore). Scegli una password robusta: con SSH, chiunque nella tua rete che la conosca può entrare nel visore.
- SteamVR sul visore. Compilare dai sorgenti (più sotto) richiede cmake, ninja, g++, pkg-config e i file di sviluppo di cairo, FreeType, PipeWire e Vulkan, già presenti in SteamOS. Una versione scaricata non ne ha bisogno.

## Installazione

### Il modo più semplice: installare dentro Frame (consigliato)

Non serve un PC. In Konsole su Frame (+ sulla barra in basso → l'elenco dei programmi → Konsole), scrivi questo comando, premi Invio e scegli **3** (frame-mic-tuner) nel menu.

```sh
curl -fsSL https://frame.sasaken1102s.net | sh
```

- Fallo una volta prima: Impostazioni di Steam → Sistema → attiva «Enable Developer Mode» (finché è spento, Konsole non compare nell'elenco +).
- Le altre app (frameeyeosc, frame-jp-keyboard, frame-perf-overlay, frame-aux-shortcuts) si installano dallo stesso menu.
- Puoi leggere lo script prima: `curl -fsSL https://frame.sasaken1102s.net/i | less`.
- Per aggiornare, lo stesso comando e lo stesso numero. Per disinstallare, `u` nel menu.
- Guida passo passo e video: https://frame.sasaken1102s.net
- Senza domande: `curl -fsSL https://frame.sasaken1102s.net | sh -s -- install mic`

Ciò che viene installato e le opzioni sono gli stessi di «Installare da un PC» (esegue `install.sh` per te).

### Installare da un PC

Scarica `frame-mic-tuner-<version>.tar.gz` dalla [pagina delle versioni](https://github.com/sasaken1102r/frame-mic-tuner/releases) e copialo sul visore, per esempio dal PC:

```sh
scp frame-mic-tuner-*.tar.gz steamos@<ip-del-visore>:
```

Poi sul visore (`ssh steamos@<ip-del-visore>`):

```sh
tar xzf frame-mic-tuner-*.tar.gz
cd frame-mic-tuner
./install.sh
```

Oppure compilalo dai sorgenti (sempre sul visore):

```sh
git clone https://github.com/sasaken1102r/frame-mic-tuner.git
cd frame-mic-tuner
./install.sh
```

Poi **riavvia il visore** una volta, così WirePlumber carica lo script di cambio. Non riavviare PipeWire o WirePlumber a mano mentre giochi: SteamVR e Steam Link perdono l'audio. Se succede lo stesso, riavvia SteamVR sul visore (o riconnetti Steam Link) e l'audio torna.

sudo non serve. `install.sh` mette tutto nella tua cartella home (`~/.local/bin`, `~/.local/share`, `~/.config`), quindi gli aggiornamenti di SteamOS non lo tolgono. Per aggiornare a mano: scarica la nuova versione ed esegui di nuovo il suo `install.sh` (oppure, in una copia dei sorgenti, `git pull && ./install.sh`). Oppure premi **Aggiorna** in **App e aggiornamenti** quando c'è una versione più nuova. La versione 0.1.0 non ha il pulsante di aggiornamento, quindi il passaggio da 0.1.0 a 0.2.0 va fatto a mano quest'unica volta.

Cosa viene installato:

- l'app, una voce per l'elenco **+** della dashboard (avvia un programma) e le sue icone
- un'unità utente systemd per avviarla insieme a SteamVR (non abilitata finché non la accendi; vedi sotto)
- uno script e una configurazione WirePlumber (`contrib/wireplumber/`) che sostituiscono il tracciamento del microfono di SteamOS con uno di cui si possono cambiare cancellazione dell'eco e filtro rumore. Per il resto si comporta come l'originale.
- lo script di aggiornamento condiviso (`~/.local/share/frame-mic-tuner/frame-update.sh`, da `vendor/frame-updater/`), usato da **App e aggiornamenti**

Per rimuoverlo: `./install.sh --uninstall`, poi riavvia il visore per tornare al comportamento originale del microfono di SteamOS. Aggiungi `--purge` per cancellare anche le impostazioni dell'app e i valori di cambio salvati.

## Uso

1. Apri la dashboard di SteamVR, premi **+** (avvia un programma) e scegli **Frame Mic Tuner**. Nella riga in basso della dashboard compare un'icona **Mic**; selezionala per aprire il pannello. Rilanciarla mentre è in esecuzione apre solo il pannello.
2. Il pulsante **Uscita** mostra l'uscita da cui stai ascoltando. Nella scheda **Rapido**, sotto «Quando ascolti da <uscita>», scegli **Auricolari / cuffie** o **Altoparlanti Frame**. Sono preimpostazioni: mettono cancellazione dell'eco e filtro rumore sui valori consigliati (le etichette di ogni scheda li mostrano); l'intensità del filtro rumore resta. L'app ricorda la scelta per quell'uscita, quindi la volta dopo cambia da sola. Per un controllo più fine passa a **Regolazione fine**. Se allora le tue impostazioni non coincidono con nessuna preimpostazione, **Rapido** dice «Ora valgono le impostazioni della regolazione fine». Il filtro rumore taglia i suoni bassi (della bocca e simili) quando è attivo.
   - In **Regolazione fine**, trascina **Severità** e **Tenuta** per regolare il filtro rumore. I valori di SteamOS sono 23 % e 500 ms; **Predefinito** li riporta. I cursori agiscono solo mentre il filtro rumore è attivo.
   - SteamOS riporta questi due valori ogni volta che il suo audio si riavvia (per esempio dopo un riavvio). L'app salva i tuoi valori per ogni uscita e li riapplica all'avvio e quando l'uscita cambia, quindi restano solo mentre l'app è in esecuzione. Attiva **Avvia con SteamVR** se li vuoi sempre. Senza l'app si usano i valori di SteamOS.
   - Dopo che l'app ha cambiato le impostazioni da sola, **Rapido** dice «<uscita> collegato, quindi vale l'impostazione salvata». **Annulla** rimette ciò che c'era prima e lo ricorda per questa uscita al posto del precedente.
3. **Uscita** (in alto a sinistra): apre «Scegli l'uscita». Premi una riga per vedere le impostazioni di quell'uscita (puoi cambiare quelle di un'uscita che ora non è collegata; si usano quando torna). **Riproduci qui** rende quell'uscita quella predefinita, come l'audio di SteamVR. **Dimentica** cancella le impostazioni ricordate di un'uscita non collegata. Scorri con lo stick o trascinando.
4. **Microfono** (accanto a Uscita): **Usa questo microfono** sceglie il microfono da cui registrano le app. Con qualsiasi cosa che non sia il microfono integrato di Frame, cancellazione dell'eco e filtro rumore non si applicano, e la scheda delle impostazioni offre **Usa il microfono integrato**.
5. **Silenzia** / **Riattiva** (in alto, accanto allo stato): silenzia o riattiva subito il microfono predefinito, senza chiedere.
6. **Controllo voce**: premi **Registra**, di' qualcosa e premi **Stop** (si ferma da solo dopo 10 secondi). Cambia l'impostazione, registra di nuovo e usa ▶ su ogni riga per confrontare. La registrazione si ferma appena chiudi il pannello.
7. **Avvia con SteamVR** (riga in basso): accendilo perché l'app parta da sola con SteamVR d'ora in poi. Puoi abilitarlo anche con `./install.sh --autostart`.
8. **Lingua**: il pannello parte nella lingua del tuo Steam Frame (giapponese, cinese semplificato, tedesco, francese, spagnolo o italiano se Steam è impostato così, altrimenti inglese). Cambiala con il menu in basso a sinistra (日本語, English, 简体中文, Deutsch, Français, Español, Italiano); la scelta viene salvata.
9. **Esci**: premilo due volte, oppure passa sull'icona Mic della dashboard e scegli Chiudi.
10. **App e aggiornamenti** (in alto a destra; **Impostazioni microfono** torna indietro). I testi e i pulsanti di quel riquadro sono ancora in inglese. **Questa app** mostra la versione in esecuzione, per esempio «Up to date (0.3.5)». **Check now** cerca subito una versione più nuova (funziona anche se **Cerca aggiornamenti** è spento).
    - Quando ce n'è una, il riquadro prende un bordo del colore di accento e dice «Version X is available». Premi **Update**; chiede «Update to X?» con **Cancel** e **Update** (la domanda sparisce da sola dopo 3 secondi). Premi di nuovo **Update** per iniziare. Il riquadro mostra poi l'avanzamento («Updating: Downloading», e così via).
    - Il pannello resta aperto e continua con la versione vecchia mentre installa. Quando dice «X is installed. Quit and start the app again to use it», premi **Esci** due volte e riavvia l'app da **+** (o riavvia SteamVR), poi premi **Close** per chiudere il messaggio se è ancora lì. Se la versione nuova ha cambiato lo script di WirePlumber, riavvia anche il visore (il riquadro te lo ricorda, e il registro dell'aggiornamento dice se è cambiato).
    - Se dice «This version can't be installed from here» (la versione non ha `SHA256SUMS` o non ha un pacchetto), aggiorna a mano come in Installazione; il riquadro mostra la pagina della versione.
    - Se l'aggiornamento fallisce, il riquadro prende un bordo rosso, dice perché e che non è stato cambiato nulla, e offre **Try again** e **Close**. Se fallisce solo il controllo (per esempio senza rete), diventa rosso solo il testo e **Check now** resta.
    - **Cerca aggiornamenti** acceso / spento: spento ferma il controllo quotidiano e il recupero dell'elenco delle app (vedi Privacy).
    - **App di supporto** mostra frame-aux-shortcuts (cambia il muto del microfono con il tasto aux) con **Installa** se manca. «App Frame di sasaken@» apre l'elenco completo. **Installa** e **Apri il programma** mostrano prima il comando esatto; **Apri in Konsole** lo esegue poi in Konsole, dove il programma mostra l'avanzamento. Niente sudo; tutto va nella cartella home.

Le impostazioni sono disponibili anche da SSH:

```sh
wpctl settings --save frame-mic.echo-cancel false        # auricolari: cancellazione dell'eco spenta
wpctl settings --save frame-mic.echo-cancel true         # altoparlante: cancellazione dell'eco attiva (predefinito)
wpctl settings --save frame-mic.noise-suppression true   # filtro rumore attivo (predefinito: spento)
~/.local/bin/frame-mic-tuner --print                     # valori attuali, microfono in uso, percorso, uscite e impostazioni salvate
```

Una modifica fatta così (o con un tasto aux) viene notata mentre il pannello è aperto, o al più tardi quando l'uscita cambia, e viene ricordata per l'uscita attuale.

## Se qualcosa non va

- **Il pannello dice «Il cambio microfono non è installato»**: lo script di WirePlumber non è ancora caricato. Esegui `./install.sh`, poi riavvia il visore.
- **I pulsanti di avvio automatico sono grigi**: l'unità systemd non è installata. Esegui `./install.sh`.
- **Le registrazioni sono mute, o gli altri non ti sentono**: controlla il volume del microfono nel mixer del visore (sola lettura; è sicuro):
  ```sh
  amixer -c 0 cget name='VA_DEC0 Volume'
  amixer -c 0 cget name='VA_DEC1 Volume'
  ```
  Entrambi dovrebbero dire `values=96`. Se dicono `0`, il microfono è di fatto silenziato. Questa app non tocca mai il mixer, ma i valori sono già scesi a 0 da soli. Per rimetterli (a tuo rischio; annota prima i valori attuali e il `max=` mostrato da `cget`, e usa il valore abituale del tuo visore, 96 sul nostro):
  ```sh
  amixer -c 0 cset name='VA_DEC0 Volume' 96
  amixer -c 0 cset name='VA_DEC1 Volume' 96
  ```
- **Nessun audio in SteamVR o Steam Link dopo un riavvio di PipeWire o WirePlumber**: riavvia SteamVR sul visore (o riconnetti Steam Link). L'audio torna.
- **Nessun audio dopo un aggiornamento di SteamOS**: lo script di cambio viene caricato come parte necessaria di WirePlumber, quindi se un aggiornamento lo rompe, WirePlumber può fermarsi e portarsi via tutto l'audio. Rimuovi la configurazione via SSH e riavvia il visore (o esegui `./install.sh --uninstall` e riavvia):
  ```sh
  rm ~/.config/wireplumber/wireplumber.conf.d/90-frame-mic.conf
  ```
- **«In uso» anche se non parli in nessuna app**: conta ogni app che registra audio, compresi i registratori di schermo o di suono. SteamOS accende i filtri per tutte.
- **Frame Mic Tuner manca dall'elenco +**: riavvia il visore una volta dopo l'installazione.
- **Manca l'icona Mic nella dashboard anche se l'app è in esecuzione**: SteamVR può perdere il pannello dell'app (per esempio quando la dashboard stessa si riavvia). L'app controlla ogni 3 secondi e ricrea il pannello da sola; aspetta qualche secondo e riapri la dashboard. Rilanciarla da **+** controlla e lo ricrea anche prima di aprire il pannello. Se ancora non torna, il registro (`journalctl --user -u frame-mic-tuner -f` quando gira come servizio) dice perché (il registro è in giapponese; cerca le righe che iniziano con «自己修復», che significa «autoriparazione»); uscire dall'app e riavviarla lo sistema anche.
- Registri: `journalctl --user -u frame-mic-tuner -f` quando gira come servizio. Per vedere il registro di un avvio manuale, esci dall'app e avvia `~/.local/bin/frame-mic-tuner` da SSH.
- **Il controllo degli aggiornamenti fallisce, o «Update» non fa nulla**: `~/.cache/frame-mic-tuner/update.log` ha i dettagli, e `~/.cache/frame-mic-tuner/update-check.json` / `update-state.json` l'ultima risposta grezza. Spegnere **Cerca aggiornamenti** (o `update_check: false` nel file delle impostazioni) ferma il controllo quotidiano automatico senza togliere **Check now** / **Update**.
- **Le impostazioni non sono cambiate quando ho collegato gli auricolari**: l'app segue solo l'uscita *predefinita*. Se SteamVR continua a suonare dagli altoparlanti di Frame, premi **Riproduci qui** sugli auricolari in **Uscita** (o sceglili nell'audio di SteamVR). Le uscite che non sono dispositivi veri (filtri, uscite virtuali) vengono ignorate.
- **«Impossibile aprire Konsole» installando un'altra app**: Konsole ha bisogno della modalità sviluppatore (Impostazioni di Steam → Sistema → Enable Developer Mode).

## Privacy

- Le registrazioni del controllo voce restano solo nella memoria dell'app. Non vengono mai scritte su disco né nei registri, e non vengono mai inviate da nessuna parte. Spariscono quando l'app esce.
- Durante la registrazione il pannello mostra «● Registrazione». La registrazione si ferma appena chiudi il pannello.
- Il microfono può prendere le voci delle persone intorno a te. Tienilo presente prima di registrare.
- Per scegliere la lingua predefinita legge una volta all'avvio la riga `language` del `~/.steam/registry.vdf` di Steam (sola lettura).
- L'app non ha telemetria. L'uso della rete è il controllo degli aggiornamenti e l'elenco delle app (sotto):
  - **Quando**: mentre l'app è in esecuzione, chiede a GitHub all'avvio se l'ultima risposta ha più di 24 ore, e poi circa una volta al giorno (un'ora dopo se il controllo è fallito). **Controlla ora** chiede subito. Spegnere **Cerca aggiornamenti** in **App e aggiornamenti** (o `update_check: false` nel file delle impostazioni) ferma il controllo automatico; allora si collega solo se premi **Controlla ora** o **Aggiorna**.
  - **Cosa invia**: una richiesta HTTPS dell'ultima versione di questo progetto a `api.github.com`, senza account, identificativo o dati d'uso. Come ogni richiesta web, GitHub vede il tuo indirizzo IP e lo User-Agent di `curl`. Registrazioni, impostazioni e registri non vengono mai inviati.
  - **Cosa riceve**: il numero di versione dell'ultima release, la sua pagina e i nomi e i link dei suoi file. La risposta resta in `~/.cache/frame-mic-tuner/update-check.json`.
  - **Solo quando premi Update e confermi**, scarica anche il pacchetto di quella versione e `SHA256SUMS` da `github.com` / `*.githubusercontent.com`.
- Con **Cerca aggiornamenti** acceso, recupera anche l'elenco delle altre app Steam Frame dell'autore e le loro icone da `frame.sasaken1102s.net` (al massimo una volta all'ora, condiviso con le altre app dell'autore; tenuto in `~/.cache/frame-apps/`). È un semplice GET HTTPS senza nulla allegato (`curl -q --proto =https --max-redirs 0`): come GitHub, `frame.sasaken1102s.net` vede il tuo indirizzo IP e lo User-Agent di `curl`, e non viene inviato nient'altro. Spegnere **Cerca aggiornamenti** ferma anche questo, e il pannello mostra allora l'ultimo elenco recuperato o quello incluso nell'app. Quando installi un'altra app da **App e aggiornamenti**, si apre Konsole ed esegue il comando di installazione mostrato nella conferma, che si collega a `frame.sasaken1102s.net` e a GitHub dentro Konsole.
- Gli unici file che scrive sono `~/.config/frame-mic-tuner/config.json` (lingua dell'interfaccia, ultima scheda, l'impostazione del controllo, e per ogni uscita usata il suo nome, le impostazioni del microfono Frame e la data dell'ultimo uso), `~/.config/frame-mic-tuner/install-args` (le opzioni del tuo ultimo `./install.sh`, così un aggiornamento reinstalla allo stesso modo), `~/.cache/frame-mic-tuner/` (la risposta in cache del controllo, il registro e lo stato dell'aggiornamento, una cartella di blocco `update.lock/` mentre un aggiornamento è in corso, e una cartella di lavoro il cui download viene cancellato dopo ogni aggiornamento), `~/.cache/frame-apps/` (l'elenco delle app e le icone recuperate, condivisi con le altre app dell'autore) e un file di blocco in `$XDG_RUNTIME_DIR` che tiene il suo identificativo di processo (per impedire l'avvio di una seconda copia). Un aggiornamento riscrive anche i file installati, come fa `./install.sh`.

## Avvertenza

- Uso a tuo rischio. Questo progetto è stato fatto con Claude Opus 5.5, un modello di IA. L'ho provato sul mio Steam Frame, ma non posso rispondere di ciò che succede sul tuo: leggi il codice e controllalo tu prima di eseguirlo. Il software è fornito senza garanzia (vedi [LICENSE](LICENSE)).
- Cosa cambia: le due impostazioni WirePlumber `frame-mic.echo-cancel` e `frame-mic.noise-suppression` (tramite `wpctl settings`), i due parametri in esecuzione del filtro rumore «VAD Threshold (%)» e «VAD Grace Period (ms)» (tramite `pw-cli set-param`; SteamOS rimette i suoi valori quando il suo audio si riavvia) e la configurazione WirePlumber nella tua cartella home, dove sostituisce il tracciamento del microfono di SteamOS con il proprio script. Quando premi **Riproduci qui**, **Usa questo microfono** / **Usa il microfono integrato** o **Silenzia** / **Riattiva**, cambia l'uscita predefinita, il microfono predefinito o il suo muto (tramite `wpctl set-default` / `wpctl set-mute`, come fa l'audio di SteamVR). A parte questo, abilita o disabilita la propria unità utente systemd solo se glielo chiedi, apre Konsole con il programma di installazione quando confermi l'installazione di un'altra app e, quando aggiorni dal pannello, esegue l'`install.sh` della nuova versione in un'unità utente systemd di breve durata (`frame-mic-tuner-update`).
- Non riavvia mai PipeWire o WirePlumber, non scrive mai nel mixer ALSA (amixer), non tocca mai `/etc` e non ha mai bisogno di root.
- **Update**: scarica il `.tar.gz` e il `SHA256SUMS` dell'ultima versione da GitHub via HTTPS (solo verso gli host di GitHub), controlla l'hash e solo allora lo estrae in `~/.cache/frame-mic-tuner/update/` ed esegue il suo `install.sh` con le stesse opzioni della tua ultima installazione. Rifiuta una versione senza `SHA256SUMS`, un hash che non coincide, o un archivio che contiene un percorso assoluto, `..`, un collegamento o un file speciale; non viene cambiato nulla se succede uno di questi casi. Il download viene cancellato dopo. `SHA256SUMS` è un checksum pubblicato nella stessa versione, non una firma: intercetta un download danneggiato o incompleto, ma non una versione che sia stata sostituita su GitHub. L'app in esecuzione non viene riavviata: esci e riavviala per usare la versione nuova. I passi esatti sono nelle [note di frame-update.sh](vendor/frame-updater/frame-update.sh).
- Un aggiornamento di SteamOS può cambiare come sono impostati i filtri del microfono. Allora i cambi possono smettere di funzionare finché questo progetto non viene aggiornato. Poiché lo script di cambio è una parte necessaria di WirePlumber, uno script che non si carica più può anche fermare tutto l'audio del visore (vedi Se qualcosa non va). `./install.sh --uninstall` e un riavvio riportano il comportamento originale di SteamOS.
- È un progetto non ufficiale, senza rapporto né approvazione da parte di Valve Corporation. Steam, Steam Frame, SteamOS, SteamVR e Steam Link sono marchi e/o marchi registrati di Valve Corporation negli Stati Uniti e/o in altri paesi. I nomi sono usati qui solo per dire con che cosa funziona questo.

## Sviluppo

Compilare sul visore:

```sh
cmake -G Ninja -S . -B build && ninja -C build
./build/frame-mic-tuner --help
scripts/package.sh   # build di release: dist/frame-mic-tuner-<version>.tar.gz, dist/SHA256SUMS
```

Opzioni utili che non hanno bisogno di SteamVR: `--print`, `--set-ns-vad N` / `--set-ns-grace N` (applicare subito un'intensità del filtro rumore senza salvarla), `--dump-png PATH` (disegnare il pannello, con stati `--fake-*` per gli screenshot, come `--fake-output`, `--fake-switched`, `--fake-external-mic`, `--fake-update STATE`, e `--view` / `--overlay` per le altre schermate), `--test-record 3` (registrare 3 secondi e riprodurli), `--contrast-report` (contrasto WCAG di ogni coppia di colori) e `--version`. Note di progetto, come funziona lo script di cambio e risultati dei test sono in [docs/DEVELOPMENT.ja.md](docs/DEVELOPMENT.ja.md) (giapponese).

Il meccanismo di aggiornamento (`vendor/frame-updater/`), le parti di interfaccia condivise (`vendor/frame-ui/`) e l'elenco delle altre app dell'autore (`vendor/frame-apps/`) sono copie di repository condivisi e privati; non modificare le copie a mano. Il loro `verify.sh` (eseguito da `scripts/package.sh`) controlla che non siano state cambiate.

Le build Release mantengono l'ottimizzazione di velocità del compilatore e attivano, quando possibile, l'ottimizzazione in fase di link e la rimozione di codice e dati inutilizzati. Dopo la compilazione, `ctest --test-dir build --output-on-failure` esegue il `--self-test` esistente e i test esaustivi della conversione dei pixel, senza SteamVR né hardware audio. `./build/rgba-test --benchmark` confronta anche il tempo di conversione con l'algoritmo precedente. I test non sono inclusi nei pacchetti di release; configura con `-DBUILD_TESTING=OFF` per non compilarli.

### Pubblicare

`scripts/package.sh` esegue il `verify.sh` delle tre copie sotto `vendor/`, compila un binario Release e scrive `dist/frame-mic-tuner-<version>.tar.gz` e `dist/SHA256SUMS` (necessario perché il pulsante **Aggiorna** del pannello accetti la versione). Stampa il comando che crea la release di GitHub (e il suo tag) con entrambi i file allegati; scrivi prima le note di release in un file fuori dal repository:

```sh
gh release create v<version> dist/frame-mic-tuner-<version>.tar.gz dist/SHA256SUMS --title v<version> --notes-file <file-delle-note>
```

Non farne una bozza né una prerelease: il pannello guarda l'ultima release di GitHub, che salta entrambe.

## Licenza

MIT. Vedi [LICENSE](LICENSE). L'header OpenVR incluso (`third_party/openvr/`) è BSD-3-Clause di Valve Corporation. L'icona della lingua (`assets/heroicons-language-16-solid.svg`, inclusa anche nel programma) viene da [Heroicons](https://github.com/tailwindlabs/heroicons) di Tailwind Labs, MIT. `vendor/frame-updater/`, `vendor/frame-ui/` e `vendor/frame-apps/` (icone delle altre app incluse nel programma comprese) sono copie del codice dell'autore e sono MIT come il resto di questo repository. Le licenze dell'header incluso e delle librerie che usa sono in [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md). I cambiamenti sono in [CHANGELOG.md](CHANGELOG.md).
