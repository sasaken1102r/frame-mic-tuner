# Frame Mic Tuner

Un panel del panel de SteamVR para Steam Frame que cambia la cancelación de eco y el filtro de ruido del micrófono del visor mientras lo llevas puesto. Apaga la cancelación de eco con auriculares, para que los sonidos bajos, como los de la boca, se oigan claros, y enciéndela con los altavoces de Frame, para que su sonido no entre en el micrófono. Una prueba de voz integrada graba unos segundos de lo que las apps oyen de verdad, para comparar los ajustes al momento.

[English](README.md) · [日本語](README.ja.md) · [简体中文](README.sc.md) · [Deutsch](README.de.md) · [Français](README.fr.md) · [Italiano](README.it.md)

| Rápido (preajustes) | Ajuste fino |
|---|---|
| ![La pestaña Rápido](docs/v12-en-quick-speaker_2026-10-18_19-50-00.png) | ![La pestaña Ajuste fino](docs/v12-en-fine_2026-10-18_19-50-00.png) |

### Vídeo de demostración (con sonido)

Los altavoces de Frame reproducen un sonido de ventisca mientras la prueba de voz graba, y luego se oyen las dos grabaciones. 1.er clip: filtro de ruido apagado (preajuste de altavoz). 2.º clip: cancelación de eco también apagada (preajuste de auriculares). Sin cancelación de eco, la ventisca de los altavoces entra en el micrófono. El panel del vídeo está en japonés.

https://github.com/user-attachments/assets/14b5e180-f417-4d90-85f5-6ba1dc573546

## Qué hace

- **Ajustes para cada salida**: el panel recuerda los ajustes del micrófono de Frame (cancelación de eco, filtro de ruido y su intensidad) por separado para cada salida de audio: los altavoces de Frame, auriculares o adaptadores USB-C, auriculares Bluetooth. Cuando cambia la salida (por ejemplo conectas auriculares y pasan a ser la salida), la app pone sola los ajustes de esa salida, aunque el panel esté cerrado, y el panel dice «%s conectado, así que vale su ajuste guardado» con **Deshacer**. Una salida que no ha visto empieza con el preajuste **Altavoces de Frame** si son los altavoces de Frame, y con **Auriculares** en los demás casos.
- **Salida y micrófono**: el botón **Salida** de arriba lista las salidas conectadas y las que usaste antes. Ahí puedes **Reproducir aquí** (la hace la salida predeterminada, el mismo ajuste que el audio de SteamVR), elegir cualquier salida para ver y cambiar sus ajustes por adelantado, u **Olvidar** una que ya no uses. La lista se desplaza con el stick o arrastrando. **Micrófono** elige qué micrófono usan las apps. La cancelación de eco y el filtro de ruido solo actúan en el micrófono integrado de Frame; con otro micrófono el panel lo dice y ofrece **Usar el micrófono integrado**.
- **Rápido y Ajuste fino**: **Rápido** tiene los preajustes de cómo escuchas. **Auriculares** apaga la cancelación de eco y el filtro de ruido. **Altavoces de Frame** enciende la cancelación de eco y apaga el filtro de ruido. **Ajuste fino** tiene los interruptores uno a uno y dos controles del filtro de ruido: **Sensibilidad** (cuánto tiene que parecerse un sonido a una voz para pasar; más bajo deja pasar más) y **Retención** (cuánto tiempo sigue pasando el sonido después de hablar). Los cambios se aplican al momento, también mientras el micrófono está en uso, sin cortar el sonido. El panel se abre en la pestaña que usaste la última vez.
- **Ruta de la señal**: muestra si una app usa el micrófono y por qué filtros pasa el sonido de verdad (micrófono → EQ → cancelación de eco → filtro de ruido → apps), leído de los enlaces de PipeWire en marcha.
- **Silencio**: el encabezado muestra «En uso», «Sin uso» o un «Silenciado» rojo (silenciado desde Steam, `wpctl`, un botón aux o cualquier otro sitio), y justo al lado **Silenciar** / **Quitar silencio**.
- **Prueba de voz**: graba hasta 10 segundos del sonido final que reciben las apps, guarda las últimas 5 grabaciones y las reproduce por la salida actual. Cada grabación muestra la hora, la duración, la salida y el ajuste con el que se hizo (por ejemplo «AB13X · Eco apagado») y una forma de onda pequeña.
- **Apps y actualizaciones** (botón arriba a la derecha; un punto rosa quiere decir que hay algo que mirar): la versión en marcha, **Check now** y **Update**, el interruptor **Buscar actualizaciones**, y las otras apps de Steam Frame del autor. La que trabaja con esta, frame-aux-shortcuts (cambia el silencio del micrófono con el botón aux), va la primera; las demás están a un toque. Instalar abre el instalador en Konsole después de mostrarte el comando exacto.
- Interfaz en japonés, inglés, chino simplificado, alemán, francés, español e italiano. El texto y los controles cumplen el contraste WCAG 2.x AA.

