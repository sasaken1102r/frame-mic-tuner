# Frame Mic Tuner

Un panneau du tableau de bord SteamVR pour le Steam Frame, qui bascule l'annulation d'écho et le filtre de bruit du micro du casque pendant que tu le portes. Coupe l'annulation d'écho avec des écouteurs, pour que les sons faibles comme les bruits de bouche passent clairement, et remets-la avec les haut-parleurs du Frame, pour que leur son n'entre pas dans le micro. Un test de voix intégré enregistre quelques secondes de ce que les apps entendent vraiment, pour comparer les réglages sur place.

[English](README.md) · [日本語](README.ja.md) · [简体中文](README.sc.md) · [Deutsch](README.de.md) · [Español](README.es.md) · [Italiano](README.it.md)

| Rapide (préréglages) | Réglage fin |
|---|---|
| ![L'onglet Rapide](docs/v12-en-quick-speaker_2026-10-18_19-50-00.png) | ![L'onglet Réglage fin](docs/v12-en-fine_2026-10-18_19-50-00.png) |

### Vidéo de démo (avec le son)

Les haut-parleurs du Frame jouent un bruit de blizzard pendant que le test de voix enregistre, puis les deux enregistrements sont lus. 1er extrait : filtre de bruit arrêté (préréglage haut-parleurs). 2e extrait : annulation d'écho arrêtée aussi (préréglage écouteurs). Sans annulation d'écho, le blizzard des haut-parleurs entre dans le micro. Le panneau dans la vidéo est en japonais.

https://github.com/user-attachments/assets/14b5e180-f417-4d90-85f5-6ba1dc573546

## Ce qu'il fait