Por qué hace falta: mientras una app usa el micrófono, SteamOS lo pasa por el EQ, la cancelación de eco y el filtro de ruido. La cancelación de eco reduce mucho los sonidos pequeños, y el filtro de ruido calla todo lo que no suena a voz. Con auriculares no hay sonido de altavoz que cancelar, así que apagar la cancelación de eco deja pasar más de tu voz.

## Requisitos

- Un Steam Frame con el modo de desarrollador y SSH (Ajustes > Sistema > Modo de desarrollador, y luego una contraseña en Desarrollador). Elige una contraseña fuerte: con SSH, cualquiera de tu red que la sepa puede entrar en el visor.
- SteamVR en el visor. Compilar desde el código (más abajo) necesita cmake, ninja, g++, pkg-config y los archivos de desarrollo de cairo, FreeType, PipeWire y Vulkan, que ya vienen con SteamOS. Una versión descargada no necesita ninguno.

## Instalación

### Lo más fácil: instalar dentro del Frame (recomendado)

No hace falta un PC. En Konsole en el Frame (+ en la barra de abajo → la lista de programas → Konsole), escribe este comando, pulsa Intro y elige **3** (frame-mic-tuner) en el menú.

```sh
curl -fsSL https://frame.sasaken1102s.net | sh
```

- Haz esto una vez antes: Ajustes de Steam → Sistema → activa «Enable Developer Mode» (mientras esté apagado, Konsole no sale en la lista +).
- Las otras apps (frameeyeosc, frame-jp-keyboard, frame-perf-overlay, frame-aux-shortcuts) se instalan desde el mismo menú.
- Puedes leer el script antes: `curl -fsSL https://frame.sasaken1102s.net/i | less`.
- Para actualizar, el mismo comando y el mismo número. Para desinstalar, `u` en el menú.
- Guía paso a paso y vídeo: https://frame.sasaken1102s.net
- Sin preguntas: `curl -fsSL https://frame.sasaken1102s.net | sh -s -- install mic`

Lo que se instala y las opciones son los mismos que en «Instalar desde un PC» (ejecuta `install.sh` por ti).

### Instalar desde un PC