- **Réglages pour chaque sortie** : le panneau retient les réglages du micro Frame (annulation d'écho, filtre de bruit et sa force) séparément pour chaque sortie audio : les haut-parleurs du Frame, des écouteurs ou adaptateurs USB-C, un casque Bluetooth. Quand la sortie change (par exemple tu branches des écouteurs et ils deviennent la sortie), l'app applique toute seule les réglages de cette sortie, même panneau fermé, et le panneau dit « %s est branché, donc son réglage enregistré est actif » avec **Annuler**. Une sortie jamais vue part du préréglage **Haut-parleurs Frame** si ce sont les haut-parleurs du Frame, sinon de **Écouteurs / casque**.
- **Sortie et micro** : le bouton **Sortie** en haut liste les sorties branchées et celles que tu as déjà utilisées. Là tu peux **Jouer le son ici** (en fait la sortie par défaut, le même réglage que l'audio de SteamVR), choisir une sortie pour voir et changer ses réglages à l'avance, ou **Oublier** une sortie que tu n'utilises plus. La liste défile au stick ou en glissant. **Micro** choisit le micro que les apps utilisent. L'annulation d'écho et le filtre de bruit ne marchent que sur le micro intégré du Frame ; avec un autre micro, le panneau le dit et propose **Utiliser le micro intégré**.
- **Rapide et Réglage fin** : **Rapide** a les préréglages selon ta façon d'écouter. **Écouteurs / casque** coupe l'annulation d'écho et le filtre de bruit. **Haut-parleurs Frame** met l'annulation d'écho en marche et le filtre de bruit à l'arrêt. **Réglage fin** a les interrupteurs un par un et deux curseurs pour le filtre de bruit : **Sévérité** (à quel point un son doit ressembler à une voix pour passer ; plus bas laisse passer plus) et **Tenue** (combien de temps le son continue après que tu arrêtes de parler). Les changements s'appliquent tout de suite, même pendant que le micro sert, sans couper le son. Le panneau s'ouvre sur le dernier onglet utilisé.
- **Chemin du signal** : montre si une app utilise le micro et par quels filtres le son passe vraiment (micro → EQ → annulation d'écho → filtre de bruit → apps), lu dans les liens PipeWire en cours.
- **Muet** : l'en-tête montre « En cours », « Inutilisé » ou un « Muet » rouge (coupé depuis Steam, `wpctl`, un bouton aux ou ailleurs), et juste à côté **Couper le son** / **Rétablir le son**.
- **Test de voix** : enregistre jusqu'à 10 secondes du son final que les apps reçoivent, garde les 5 derniers enregistrements et les joue sur la sortie actuelle. Chaque enregistrement montre l'heure, la durée, la sortie et le réglage utilisés (par exemple « AB13X · Annulation d'écho : arrêt ») et une petite forme d'onde.
- **Apps et mises à jour** (bouton en haut à droite ; un point rose veut dire qu'il y a quelque chose à voir) : la version en cours, **Check now** et **Update**, l'interrupteur **Vérifier les mises à jour**, et les autres apps Steam Frame de l'auteur. Celle avec laquelle cette app travaille, frame-aux-shortcuts (bascule le mute du micro avec le bouton aux), est en premier ; les autres sont à un appui. Installer ouvre l'installateur dans Konsole après t'avoir montré la commande exacte.
- Interface en japonais, anglais, chinois simplifié, allemand, français, espagnol et italien. Le texte et les commandes respectent le contraste WCAG 2.x AA.

Pourquoi c'est utile : tant qu'une app utilise le micro, SteamOS le fait passer par l'EQ, l'annulation d'écho et le filtre de bruit. L'annulation d'écho réduit fortement les petits sons, et le filtre de bruit coupe tout ce qui ne ressemble pas à une voix. Avec des écouteurs il n'y a pas de son de haut-parleur à annuler, donc couper l'annulation d'écho laisse passer plus de ta voix.

## Prérequis

- Un Steam Frame avec le mode développeur et l'accès SSH (Paramètres > Système > Mode développeur, puis un mot de passe sous Développeur). Choisis un mot de passe solide : avec SSH, toute personne de ton réseau qui le connaît peut se connecter au casque.
- SteamVR sur le casque. Compiler depuis les sources (voir plus bas) demande cmake, ninja, g++, pkg-config et les fichiers de développement cairo, FreeType, PipeWire et Vulkan, déjà fournis par SteamOS. Une version téléchargée n'en a besoin d'aucun.

## Installation

### Le plus simple : installer dans le Frame (recommandé)

Pas besoin de PC. Dans Konsole sur le Frame (+ sur la barre du bas → la liste des programmes → Konsole), tape cette commande, appuie sur Entrée, et choisis **3** (frame-mic-tuner) dans le menu.

```sh
curl -fsSL https://frame.sasaken1102s.net | sh
```

- Une fois d'abord : Paramètres Steam → Système → activer « Enable Developer Mode » (tant que c'est arrêté, Konsole n'apparaît pas dans la liste +).
- Les autres apps (frameeyeosc, frame-jp-keyboard, frame-perf-overlay, frame-aux-shortcuts) s'installent depuis le même menu.
- Tu peux lire le script avant : `curl -fsSL https://frame.sasaken1102s.net/i | less`.
- Pour mettre à jour, la même commande et le même numéro. Pour désinstaller, `u` dans le menu.
- Guide pas à pas et vidéo : https://frame.sasaken1102s.net
- Sans questions : `curl -fsSL https://frame.sasaken1102s.net | sh -s -- install mic`

Ce qui est installé et les options sont les mêmes que dans « Installer depuis un PC » (ça lance `install.sh` pour toi).

### Installer depuis un PC

Télécharge `frame-mic-tuner-<version>.tar.gz` depuis la [page des versions](https://github.com/sasaken1102r/frame-mic-tuner/releases) et copie-le sur le casque, par exemple depuis ton PC :

```sh
scp frame-mic-tuner-*.tar.gz steamos@<ip-du-casque>:
```

Puis sur le casque (`ssh steamos@<ip-du-casque>`) :

```sh
tar xzf frame-mic-tuner-*.tar.gz
cd frame-mic-tuner
./install.sh
```

Ou compile depuis les sources (aussi sur le casque) :

```sh
git clone https://github.com/sasaken1102r/frame-mic-tuner.git
cd frame-mic-tuner
./install.sh
```

Ensuite **redémarre le casque** une fois, pour que WirePlumber charge le script de bascule. Ne redémarre pas PipeWire ni WirePlumber à la main pendant que tu joues : SteamVR et Steam Link perdent le son. Si ça arrive quand même, redémarre SteamVR sur le casque (ou reconnecte Steam Link) et le son revient.

sudo n'est pas nécessaire. `install.sh` met tout dans ton dossier personnel (`~/.local/bin`, `~/.local/share`, `~/.config`), donc les mises à jour de SteamOS ne l'enlèvent pas. Pour mettre à jour à la main : télécharge la nouvelle version et relance son `install.sh` (ou, dans une copie des sources, `git pull && ./install.sh`). Ou appuie sur **Mettre à jour** dans **Apps et mises à jour** dès qu'une version plus récente est là. La version 0.1.0 n'a pas de bouton de mise à jour, donc le passage de 0.1.0 à 0.2.0 se fait à la main, une seule fois.

Ce qui est installé :

- l'app, une entrée pour la liste **+** du tableau de bord (lancer un programme), et ses icônes
- une unité utilisateur systemd pour la démarrer avec SteamVR (pas activée tant que tu ne l'allumes pas ; voir plus bas)
- un script et une config WirePlumber (`contrib/wireplumber/`) qui remplacent le suivi de micro de SteamOS par un suivi dont l'annulation d'écho et le filtre de bruit se basculent. Pour le reste, il se comporte comme l'original.
- le script de mise à jour partagé (`~/.local/share/frame-mic-tuner/frame-update.sh`, depuis `vendor/frame-updater/`), utilisé par **Apps et mises à jour**

Pour l'enlever : `./install.sh --uninstall`, puis redémarre le casque pour retrouver le comportement micro d'origine de SteamOS. Ajoute `--purge` pour effacer aussi les réglages de l'app et les valeurs de bascule enregistrées.

## Utilisation

1. Ouvre le tableau de bord SteamVR, appuie sur **+** (lancer un programme) et choisis **Frame Mic Tuner**. Une icône **Mic** apparaît dans la rangée du bas ; la sélectionner ouvre le panneau. Le relancer pendant qu'il tourne ouvre seulement le panneau.
2. Le bouton **Sortie** montre la sortie sur laquelle tu écoutes. Dans l'onglet **Rapide**, sous « Quand l'écoute passe par <sortie> », choisis **Écouteurs / casque** ou **Haut-parleurs Frame**. Ce sont des préréglages : ils mettent l'annulation d'écho et le filtre de bruit aux valeurs conseillées (les pastilles de chaque carte les montrent) ; la force du filtre de bruit reste. L'app retient le choix pour cette sortie, donc la prochaine fois elle bascule toute seule. Pour un réglage plus fin, passe à l'onglet **Réglage fin**. Si tes réglages ne correspondent plus à aucun préréglage, **Rapide** dit « Les réglages fins sont utilisés pour l'instant ». Le filtre de bruit coupe les sons faibles (bruits de bouche et autres) quand il est en marche.
   - Dans l'onglet **Réglage fin**, fais glisser **Sévérité** et **Tenue** pour régler le filtre de bruit. Les valeurs de SteamOS sont 23 % et 500 ms ; **Défaut** les ramène. Les curseurs n'agissent que tant que le filtre de bruit est en marche.
   - SteamOS remet ces deux valeurs à chaque redémarrage de son audio (par exemple après un reboot). L'app enregistre tes valeurs pour chaque sortie et les réapplique au démarrage et quand la sortie change, donc elles ne tiennent que pendant que l'app tourne. Active **Démarrer avec SteamVR** si tu les veux tout le temps. Sans l'app, les valeurs de SteamOS sont utilisées.
   - Après un basculement automatique, l'onglet **Rapide** dit « <sortie> est branché, donc son réglage enregistré est actif ». **Annuler** remet ce qui était actif avant et le retient pour cette sortie à la place.
3. **Sortie** (en haut à gauche) : ouvre « Choisir la sortie ». Appuie sur une ligne pour montrer les réglages de cette sortie (tu peux changer ceux d'une sortie qui n'est pas branchée maintenant ; ils servent quand elle revient). **Jouer le son ici** en fait la sortie par défaut, comme l'audio de SteamVR. **Oublier** efface les réglages retenus d'une sortie qui n'est pas branchée. Défile au stick ou en glissant.
4. **Micro** (à côté de Sortie) : **Utiliser ce micro** choisit le micro dont les apps enregistrent. Avec autre chose que le micro intégré du Frame, l'annulation d'écho et le filtre de bruit ne s'appliquent pas, et la carte de réglages propose **Utiliser le micro intégré**.
5. **Couper le son** / **Rétablir le son** (en haut, à côté de l'état) : coupe ou rétablit tout de suite le micro par défaut, sans demander.
6. **Test de voix** : appuie sur **Enregistrer**, dis quelque chose, et appuie sur **Stop** (ça s'arrête tout seul après 10 secondes). Change le réglage, enregistre encore, et utilise ▶ sur chaque ligne pour comparer. L'enregistrement s'arrête dès que tu fermes le panneau.
7. **Démarrer avec SteamVR** (rangée du bas) : allume-le pour que l'app démarre toute seule avec SteamVR à partir de maintenant. Tu peux aussi l'activer avec `./install.sh --autostart`.
8. **Langue** : le panneau démarre dans la langue de ton Steam Frame (japonais, chinois simplifié, allemand, français, espagnol ou italien si Steam est réglé ainsi, anglais sinon). Change-la avec le menu en bas à gauche (日本語, English, 简体中文, Deutsch, Français, Español, Italiano) ; ton choix est enregistré.
9. **Quitter** : appuie deux fois, ou survole l'icône Mic du tableau de bord et choisis Fermer.
10. **Apps et mises à jour** (en haut à droite ; **Réglages du micro** revient en arrière). Les textes et les boutons de cette boîte sont encore en anglais. **Cette app** montre la version en cours, par exemple « Up to date (0.3.4) ». **Check now** cherche tout de suite une version plus récente (ça marche même si **Vérifier les mises à jour** est arrêté).
    - Quand il y en a une, la boîte prend un cadre de la couleur d'accent et dit « Version X is available ». Appuie sur **Update** ; ça demande « Update to X ? » avec **Cancel** et **Update** (la question disparaît toute seule après 3 secondes). Appuie encore sur **Update** pour commencer. La boîte montre alors la progression (« Updating: Downloading », et ainsi de suite).
    - Le panneau reste ouvert et continue avec l'ancienne version pendant l'installation. Quand il dit « X is installed. Quit and start the app again to use it », appuie deux fois sur **Quitter** et relance l'app depuis **+** (ou redémarre SteamVR), puis appuie sur **Close** pour fermer le message s'il est encore là. Si la nouvelle version a changé le script WirePlumber, redémarre aussi le casque (la boîte te le rappelle, et le journal de mise à jour dit s'il a changé).
    - S'il dit « This version can't be installed from here » (la version n'a pas de `SHA256SUMS` ou pas de paquet), mets à jour à la main comme dans Installation ; la boîte montre la page de la version.
    - Si la mise à jour échoue, la boîte prend un cadre rouge, dit pourquoi et que rien n'a changé, et propose **Try again** et **Close**. Si seule la vérification échoue (par exemple sans réseau), seul le texte devient rouge et **Check now** reste.
    - **Vérifier les mises à jour** oui / non : non arrête la vérification quotidienne et le téléchargement de la liste d'apps (voir Vie privée).
    - **Apps associées** montre frame-aux-shortcuts (bascule le mute du micro avec le bouton aux) avec **Installer** s'il manque. « Apps Frame de sasaken@ » ouvre la liste complète. **Installer** et **Ouvrir l'installateur** montrent d'abord la commande exacte ; **Ouvrir dans Konsole** la lance ensuite dans Konsole, où l'installateur montre sa progression. Pas de sudo ; tout va dans le dossier personnel.

Les réglages sont aussi accessibles en SSH :

```sh
wpctl settings --save frame-mic.echo-cancel false        # écouteurs : annulation d'écho arrêtée
wpctl settings --save frame-mic.echo-cancel true         # haut-parleur : annulation d'écho en marche (défaut)
wpctl settings --save frame-mic.noise-suppression true   # filtre de bruit en marche (défaut : arrêté)
~/.local/bin/frame-mic-tuner --print                     # valeurs actuelles, micro en cours, chemin du signal, sorties et réglages enregistrés
```

Un changement fait ainsi (ou par un bouton aux) est remarqué tant que le panneau est ouvert, ou au plus tard quand la sortie change, et il est retenu pour la sortie actuelle.

## Dépannage

- **Le panneau dit « La bascule du micro n'est pas installée »** : le script WirePlumber n'est pas encore chargé. Lance `./install.sh`, puis redémarre le casque.
- **Les boutons de démarrage auto sont gris** : l'unité systemd n'est pas installée. Lance `./install.sh`.
- **Les enregistrements sont silencieux, ou les autres ne t'entendent pas du tout** : vérifie le volume du micro dans le mixeur du casque (lecture seule, c'est sans risque) :
  ```sh
  amixer -c 0 cget name='VA_DEC0 Volume'
  amixer -c 0 cget name='VA_DEC1 Volume'
  ```
  Les deux doivent dire `values=96`. S'ils disent `0`, le micro est en pratique muet. Cette app ne touche jamais le mixeur, mais ces valeurs sont déjà tombées à 0 toutes seules. Pour les remettre (à tes risques ; note d'abord les valeurs actuelles et le `max=` montré par `cget`, et utilise la valeur habituelle de ton casque, 96 chez nous) :
  ```sh
  amixer -c 0 cset name='VA_DEC0 Volume' 96
  amixer -c 0 cset name='VA_DEC1 Volume' 96
  ```
- **Plus aucun son dans SteamVR ou Steam Link après un redémarrage de PipeWire ou WirePlumber** : redémarre SteamVR sur le casque (ou reconnecte Steam Link). Le son revient.
- **Plus aucun son après une mise à jour SteamOS** : le script de bascule est chargé comme partie obligatoire de WirePlumber, donc si une mise à jour le casse, WirePlumber peut s'arrêter et emporter tout le son. Enlève la config en SSH et redémarre le casque (ou lance `./install.sh --uninstall` et redémarre) :
  ```sh
  rm ~/.config/wireplumber/wireplumber.conf.d/90-frame-mic.conf
  ```
- **« En cours » alors que tu ne parles dans aucune app** : toute app qui enregistre de l'audio compte, y compris les enregistreurs d'écran ou de son. SteamOS allume les filtres pour toutes.
- **Frame Mic Tuner manque dans la liste +** : redémarre le casque une fois après l'installation.
- **L'icône Mic manque au tableau de bord alors que l'app tourne** : SteamVR peut perdre le panneau de l'app (par exemple quand le tableau de bord lui-même redémarre). L'app vérifie toutes les 3 secondes et recrée le panneau toute seule ; attends quelques secondes et rouvre le tableau de bord. Relancer l'app depuis **+** vérifie et le recrée aussi avant d'ouvrir le panneau. S'il ne revient toujours pas, le journal (`journalctl --user -u frame-mic-tuner -f` quand elle tourne en service) dit pourquoi (le journal est en japonais ; cherche les lignes qui commencent par « 自己修復 », ce qui veut dire « auto-réparation ») ; quitter l'app et la relancer le répare aussi.
- Journaux : `journalctl --user -u frame-mic-tuner -f` quand elle tourne en service. Pour voir le journal d'un lancement manuel, quitte l'app et lance `~/.local/bin/frame-mic-tuner` en SSH.
- **La vérification de mise à jour échoue, ou « Update » ne fait rien** : `~/.cache/frame-mic-tuner/update.log` a le détail, et `~/.cache/frame-mic-tuner/update-check.json` / `update-state.json` la dernière réponse brute. Couper **Vérifier les mises à jour** (ou `update_check: false` dans le fichier de réglages) arrête la vérification quotidienne automatique sans enlever **Check now** / **Update**.
- **Les réglages n'ont pas basculé quand j'ai branché des écouteurs** : l'app ne suit que la sortie *par défaut*. Si SteamVR continue de jouer par les haut-parleurs du Frame, appuie sur **Jouer le son ici** pour les écouteurs dans **Sortie** (ou choisis-les dans l'audio de SteamVR). Les sorties qui ne sont pas de vrais appareils (filtres, sorties virtuelles) sont ignorées.
- **« Impossible d'ouvrir Konsole » en installant une autre app** : Konsole a besoin du mode développeur (Paramètres Steam → Système → Enable Developer Mode).

## Vie privée

- Les enregistrements du test de voix restent seulement dans la mémoire de l'app. Ils ne sont jamais écrits sur le disque ni dans les journaux, et jamais envoyés nulle part. Ils disparaissent quand l'app quitte.
- Pendant l'enregistrement, le panneau montre « ● Enregistrement ». L'enregistrement s'arrête dès que tu fermes le panneau.
- Le micro peut prendre les voix des gens autour de toi. Penses-y avant d'enregistrer.
- Pour choisir la langue par défaut, il lit une fois au démarrage la ligne `language` du `~/.steam/registry.vdf` de Steam (lecture seule).
- L'app n'a pas de télémétrie. Son usage du réseau, c'est la vérification de mise à jour et la liste d'apps (ci-dessous) :
  - **Quand** : tant que l'app tourne, elle interroge GitHub au démarrage si sa dernière réponse a plus de 24 heures, puis environ une fois par jour (une heure plus tard après un échec). **Vérifier** interroge tout de suite. Couper **Vérifier les mises à jour** dans **Apps et mises à jour** (ou `update_check: false` dans le fichier de réglages) arrête la vérification automatique ; elle ne se connecte plus que si tu appuies sur **Vérifier** ou **Mettre à jour**.
  - **Ce qu'elle envoie** : une requête HTTPS pour la dernière version de ce projet vers `api.github.com`, sans compte, identifiant ni donnée d'usage. Comme toute requête web, GitHub voit ton adresse IP et le User-Agent de `curl`. Tes enregistrements, réglages et journaux ne sont jamais envoyés.
  - **Ce qu'elle reçoit** : le numéro de version de la dernière release, sa page et les noms et liens de ses fichiers. La réponse est gardée dans `~/.cache/frame-mic-tuner/update-check.json`.
  - **Seulement quand tu appuies sur Update et que tu confirmes**, elle télécharge aussi le paquet de cette version et `SHA256SUMS` depuis `github.com` / `*.githubusercontent.com`.
- Avec **Vérifier les mises à jour** en marche, elle récupère aussi la liste des autres apps Steam Frame de l'auteur et leurs icônes depuis `frame.sasaken1102s.net` (au plus une fois par heure, partagé avec les autres apps de l'auteur ; gardé dans `~/.cache/frame-apps/`). C'est un simple GET HTTPS sans rien joint (`curl -q --proto =https --max-redirs 0`) : comme GitHub, `frame.sasaken1102s.net` voit ton adresse IP et le User-Agent de `curl`, et rien d'autre n'est envoyé. Couper **Vérifier les mises à jour** arrête ça aussi, et le panneau montre alors la dernière liste récupérée ou celle intégrée à l'app. Quand tu installes une autre app depuis **Apps et mises à jour**, Konsole s'ouvre et lance la commande d'installation montrée sur la confirmation, qui se connecte à `frame.sasaken1102s.net` et à GitHub dans Konsole.
- Les seuls fichiers qu'elle écrit sont `~/.config/frame-mic-tuner/config.json` (langue de l'interface, dernier onglet, le réglage de vérification, et pour chaque sortie utilisée son nom, les réglages du micro Frame et la date de dernière utilisation), `~/.config/frame-mic-tuner/install-args` (les options de ton dernier `./install.sh`, pour qu'une mise à jour réinstalle de la même façon), `~/.cache/frame-mic-tuner/` (la réponse en cache de la vérification, le journal et l'état de la mise à jour, un dossier verrou `update.lock/` pendant une mise à jour, et un dossier de travail dont le téléchargement est effacé après chaque mise à jour), `~/.cache/frame-apps/` (la liste d'apps et les icônes récupérées, partagées avec les autres apps de l'auteur), et un fichier verrou dans `$XDG_RUNTIME_DIR` qui contient son identifiant de processus (pour empêcher une deuxième copie de démarrer). Une mise à jour réécrit aussi les fichiers installés, comme le fait `./install.sh`.

## Avertissement

- Utilisation à tes risques. Ce projet a été fait avec Claude Opus 5.5, un modèle d'IA. Je l'ai testé sur mon propre Steam Frame, mais je ne peux pas répondre de ce qui arrive sur le tien : lis le code et vérifie-le toi-même avant de le lancer. Le logiciel est fourni sans garantie (voir [LICENSE](LICENSE)).
- Ce qu'il change : les deux réglages WirePlumber `frame-mic.echo-cancel` et `frame-mic.noise-suppression` (via `wpctl settings`), les deux paramètres en direct du filtre de bruit « VAD Threshold (%) » et « VAD Grace Period (ms) » (via `pw-cli set-param` ; SteamOS remet ses valeurs quand son audio redémarre), et la configuration WirePlumber dans ton dossier personnel, où il remplace le suivi de micro de SteamOS par son propre script. Quand tu appuies sur **Jouer le son ici**, **Utiliser ce micro** / **Utiliser le micro intégré** ou **Couper le son** / **Rétablir le son**, il change la sortie par défaut, le micro par défaut ou son mute (via `wpctl set-default` / `wpctl set-mute`, comme l'audio de SteamVR). À part ça, il n'active ou ne désactive sa propre unité utilisateur systemd que si tu le demandes, ouvre Konsole avec l'installateur quand tu confirmes l'installation d'une autre app, et, quand tu mets à jour depuis le panneau, lance le `install.sh` de la nouvelle version dans une unité utilisateur systemd de courte durée (`frame-mic-tuner-update`).
- Il ne redémarre jamais PipeWire ni WirePlumber, n'écrit jamais dans le mixeur ALSA (amixer), ne touche jamais `/etc` et n'a jamais besoin de root.
- **Update** : télécharge le `.tar.gz` et le `SHA256SUMS` de la dernière version depuis GitHub en HTTPS (seulement vers les hôtes de GitHub), vérifie l'empreinte, et seulement ensuite l'extrait dans `~/.cache/frame-mic-tuner/update/` et lance son `install.sh` avec les mêmes options que ta dernière installation. Il refuse une version sans `SHA256SUMS`, une empreinte qui ne correspond pas, ou une archive qui contient un chemin absolu, `..`, un lien ou un fichier spécial ; rien n'est changé si l'un de ces cas arrive. Le téléchargement est effacé ensuite. `SHA256SUMS` est une somme de contrôle publiée dans la même version, pas une signature : elle attrape un téléchargement corrompu ou incomplet, mais pas une version qui aurait été remplacée sur GitHub. L'app en cours n'est pas relancée : quitte-la et relance-la pour utiliser la nouvelle version. Voir les [notes de frame-update.sh](vendor/frame-updater/frame-update.sh) pour les étapes exactes.
- Une mise à jour SteamOS peut changer la façon dont les filtres du micro sont montés. Les bascules peuvent alors cesser de marcher jusqu'à ce que ce projet soit mis à jour. Comme le script de bascule est une partie obligatoire de WirePlumber, un script qui ne se charge plus peut aussi couper tout le son du casque (voir Dépannage). `./install.sh --uninstall` et un redémarrage ramènent le comportement d'origine de SteamOS.
- Projet non officiel, sans lien ni approbation de Valve Corporation. Steam, Steam Frame, SteamOS, SteamVR et Steam Link sont des marques et/ou des marques déposées de Valve Corporation aux États-Unis et/ou dans d'autres pays. Les noms sont utilisés ici seulement pour dire avec quoi ceci fonctionne.

## Développement

Compiler sur le casque :

```sh
cmake -G Ninja -S . -B build && ninja -C build
./build/frame-mic-tuner --help
scripts/package.sh   # build de version : dist/frame-mic-tuner-<version>.tar.gz, dist/SHA256SUMS
```

Options utiles qui n'ont pas besoin de SteamVR : `--print`, `--set-ns-vad N` / `--set-ns-grace N` (appliquer tout de suite une force de filtre de bruit sans l'enregistrer), `--dump-png PATH` (dessiner le panneau, avec des états `--fake-*` pour les captures, comme `--fake-output`, `--fake-switched`, `--fake-external-mic`, `--fake-update STATE`, et `--view` / `--overlay` pour les autres écrans), `--test-record 3` (enregistrer 3 secondes et les lire), `--contrast-report` (contraste WCAG de chaque paire de couleurs) et `--version`. Les notes de conception, le fonctionnement du script de bascule et les résultats de test sont dans [docs/DEVELOPMENT.ja.md](docs/DEVELOPMENT.ja.md) (japonais).

Le mécanisme de mise à jour (`vendor/frame-updater/`), les parties d'interface partagées (`vendor/frame-ui/`) et la liste des autres apps de l'auteur (`vendor/frame-apps/`) sont des copies de dépôts partagés et privés ; ne modifie pas les copies à la main. Leur `verify.sh` (lancé par `scripts/package.sh`) vérifie qu'elles n'ont pas été modifiées.

Les builds Release gardent l'optimisation de vitesse du compilateur et activent, quand c'est possible, l'optimisation à l'édition de liens ainsi que la suppression du code et des données inutilisés. Après la compilation, `ctest --test-dir build --output-on-failure` lance le `--self-test` existant et les tests exhaustifs de conversion des pixels, sans SteamVR ni matériel audio. `./build/rgba-test --benchmark` compare aussi le temps de conversion avec l'ancien algorithme. Les tests ne sont pas inclus dans les paquets de version ; configure avec `-DBUILD_TESTING=OFF` pour ne pas les compiler.

### Publier

`scripts/package.sh` lance le `verify.sh` des trois copies sous `vendor/`, compile un binaire Release, et écrit `dist/frame-mic-tuner-<version>.tar.gz` et `dist/SHA256SUMS` (nécessaire pour que le bouton **Mettre à jour** du panneau accepte la version). Il affiche la commande qui crée la version GitHub (et son tag) avec les deux fichiers joints ; écris les notes de version dans un fichier hors du dépôt d'abord :

```sh
gh release create v<version> dist/frame-mic-tuner-<version>.tar.gz dist/SHA256SUMS --title v<version> --notes-file <fichier-de-notes>
```

N'en fais pas un brouillon ni une préversion : le panneau regarde la dernière version GitHub, qui ignore les deux.

## Licence

MIT. Voir [LICENSE](LICENSE). L'en-tête OpenVR fourni (`third_party/openvr/`) est BSD-3-Clause de Valve Corporation. L'icône des langues (`assets/heroicons-language-16-solid.svg`, aussi intégrée au programme) vient de [Heroicons](https://github.com/tailwindlabs/heroicons) par Tailwind Labs, MIT. `vendor/frame-updater/`, `vendor/frame-ui/` et `vendor/frame-apps/` (y compris les icônes des autres apps intégrées au programme) sont des copies du propre code de l'auteur et sont MIT comme le reste de ce dépôt. Les licences de l'en-tête fourni et des bibliothèques qu'il utilise sont listées dans [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md). Les changements sont listés dans [CHANGELOG.md](CHANGELOG.md).