Descarga `frame-mic-tuner-<version>.tar.gz` de la [página de versiones](https://github.com/sasaken1102r/frame-mic-tuner/releases) y cópialo al visor, por ejemplo desde tu PC:

```sh
scp frame-mic-tuner-*.tar.gz steamos@<ip-del-visor>:
```

Luego en el visor (`ssh steamos@<ip-del-visor>`):

```sh
tar xzf frame-mic-tuner-*.tar.gz
cd frame-mic-tuner
./install.sh
```

O compílalo desde el código (también en el visor):

```sh
git clone https://github.com/sasaken1102r/frame-mic-tuner.git
cd frame-mic-tuner
./install.sh
```

Después **reinicia el visor** una vez, para que WirePlumber cargue el script de cambio. No reinicies PipeWire ni WirePlumber a mano mientras juegas: SteamVR y Steam Link pierden el sonido. Si pasa de todos modos, reinicia SteamVR en el visor (o vuelve a conectar Steam Link) y el sonido regresa.

No hace falta sudo. `install.sh` lo pone todo en tu carpeta personal (`~/.local/bin`, `~/.local/share`, `~/.config`), así que las actualizaciones de SteamOS no lo quitan. Para actualizar a mano: descarga la versión nueva y vuelve a ejecutar su `install.sh` (o, en una copia del código, `git pull && ./install.sh`). O pulsa **Actualizar** en **Apps y actualizaciones** cuando haya una versión más nueva. La versión 0.1.0 no tiene botón de actualización, así que el paso de 0.1.0 a 0.2.0 hay que hacerlo a mano esta única vez.

Qué se instala:

- la app, una entrada para la lista **+** del panel (iniciar un programa) y sus iconos
- una unidad de usuario de systemd para arrancarla con SteamVR (no queda activada hasta que tú la enciendas; ver más abajo)
- un script y una configuración de WirePlumber (`contrib/wireplumber/`) que sustituyen el seguimiento del micrófono de SteamOS por uno cuya cancelación de eco y filtro de ruido se pueden cambiar. Por lo demás se comporta como el original.
- el script de actualización compartido (`~/.local/share/frame-mic-tuner/frame-update.sh`, de `vendor/frame-updater/`), que usa **Apps y actualizaciones**

Para quitarlo: `./install.sh --uninstall`, y luego reinicia el visor para volver al comportamiento original del micrófono de SteamOS. Añade `--purge` para borrar también los ajustes de la app y los valores de cambio guardados.

## Uso

1. Abre el panel de SteamVR, pulsa **+** (iniciar un programa) y elige **Frame Mic Tuner**. Aparece un icono **Mic** en la fila de abajo del panel; al elegirlo se abre el panel de la app. Volver a iniciarla mientras está en marcha solo abre el panel.
2. El botón **Salida** muestra la salida por la que escuchas. En la pestaña **Rápido**, bajo «Al escuchar por <salida>», elige **Auriculares** o **Altavoces de Frame**. Son preajustes: ponen la cancelación de eco y el filtro de ruido en los valores recomendados (las fichas de cada tarjeta los muestran); la intensidad del filtro de ruido se queda. La app recuerda la elección para esa salida, así que la próxima vez cambia sola. Para un control más fino, pasa a **Ajuste fino**. Si entonces tus ajustes no coinciden con ningún preajuste, **Rápido** dice «Ahora se usan los ajustes finos». El filtro de ruido corta los sonidos bajos (de la boca y parecidos) cuando está activado.
   - En **Ajuste fino**, arrastra **Sensibilidad** y **Retención** para afinar el filtro de ruido. Los valores de SteamOS son 23 % y 500 ms; **Predeterminado** los devuelve. Los controles solo actúan mientras el filtro de ruido está activado.
   - SteamOS restablece estos dos valores cada vez que su audio se reinicia (por ejemplo tras un reinicio). La app guarda tus valores para cada salida y los vuelve a aplicar al arrancar y cuando cambia la salida, así que solo se mantienen mientras la app está en marcha. Activa **Iniciar con SteamVR** si los quieres siempre. Sin la app se usan los valores de SteamOS.
   - Después de que la app cambie los ajustes sola, **Rápido** dice «<salida> conectado, así que vale su ajuste guardado». **Deshacer** vuelve a poner lo que había antes y lo recuerda para esta salida en su lugar.
3. **Salida** (arriba a la izquierda): abre «Elegir la salida». Pulsa una fila para ver los ajustes de esa salida (puedes cambiar los de una salida que ahora no está conectada; se usan cuando vuelve). **Reproducir aquí** hace esa salida la predeterminada, como el audio de SteamVR. **Olvidar** borra los ajustes recordados de una salida que no está conectada. Desplaza con el stick o arrastrando.
4. **Micrófono** (junto a Salida): **Usar este micrófono** elige el micrófono del que graban las apps. Con cualquier cosa que no sea el micrófono integrado de Frame, la cancelación de eco y el filtro de ruido no se aplican, y la tarjeta de ajustes ofrece **Usar el micrófono integrado**.
5. **Silenciar** / **Quitar silencio** (arriba, junto al estado): silencia o quita el silencio del micrófono predeterminado al momento, sin preguntar.
6. **Prueba de voz**: pulsa **Grabar**, di algo y pulsa **Parar** (se para solo a los 10 segundos). Cambia el ajuste, graba otra vez y usa ▶ en cada fila para comparar. La grabación se para en cuanto cierras el panel.
7. **Iniciar con SteamVR** (fila de abajo): actívalo para que la app arranque sola con SteamVR a partir de ahora. También puedes activarlo con `./install.sh --autostart`.
8. **Idioma**: el panel arranca en el idioma de tu Steam Frame (japonés, chino simplificado, alemán, francés, español o italiano si Steam está en ese idioma, inglés en los demás casos). Cámbialo con el menú de abajo a la izquierda (日本語, English, 简体中文, Deutsch, Français, Español, Italiano); la elección se guarda.
9. **Salir**: púlsalo dos veces, o pasa por el icono Mic del panel y elige Cerrar.
10. **Apps y actualizaciones** (arriba a la derecha; **Ajustes del micrófono** vuelve atrás). Los textos y botones de esa caja siguen en inglés. **Esta app** muestra la versión en marcha, por ejemplo «Up to date (0.3.5)». **Check now** busca una versión más nueva al momento (funciona aunque **Buscar actualizaciones** esté apagado).
    - Cuando hay una, la caja toma un borde del color de acento y dice «Version X is available». Pulsa **Update**; pregunta «Update to X?» con **Cancel** y **Update** (la pregunta se va sola a los 3 segundos). Pulsa **Update** otra vez para empezar. La caja muestra entonces el avance («Updating: Downloading», y así).
    - El panel sigue abierto y con la versión vieja mientras instala. Cuando dice «X is installed. Quit and start the app again to use it», pulsa **Salir** dos veces y vuelve a iniciar la app desde **+** (o reinicia SteamVR), y luego pulsa **Close** para cerrar el mensaje si sigue ahí. Si la versión nueva cambió el script de WirePlumber, reinicia también el visor (la caja te lo recuerda, y el registro de la actualización dice si cambió).
    - Si dice «This version can't be installed from here» (la versión no tiene `SHA256SUMS` o no tiene paquete), actualiza a mano como en Instalación; la caja muestra la página de la versión.
    - Si la actualización falla, la caja toma un borde rojo, dice por qué y que no se cambió nada, y ofrece **Try again** y **Close**. Si solo falla la comprobación (por ejemplo sin red), solo el texto se pone rojo y **Check now** sigue.
    - **Buscar actualizaciones** activado / apagado: apagado para la comprobación diaria y la descarga de la lista de apps (ver Privacidad).
    - **Apps de apoyo** muestra frame-aux-shortcuts (cambia el silencio del micrófono con el botón aux) con **Instalar** si falta. «Apps de Frame de sasaken@» abre la lista completa. **Instalar** y **Abrir el instalador** muestran primero el comando exacto; **Abrir en Konsole** lo ejecuta luego en Konsole, donde el instalador muestra el avance. Sin sudo; todo va a la carpeta personal.

Los ajustes también están por SSH:

```sh
wpctl settings --save frame-mic.echo-cancel false        # auriculares: cancelación de eco apagada
wpctl settings --save frame-mic.echo-cancel true         # altavoz: cancelación de eco activada (predeterminado)
wpctl settings --save frame-mic.noise-suppression true   # filtro de ruido activado (predeterminado: apagado)
~/.local/bin/frame-mic-tuner --print                     # valores actuales, micrófono en uso, ruta, salidas y sus ajustes guardados
```

Un cambio hecho así (o con un botón aux) se nota mientras el panel está abierto, o como tarde cuando cambia la salida, y se recuerda para la salida actual.

## Si algo no va

- **El panel dice «El cambio de micrófono no está instalado»**: el script de WirePlumber aún no está cargado. Ejecuta `./install.sh` y reinicia el visor.
- **Los botones de inicio automático están en gris**: la unidad de systemd no está instalada. Ejecuta `./install.sh`.
- **Las grabaciones están en silencio, o los demás no te oyen**: comprueba el volumen del micrófono en el mezclador del visor (solo lectura; es seguro):
  ```sh
  amixer -c 0 cget name='VA_DEC0 Volume'
  amixer -c 0 cget name='VA_DEC1 Volume'
  ```
  Los dos deberían decir `values=96`. Si dicen `0`, el micrófono está prácticamente silenciado. Esta app nunca toca el mezclador, pero se ha visto que bajan a 0 solos. Para devolverlos (bajo tu responsabilidad; anota antes los valores actuales y el `max=` que muestra `cget`, y usa el valor habitual de tu visor, 96 en el nuestro):
  ```sh
  amixer -c 0 cset name='VA_DEC0 Volume' 96
  amixer -c 0 cset name='VA_DEC1 Volume' 96
  ```
- **Ningún sonido en SteamVR o Steam Link después de reiniciar PipeWire o WirePlumber**: reinicia SteamVR en el visor (o vuelve a conectar Steam Link). El sonido vuelve.
- **Ningún sonido después de una actualización de SteamOS**: el script de cambio se carga como parte necesaria de WirePlumber, así que si una actualización lo rompe, WirePlumber puede pararse y llevarse todo el sonido. Quita la configuración por SSH y reinicia el visor (o ejecuta `./install.sh --uninstall` y reinicia):
  ```sh
  rm ~/.config/wireplumber/wireplumber.conf.d/90-frame-mic.conf
  ```
- **«En uso» aunque no hables en ninguna app**: cuenta cualquier app que grabe audio, incluidos los grabadores de pantalla o de sonido. SteamOS enciende los filtros para todas.
- **Frame Mic Tuner no está en la lista +**: reinicia el visor una vez después de instalar.
- **Falta el icono Mic en el panel aunque la app está en marcha**: SteamVR puede perder el panel de la app (por ejemplo cuando el propio panel se reinicia). La app comprueba cada 3 segundos y vuelve a crear el panel sola; espera unos segundos y abre el panel otra vez. Volver a iniciarla desde **+** también comprueba y lo recrea antes de abrir el panel. Si aun así no vuelve, el registro (`journalctl --user -u frame-mic-tuner -f` cuando corre como servicio) dice por qué (el registro está en japonés; busca líneas que empiezan por «自己修復», que significa «autorreparación»); salir de la app y volver a iniciarla también lo arregla.
- Registros: `journalctl --user -u frame-mic-tuner -f` cuando corre como servicio. Para ver el registro de un arranque manual, sal de la app e inicia `~/.local/bin/frame-mic-tuner` por SSH.
- **La búsqueda de actualizaciones falla, o «Update» no hace nada**: `~/.cache/frame-mic-tuner/update.log` tiene el detalle, y `~/.cache/frame-mic-tuner/update-check.json` / `update-state.json` la última respuesta en bruto. Apagar **Buscar actualizaciones** (o `update_check: false` en el archivo de ajustes) para la comprobación diaria automática sin quitar **Check now** / **Update**.
- **Los ajustes no cambiaron al conectar auriculares**: la app solo sigue la salida *predeterminada*. Si SteamVR sigue sonando por los altavoces de Frame, pulsa **Reproducir aquí** en los auriculares dentro de **Salida** (o elígelos en el audio de SteamVR). Las salidas que no son aparatos de verdad (filtros, salidas virtuales) se ignoran.
- **«No se pudo abrir Konsole» al instalar otra app**: Konsole necesita el modo de desarrollador (Ajustes de Steam → Sistema → Enable Developer Mode).

## Privacidad

- Las grabaciones de la prueba de voz se quedan solo en la memoria de la app. Nunca se escriben en el disco ni en registros, y nunca se envían a ningún sitio. Desaparecen cuando la app sale.
- Mientras graba, el panel muestra «● Grabando». La grabación se para en cuanto cierras el panel.
- El micrófono puede recoger las voces de la gente a tu alrededor. Tenlo en cuenta antes de grabar.
- Para elegir el idioma predeterminado lee una vez al arrancar la línea `language` del `~/.steam/registry.vdf` de Steam (solo lectura).
- La app no tiene telemetría. Su uso de la red es la búsqueda de actualizaciones y la lista de apps (abajo):
  - **Cuándo**: mientras la app está en marcha, pregunta a GitHub al arrancar si su última respuesta tiene más de 24 horas, y después más o menos una vez al día (una hora después si la comprobación falló). **Comprobar ahora** pregunta al momento. Apagar **Buscar actualizaciones** en **Apps y actualizaciones** (o `update_check: false` en el archivo de ajustes) para la comprobación automática; entonces solo se conecta cuando pulsas **Comprobar ahora** o **Actualizar**.
  - **Qué envía**: una petición HTTPS de la última versión de este proyecto a `api.github.com`, sin cuenta, identificador ni datos de uso. Como cualquier petición web, GitHub ve tu dirección IP y el User-Agent de `curl`. Tus grabaciones, ajustes y registros nunca se envían.
  - **Qué recibe**: el número de versión de la última release, su página y los nombres y enlaces de sus archivos. La respuesta se guarda en `~/.cache/frame-mic-tuner/update-check.json`.
  - **Solo cuando pulsas Update y confirmas**, también descarga el paquete de esa versión y `SHA256SUMS` de `github.com` / `*.githubusercontent.com`.
- Con **Buscar actualizaciones** activado, también trae la lista de las otras apps de Steam Frame del autor y sus iconos de `frame.sasaken1102s.net` (como mucho una vez por hora, compartido con las otras apps del autor; se guarda en `~/.cache/frame-apps/`). Es un GET HTTPS sencillo sin nada añadido (`curl -q --proto =https --max-redirs 0`): igual que GitHub, `frame.sasaken1102s.net` ve tu dirección IP y el User-Agent de `curl`, y no se envía nada más. Apagar **Buscar actualizaciones** para esto también, y el panel muestra entonces la última lista traída o la que viene dentro de la app. Cuando instalas otra app desde **Apps y actualizaciones**, se abre Konsole y ejecuta el comando del instalador mostrado en la confirmación, que se conecta a `frame.sasaken1102s.net` y a GitHub dentro de Konsole.
- Los únicos archivos que escribe son `~/.config/frame-mic-tuner/config.json` (idioma de la interfaz, última pestaña, el ajuste de búsqueda, y para cada salida usada su nombre, los ajustes del micrófono de Frame y la fecha del último uso), `~/.config/frame-mic-tuner/install-args` (las opciones de tu último `./install.sh`, para que una actualización reinstale igual), `~/.cache/frame-mic-tuner/` (la respuesta en caché de la búsqueda, el registro y el estado de la actualización, una carpeta de bloqueo `update.lock/` mientras hay una actualización, y una carpeta de trabajo cuya descarga se borra después de cada actualización), `~/.cache/frame-apps/` (la lista de apps y los iconos traídos, compartidos con las otras apps del autor) y un archivo de bloqueo en `$XDG_RUNTIME_DIR` que guarda su identificador de proceso (para que no arranque una segunda copia). Una actualización también reescribe los archivos instalados, igual que ejecutar `./install.sh`.

## Aviso

- Úsalo bajo tu responsabilidad. Este proyecto se hizo con Claude Opus 5.5, un modelo de IA. Lo he probado en mi propio Steam Frame, pero no puedo responder de lo que pase en el tuyo: lee el código y revísalo tú antes de ejecutarlo. El software se entrega sin garantía (ver [LICENSE](LICENSE)).
- Qué cambia: los dos ajustes de WirePlumber `frame-mic.echo-cancel` y `frame-mic.noise-suppression` (con `wpctl settings`), los dos parámetros en marcha del filtro de ruido «VAD Threshold (%)» y «VAD Grace Period (ms)» (con `pw-cli set-param`; SteamOS devuelve sus valores cuando su audio se reinicia) y la configuración de WirePlumber en tu carpeta personal, donde sustituye el seguimiento del micrófono de SteamOS por su propio script. Cuando pulsas **Reproducir aquí**, **Usar este micrófono** / **Usar el micrófono integrado** o **Silenciar** / **Quitar silencio**, cambia la salida predeterminada, el micrófono predeterminado o su silencio (con `wpctl set-default` / `wpctl set-mute`, como hace el audio de SteamVR). Aparte de eso, solo activa o desactiva su propia unidad de usuario de systemd cuando se lo pides, abre Konsole con el instalador cuando confirmas instalar otra app y, al actualizar desde el panel, ejecuta el `install.sh` de la versión nueva en una unidad de usuario de systemd de corta duración (`frame-mic-tuner-update`).
- Nunca reinicia PipeWire ni WirePlumber, nunca escribe en el mezclador ALSA (amixer), nunca toca `/etc` y nunca necesita root.
- **Update**: descarga el `.tar.gz` y el `SHA256SUMS` de la última versión desde GitHub por HTTPS (solo a los servidores de GitHub), comprueba la suma y solo entonces lo extrae en `~/.cache/frame-mic-tuner/update/` y ejecuta su `install.sh` con las mismas opciones que tu última instalación. Rechaza una versión sin `SHA256SUMS`, una suma que no coincide, o un archivo que contiene una ruta absoluta, `..`, un enlace o un archivo especial; no se cambia nada si ocurre cualquiera de eso. La descarga se borra después. `SHA256SUMS` es una suma de comprobación publicada en la misma versión, no una firma: detecta una descarga dañada o incompleta, pero no una versión que haya sido sustituida en GitHub. La app en marcha no se reinicia: sal y vuelve a iniciarla para usar la versión nueva. Los pasos exactos están en las [notas de frame-update.sh](vendor/frame-updater/frame-update.sh).
- Una actualización de SteamOS puede cambiar cómo están montados los filtros del micrófono. Entonces los cambios pueden dejar de funcionar hasta que este proyecto se actualice. Como el script de cambio es una parte necesaria de WirePlumber, un script que ya no carga también puede parar todo el sonido del visor (ver Si algo no va). `./install.sh --uninstall` y un reinicio devuelven el comportamiento original de SteamOS.
- Es un proyecto no oficial, sin relación ni respaldo de Valve Corporation. Steam, Steam Frame, SteamOS, SteamVR y Steam Link son marcas y/o marcas registradas de Valve Corporation en EE. UU. y/u otros países. Los nombres se usan aquí solo para decir con qué funciona esto.

## Desarrollo

Compilar en el visor:

```sh
cmake -G Ninja -S . -B build && ninja -C build
./build/frame-mic-tuner --help
scripts/package.sh   # compilación de release: dist/frame-mic-tuner-<version>.tar.gz, dist/SHA256SUMS
```

Opciones útiles que no necesitan SteamVR: `--print`, `--set-ns-vad N` / `--set-ns-grace N` (aplicar al momento una intensidad del filtro de ruido sin guardarla), `--dump-png PATH` (dibujar el panel, con estados `--fake-*` para capturas, como `--fake-output`, `--fake-switched`, `--fake-external-mic`, `--fake-update STATE`, y `--view` / `--overlay` para las otras pantallas), `--test-record 3` (grabar 3 segundos y reproducirlos), `--contrast-report` (contraste WCAG de cada par de colores) y `--version`. Las notas de diseño, cómo funciona el script de cambio y los resultados de las pruebas están en [docs/DEVELOPMENT.ja.md](docs/DEVELOPMENT.ja.md) (japonés).

El mecanismo de actualización (`vendor/frame-updater/`), las partes de interfaz compartidas (`vendor/frame-ui/`) y la lista de las otras apps del autor (`vendor/frame-apps/`) son copias de repositorios compartidos y privados; no edites las copias a mano. Su `verify.sh` (lo ejecuta `scripts/package.sh`) comprueba que no se hayan cambiado.

Las compilaciones Release mantienen la optimización de velocidad del compilador y activan, cuando es posible, la optimización en tiempo de enlace y la eliminación de código y datos sin usar. Después de compilar, `ctest --test-dir build --output-on-failure` ejecuta el `--self-test` de siempre y las pruebas exhaustivas de conversión de píxeles, sin SteamVR ni hardware de audio. `./build/rgba-test --benchmark` además compara el tiempo de conversión con el algoritmo anterior. Las pruebas no se incluyen en los paquetes de versión; configura con `-DBUILD_TESTING=OFF` para no compilarlas.

### Publicar

`scripts/package.sh` ejecuta el `verify.sh` de las tres copias bajo `vendor/`, compila un binario Release y escribe `dist/frame-mic-tuner-<version>.tar.gz` y `dist/SHA256SUMS` (necesario para que el botón **Actualizar** del panel acepte la versión). Imprime el comando que crea la versión de GitHub (y su etiqueta) con los dos archivos adjuntos; escribe antes las notas de la versión en un archivo fuera del repositorio:

```sh
gh release create v<version> dist/frame-mic-tuner-<version>.tar.gz dist/SHA256SUMS --title v<version> --notes-file <archivo-de-notas>
```

No lo hagas borrador ni versión preliminar: el panel mira la última versión de GitHub, que se salta las dos.

## Licencia

MIT. Ver [LICENSE](LICENSE). La cabecera OpenVR incluida (`third_party/openvr/`) es BSD-3-Clause de Valve Corporation. El icono de idioma (`assets/heroicons-language-16-solid.svg`, también integrado en el programa) es de [Heroicons](https://github.com/tailwindlabs/heroicons) de Tailwind Labs, MIT. `vendor/frame-updater/`, `vendor/frame-ui/` y `vendor/frame-apps/` (incluidos los iconos de las otras apps integrados en el programa) son copias del propio código del autor y son MIT como el resto de este repositorio. Las licencias de la cabecera incluida y de las bibliotecas que usa están en [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md). Los cambios están en [CHANGELOG.md](CHANGELOG.md).
