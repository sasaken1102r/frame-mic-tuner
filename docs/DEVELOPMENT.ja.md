# 開発メモ（Frame Mic Tuner）

使い方は [README.ja.md](../README.ja.md)。ここには仕組み・画面の決まり・確かめたこと・開発用のオプションをまとめる。

Steam Frame（aarch64 SteamOS）で、マイクの**エコー除去**と**ノイズ除去**を SteamVR のダッシュボードから切り替えるパネル。
ヘッドセットを被ったまま、イヤホンのときはエコー除去を切って小さな音まで入れ、スピーカーのときはエコー除去を入れてスピーカーの音の回り込みを消す。

- 切り替えの仕組み（WirePlumber のスクリプト）は `contrib/wireplumber/`（下の「切り替えの仕組み」）。アプリはその設定 `frame-mic.echo-cancel`・`frame-mic.noise-suppression` を `wpctl settings --save` で書き換える
- ダッシュボードのオーバーレイ、Vulkan での画像の送り方、終了処理、＋への登録、二重起動の扱いは、同じ作者の Steam Frame 向けオーバーレイ（未公開）と同じ作り

## Frame のマイクの音の流れ

```
alsa_input.platform-sound.HiFi__Mic__source（2ch、デジタルマイク 2 個）
 → eq_capture / eq_source（EQ: 音質補正 ＋ 2ch→1ch）
 → echo_cancel_capture / echo_cancel_source（WebRTC のエコー除去＋自動音量調整）
 → ns_capture / ns_source（ノイズ除去: 声らしさで判定して、声でない音を消す）
 → alsa_loopback_stream → alsa_loopback_device...HiFi__Mic__source（既定のマイク。アプリはここから録る）
```

- 各段は WirePlumber の smart filter（`filter.smart = true`、`steamos.mic_filter = true`）。有効・無効は `filters` メタデータの `filter.smart.disabled`
- SteamOS の元の動き（Valve の `/etc/wireplumber/scripts/microphone-tracker.lua`）は、録音のストリーム（`Stream/Input/Audio`）が 1 つでもあれば 3 段とも有効、0 になったら全部無効
- 口の小さな音（ぺちゃくちゃ音など）は、エコー除去で大きく削られ、ノイズ除去で完全な無音にされる。イヤホンで使うならエコー除去は要らないので、切れるようにした

## 切り替えの仕組み（contrib/wireplumber）

| ファイル | Frame での置き場所（install.sh が入れる） |
|---|---|
| `frame-mic-tracker.lua` | `~/.local/share/wireplumber/scripts/frame-mic-tracker.lua` |
| `90-frame-mic.conf` | `~/.config/wireplumber/wireplumber.conf.d/90-frame-mic.conf`（`@SCRIPT_PATH@` を上のスクリプトの絶対パスに置き換える） |

- `90-frame-mic.conf`: Valve の tracker（`steamos.microphone-tracker`）を `disabled` にして、自作の `frame-mic-tracker.lua` を読み込む。設定 `frame-mic.echo-cancel`（既定 true）と `frame-mic.noise-suppression`（既定 false）を定義する
- `frame-mic-tracker.lua`: 動きは元と同じ（録音のストリームがいる間だけフィルターを入れる）。違いは、エコー除去（`echo_cancel*`）とノイズ除去（`ns_*`・`dsp_*`）をそれぞれ設定で切れること。設定を変えると、使用中のマイクにもその場で反映する（PipeWire の再起動は要らない。録音中に切り替えても音は途切れない）
- どちらもホームフォルダの中だけ。Valve の `/etc` のファイルには触らない。入れたあとは、ヘッドセットの再起動で有効になる
- 外すと（`./install.sh --uninstall` のあと再起動）Valve の元の動き（3 段とも有効）に戻る
- 作るときにはまったこと:
  - ノードの `steamos.mic_filter` は情報側のプロパティなので、`Constraint` に `type = "pw"` が要る。既定の `pw-global` だと 1 つも一致しない
  - Valve の tracker はイベントの source から ObjectManager を借りているが、設定の変更（`Settings.subscribe`）のときはイベントが無いので、自前の ObjectManager を持つ

手で切り替えるとき:

```sh
wpctl settings --save frame-mic.echo-cancel false        # エコー除去を切る（イヤホン）
wpctl settings --save frame-mic.echo-cancel true         # エコー除去を入れる（スピーカー）
wpctl settings --save frame-mic.noise-suppression true   # ノイズ除去を入れる
wpctl settings frame-mic.echo-cancel                     # 今の値（Value: true (Saved: true)）
```

`--save` の値は `~/.local/state/wireplumber/sm-settings` に書かれ、再起動後も残る。

## 画面

SteamVR のダッシュボードの下の並びに「Mic」のアイコン（マイクの絵）が出る。選ぶとパネル（1200×788、幅 2.8m）が開き、レーザーポインターで押して操作する。

2026-10-10 に、ユーザーが決めた見本（claude.ai の Artifact「マイク（試作）」。1200×788 の絶対座標の HTML）に合わせて作り直した。部品（切り替え・ボタン・カード・一番下の行）は共通の UI 部品 frame-ui（`vendor/frame-ui/`）で描き、色も frame-ui と同じ（下の「色とアクセシビリティ」）。画面は 2 つ（**マイクの設定** と **アプリと更新**）で、その上に重ねて出す画面が 4 つある（音の出口を選ぶ・使うマイク・ささけんの Frame アプリ・入れる前の確認）。

| 場所 | ボタン | 動き |
|---|---|---|
| 見出し: 状態ラベル（x 140・幅 168 で固定） | （表示だけ） | 「● 使用中」（緑）/「○ 未使用」（灰）/ マイクに斜線の絵と「ミュート中」（赤）/「読み込み中…」/「○ 状態不明」（つながりを読めない）。ミュート中が優先 |
| 見出し: ミュート（x 324・幅 220 で固定） | **ミュートする** / **ミュートを解除** | 確認なし。`wpctl set-mute @DEFAULT_AUDIO_SOURCE@ 1` / `0` を書いて、`wpctl get-volume` で読み返して確かめる。失敗は一番下の行に赤で（「ミュートできませんでした（wpctl）」「ミュートを解除できませんでした（wpctl）」）。ミュートを読めていない間は押せない。**ラベルもボタンも幅を決めてあるので、ミュートしても位置・幅は変わらない**（3 言語とも。`--self-test` で確かめている）。「ミュートする」のときだけ斜線のマイクの絵を付ける |
| 見出し: 右（x 964・幅 208） | **アプリと更新** / **← マイクの設定へ** | 画面を切り替える。マイクの設定の画面では、新しい版がある（入れ終わり・失敗も）か、いっしょに使うアプリ（frame-aux-shortcuts）が入っていないときに、ピンクの点を付ける |
| 音の出口の欄（28, 92, 380×60） | 欄を押す | 「音の出口を選ぶ」を重ねる（一覧をすぐ読み直す）。絵（スピーカー / イヤホン）・小さな見出し「音の出口」・**設定を表示している出口**の名前・一覧の数「N 個」・▾ |
| マイクの欄（420, 92, 184×60） | 欄を押す | 「使うマイク」を重ねる。既定のマイクの短い名前（内蔵は「Frame 内蔵」、外付けは機器の名前から「USB Audio」などを外したもの） |
| タブ（左のカードの上） | **かんたん** / **細かく調整** | 左のカードの中身を切り替える（frame-ui の切り替え）。最後に見ていたタブを `config.json` の `tab` に保存し、次に開いたときもそのタブ。バーのドラッグ中にタブを切り替えた・パネルを閉じたときは、先にドラッグを終わらせて最後の値を送って保存する。外付けのマイクを使っている間は薄くして押せない |
| 「<出口> で聞いているとき」（かんたんのタブ、プリセット） | **イヤホン・ヘッドホン** / **Frame のスピーカー** | 設定をまとめて切り替えるプリセット。イヤホン = エコー除去オフ・ノイズ除去オフ、スピーカー = エコー除去オン・ノイズ除去オフ。表示している出口が今の出口なら、1 つのコマンド（出口の設定をまとめてかける `ApplySettings`）としてワーカーに渡し、`wpctl settings --save` をエコー除去 → ノイズ除去の順に 2 回書いてから 1 回だけ読み直す。途中の値で選択が外れないよう、書き終わるまで（最長 8 秒）押したカードを選択中の見た目で保つ。片方が失敗しても、もう片方は書き、「エコー除去の切り替えに失敗（wpctl）」「ノイズ除去の切り替えに失敗（wpctl）」のどちらか（両方なら「切り替えに失敗（wpctl）」）を赤で出す。どちらの場合も、表示している出口の設定として覚える（今の出口でなければ覚えるだけで、かけない）。ノイズ除去の強さは変えない。各カードに、押すと何になるかの札 2 つ。今の設定と一致するカードが選択中（✓ の丸・ピンクの塗り）。一致はエコー除去とノイズ除去の 2 つだけで見る |
| プリセットの下の 1 行 | **元に戻す**（知らせのときだけ） | ふだんは「<出口> のときは、いつもこの設定になります。つないだら自動で切り替えます」（どちらとも一致しなければ「いまは「細かく調整」の設定を使っています」）。出口が変わって自動で設定をかけたあとは、ピンクの枠の知らせ「<出口> をつないだので、覚えていた設定にしました」と **元に戻す**（切り替える前にかかっていた値をかけ、その出口の設定として覚え直す）。知らせは、設定を手で変えた・別の出口を表示した・また出口が変わったときに消える |
| エコー除去（細かく調整のタブ） | オン / オフ | `frame-mic.echo-cancel`（今の出口なら書いて、表示している出口の設定として覚える） |
| ノイズ除去（細かく調整のタブ） | オン / オフ | `frame-mic.noise-suppression`。オンにすると小さい音（口の音など）は消える |
| 判定の厳しさ / 余韻（細かく調整のタブ） | バー（ドラッグ）、標準に戻す | ノイズ除去の強さ（下の「ノイズ除去の強さ」）。見本に合わせて − / ＋ はやめ、バーだけにした（押したところへ飛び、そのままドラッグ）。ノイズ除去がオフの間は塗りとつまみと値を灰色にし、1 本目の説明を「オフの間は効きません」に（押せるまま。見出しは文字のコントラストを保つため薄くしない）。「標準に戻す」はバーの下の行に、「標準は 23%・500ms（SteamOS の値）」と一緒に置く |
| 外付けのマイクのとき（タブの中身の代わり） | **Frame 内蔵マイクに戻す** | 「いまは <マイク> を使っています」、処理がかかるのは内蔵マイクだけという説明、「<出口> のときの設定（<まとめ>）は、内蔵マイクに戻すと使います」。ボタンは内蔵マイクを既定のマイクにする（下の「既定の切り替え」） |
| つながり（左のカードの下の暗い箱） | （表示だけ） | 「マイク ─ 音質補正 ─ エコー除去 ─ ノイズ除去 ─ アプリへ」。実際に通っている段はピンクの薄い塗り＋実線の枠＋太字、通っていない段は点線の枠＋細字。段と段の線は、両側が通っていればピンクの実線、ほかは点線（見本のとおり。前の「下を回って飛ばす線」はやめた）。外付けのマイクのときは最初の段をそのマイクの短い名前にして、処理の段はすべて通っていない形。未使用・読めないときは「つながり」の右に説明 |
| 声のチェック | ● 録音 / ■ 停止、履歴の ▶ / ■ | アプリに届く音を最大 10 秒録って、5 件まで聞き比べる（下の「声のチェック」）。履歴の 2 行目は録ったときの出口と設定（「AB13X・エコー除去オフ」「スピーカー・エコー除去オン・ノイズ除去オン 10%/800ms」）か、外付けのマイクなら「AB13X のマイク（処理なし）」 |
| 下の 1 行（frame-ui の `drawFooter`） | 日本語 / English / 简体中文、SteamVR と一緒に起動 オン / オフ、終了 | 言語はすぐ切り替えて保存。自動起動は `systemctl --user enable` / `disable`（下の「自動起動」。ユニットが無いときは押せず「準備されていません」）。終了は 3 秒以内の 2 回目で終了（終了コード 3）。いちばん下の注意書きは、失敗（赤）があればそれ、無ければ自動起動が使えない理由、どちらも無ければ「押すとすぐ切り替わり、音の出口ごとに覚えます。録音はメモリの中だけで、終了すると消えます」 |

重ねて出す画面（後ろを暗くし、後ろのボタンは押せない。右上の ✕ で閉じる）:

| 画面 | ボタン | 動き |
|---|---|---|
| 音の出口を選ぶ（56, 56, 1088×676） | 行・**ここから音を出す**・**忘れる** | 「つながっている出口」（Frame のスピーカーが先頭）と「前に使った出口（いまはつながっていない）」（最後に使った日の新しい順）。各行に絵・名前・状態（いま使用中（緑の ●）/ つながっています / 最後に使ったのは M/D）・覚えている設定のまとめ。行を押すとその出口の設定を表示して閉じる（「設定を表示中」の札。今の出口でない出口は、変えても覚えるだけ）。右は、つながっている出口なら **ここから音を出す**（既定にする。一覧は開いたまま）、今の出口なら「いま使用中」の札、前に使った出口なら **忘れる**（覚えた設定を消す。今の出口・つながっている出口は消せない）。一覧は 76px の行を 8px おきに並べ、入りきらなければスクロール（下の「一覧のスクロール」）。下に説明 2 行 |
| 使うマイク（160, 140, 880×440） | **このマイクを使う** | つながっているマイク（内蔵が先頭）。内蔵は「エコー除去・ノイズ除去がかかります」、外付けは「処理はかかりません（そのままアプリへ）」。使うマイクを既定にして閉じる |
| ささけんの Frame アプリ（アプリと更新から） | **入れる**・**インストーラーを開く** | frame-apps の一覧（下の「ほかのアプリ」） |
| 入れる前の確認 | **やめる**・**Konsole で開く** | 実行するコマンドを等幅の箱で見せる。一覧から開いたときは、やめると一覧に戻る |

アプリと更新の画面:

| 場所 | ボタン | 動き |
|---|---|---|
| このアプリ（左のカード） | 今すぐ確かめる / 更新する（確認: やめる・更新する）/ もう一度・閉じる / 閉じる | アイコン（サムネイルと同じ絵）・名前・版・説明。その下の暗い箱に `vendor/frame-updater` の `UpdateChecker` の状態（前の全幅の更新の帯をここへ移した）。文言は frame-ui の表（strings.md のとおり）。ただし確認・更新中の補足と入れ終わりの文と補足は、install.sh が常駐を再起動しないこのアプリに合わせたこのアプリの表のもの（「終わったら、終了して起動し直すと新しい版になります」「WirePlumber のスクリプトも変わったときは、ヘッドセットも再起動してね」）。新しい版あり・確認・入れ終わりはピンクの枠、更新の失敗は赤い枠、確認の失敗は文字だけ赤。箱の幅に合わせて、新しい版があるときのボタンは「更新する」だけ（frame-ui の帯は「今すぐ確かめる」も並べる）。「更新する」の 1 回目は確認の表示にするだけで、3 秒以内に確認の「更新する」で `install()`。最新・版だけのときは 2 行目に「起動したときと 1 日 1 回、GitHub で新しい版を確かめます」（オフなら「自動では確かめません…」） |
| 新しい版の確認 | オン / オフ | `config.json` の `update_check`。オフにすると、自動の確認もアプリの一覧の取得も止まる（「今すぐ確かめる」は押せる） |
| いっしょに使うアプリ（右のカード） | **入れる**・「ささけんの Frame アプリ … ›」 | 下の「ほかのアプリ」 |

画面の並び（見本の座標のとおり。px はパネルの画像の px）:

- 見出し（y 24〜76）: 「マイク」32px 太字（x 28）・状態ラベル（140, 28, 168×44）・ミュートのボタン（324, 22, 220×56）・右に「アプリと更新」（964, 22, 208×56）
- マイクの設定の画面:
  - 上の段（y 92、高さ 60）: 音の出口の欄（x 28・幅 380）とマイクの欄（x 420・幅 184）。地はカードの色に 2px の枠、角丸 18
  - 左のカード（28, 168, 576×492）: タブ（50, 188, 532×56）→ かんたん（見出し 18px（y 262）→ プリセット 2 枚（y 296・402、532×96、角丸 16。左に 44px の絵、名前 22px 太字、効果 15px、右に札 2 つ 13px）→ y 502〜558 に説明 14px（2 行）か知らせ（ピンクの 2px の枠、56px、右に「元に戻す」44px））/ 細かく調整（エコー除去・ノイズ除去の行 y 262・330（見出し 20px・切り替え 196, 210×56・説明 14px 2 行）→ バー y 404・458（見出し 17px・説明 12px・溝 6px・つまみ 24px・値 20px）→ y 514 に標準の説明と「標準に戻す」（424, 158×44））/ 外付けのマイク（y 268〜508）→ つながり（50, 568, 532×74。暗い箱 #14171c、段は高さ 36・15px）
  - 右のカード（620, 92, 552×568）: 声のチェック 22px・説明 14px・録音（1000, 110, 150×56。赤い枠）→ y 180 の行（説明 / 録音中のメーター / 失敗）→ y 216 の区切り → 履歴 5 件（y 226 から 84 おき、高さ 72。▶ は直径 52、時刻 18px・長さ 13px・2 行目 13px（幅 176）、波形は幅 3px の棒を 5px おき）
- アプリと更新の画面: 左のカード（28, 92, 560×568。このアプリ）と右のカード（604, 92, 568×568。いっしょに使うアプリ）
- 一番下の行（y 676〜732）と注意書き（y 744 ごろ）: frame-ui の `drawFooter`（rect 28, 674, 1144×88）
- 大きさ: 1200×788 px を幅 2.8m で出す（高さ約 1.84m。1px あたり約 429 px/m）

- 2026-09-27 の前の画面（左の列にタブ・プリセット・つながり、右に声のチェック、下に全幅の更新の帯）から、2026-10-10 に見本の画面へ作り直した。グレーのプリセット・「細かく調整を見る →」・ミュートの帯・全幅の更新の帯はやめた
- ボタンの選択状態・使用中か・つながりは、実際の値から作る（今の出口のとき。ほかの出口を表示しているときは、覚えている値）
  - エコー除去・ノイズ除去: `wpctl settings`（全設定の一覧）の `frame-mic.*` の `Value:`
  - つながり: `pw-link -l` の実際のリンク。マイク（`alsa_input.platform-sound.HiFi__Mic__source`）から出発して、`<名前>_capture` に入り `<名前>_source` から出るフィルターをたどり、`alsa_loopback_stream.alsa_input...`（既定のマイクの手前）に着くまでを並べる。`eq` = 音質補正（英語では EQ）、`echo_cancel` = エコー除去、`ns` / `dsp` = ノイズ除去
  - マイク使用中 = 通り道にフィルターが入っている。tracker は録音のストリーム（`Stream/Input/Audio`）が 1 つでもあると EQ を必ず入れるので、「使用中」は「tracker がフィルターを入れている」と同じ。**スピーカーの音を録るアプリ（画面録画の ffmpeg など）でも録音のストリームなので数えられる**
  - 未使用のときの `pw-link -l` は、マイクから `alsa_loopback_stream...` へ直接つながる形（「マイク → アプリへ」と「使っていないので処理はお休み中」）
- パネルがダッシュボードで**開いている間は** 1 秒ごとに読み直す（外から `wpctl settings --save ...` で変えられても、1 秒以内に表示が合い、今の出口の設定として覚える）。自動起動の状態（`systemctl --user is-enabled`）は開いた直後と 5 秒おき、出口とマイクの一覧（`pw-dump`）は開いた直後と 5 秒おきと、選ぶ画面を開いたとき。閉じている間は既定の出力・入力だけを 2 秒おきに見る（下の「音の出口ごとの設定」）
- ボタンを押したら、書く → すぐ読み直す → 表示に反映。書いた値は読み返して確かめ、違えば失敗として赤く出す
- パネルを閉じたら、マイクの設定の画面・今の出口の表示に戻し、重ねた画面も閉じる（次に開いたときは今の出口から）
- スクリーンショット（docs/ の v10-*.png）はまだ前の画面のまま（作り直していない）。新しい画面は `--dump-png` の `--fake-*`・`--view`・`--overlay` で撮れる（下の「手動で動かす」）

## 音の出口ごとの設定

イヤホンをつないだら、Frame のマイクのエコー除去が自動で切れるようにするため（ユーザーの目的）。Frame のマイクの設定（エコー除去・ノイズ除去・ノイズ除去の強さ）を、既定の出力（default sink）ごとに覚えてかける。仕組みは `src/outputs.*`（一覧の読み取り・キー）、`src/output_sync.*`（いつ何をかけるか。OpenVR も外部コマンドも使わないので `--self-test` で試せる）、`src/mic_worker.*`（見張りと書き込み）。

### 出口の見分け（キー）

Frame（2026-10-10、AB13X USB Audio の USB-C 変換をつないだ状態）の `pw-dump` / `wpctl status` で確かめた形:

| ノード | id | node.name | 説明 |
|---|---|---|---|
| Valve のラッパー（既定の出力。最初からこれ） | 126 | `alsa_loopback_device.stereo.alsa_output.platform-sound.HiFi__Speaker__sink` | Built-in Audio Playback。`node.link-group` が同じ `alsa_loopback_stream...` の `target.object` が中身 |
| Frame のスピーカー（機器） | 73 | `alsa_output.platform-sound.HiFi__Speaker__sink` | 機器（Device 66）の説明は「Built-in Audio」 |
| AB13X（USB） | 65 | `alsa_output.usb-Generic_AB13X_USB_Audio_202405280846-00.analog-stereo` | 機器（Device 115）の説明は「AB13X USB Audio」 |
| Valve のラッパー（既定の入力） | 69 | `alsa_loopback_device.alsa_input.platform-sound.HiFi__Mic__source` | 中身は `alsa_input.platform-sound.HiFi__Mic__source`（74） |
| AB13X のマイク | 114 | `alsa_input.usb-Generic_AB13X_USB_Audio_202405280846-00.analog-stereo` | |

- キー = 機器のノード名。ラッパー（`alsa_loopback_device.*`）は中身の名前にする（`pw-dump` では link-group のストリームの `target.object`、`pw-metadata` の名前だけのときは名前の中の `alsa_output.` 以降）。Bluetooth（`bluez_output.<アドレス>.<番号>`）は末尾の番号を外して、プロファイルが変わっても同じキーにする。USB などはそのまま
- 一覧に入れるのは Audio/Sink・Audio/Source のうち `alsa_*` / `bluez_*` の機器のノードとラッパーだけ。フィルター（`echo_cancel_sink`・`filter-chain-sink`・smart filter）、ループバックのストリーム、`speaker-ref-sink` などは入れない
- 表示名は機器（Device）の `device.description`（無ければノードの説明）。Frame のスピーカーは画面では言語の表の「Frame のスピーカー」、内蔵マイクは「Frame 内蔵マイク」
- 既定にするノード（`wpctl set-default` に渡す）は、ラッパーがあればラッパー（初めの既定と同じものに戻せるように）、無ければ機器のノード
- 既定の出力が機器の出口でない（フィルター・仮想の出口）ときは、出口の設定を何もかけず、今の出口も変えない

### 覚える場所（config.json の outputs）

```json
"outputs": {
  "alsa_output.platform-sound.HiFi__Speaker__sink": {"name": "Built-in Audio", "kind": "speaker", "echo": true, "ns": false, "ns_vad_threshold_percent": 23, "ns_vad_grace_ms": 500, "last_used": "2026-10-10"},
  "alsa_output.usb-Generic_AB13X_USB_Audio_202405280846-00.analog-stereo": {"name": "AB13X USB Audio", "kind": "earphones", "echo": false, "ns": false, "ns_vad_threshold_percent": 23, "ns_vad_grace_ms": 500, "last_used": "2026-10-10"}
}
```

- 初めて見た出口は、Frame のスピーカー（キーに `HiFi__Speaker__sink`）ならスピーカーのプリセット（エコー除去オン・ノイズ除去オフ）、ほかはイヤホンのプリセット（両方オフ）。強さは SteamOS の値（23% / 500ms）
- この版を初めて起動したとき（`outputs` が無い = 前の版の設定ファイル）は、何もかけずに、今かかっている値（`wpctl settings`）を今の出口の設定として移す。強さは前の版の `ns_vad_*` があればそれ（前の版が起動のたびにかけ直していた値）、無ければ今の値
- そのあとは、今の出口の強さを前の版のキー `ns_vad_threshold_percent` / `ns_vad_grace_ms` にも写す（前の版に戻したときも、最後の出口の強さで動くように）
- 最後に使った日（`last_used`、ローカル時刻の日付）は、その出口が今の出口のあいだ、日付が変わったら書く（音の出口を選ぶ画面の「最後に使ったのは 10/8」）

### いつ何をかけるか

- 見張り: ワーカーが、パネルを閉じている間も 2 秒おきに `pw-metadata -n default` の `default.audio.sink` / `default.audio.source`（今の既定。`default.configured.*` ではない）を読む（約 10ms）。前と違えば、一覧（`pw-dump`、約 90ms）と今の設定（`wpctl settings`・`pw-link -l`・`wpctl get-volume`・`pw-dump ns_capture`）を読み直す。パネルが開いている間は、ほかに 1 秒おきの読み直しと 5 秒おきの一覧がある
- 起動したとき: ワーカーが始めた直後に 1 回、設定と一覧をまとめて読む（WirePlumber がまだで設定を読めなければ、2 秒おきの見張りのついでに最大 10 回読み直す）。今の出口の覚えた設定を、今と同じものは書かない形（`onlyIfChanged`）でかける。エコー除去かノイズ除去が変わったら知らせを出す
- 出口が変わったとき: まず、前の出口の覚えた設定と、切り替わる直前の今の値を比べ、違えば前の出口の設定として覚える（閉じている間に aux ボタンなどで変えられた分）。次に新しい出口の設定を同じくかけ、エコー除去かノイズ除去が変わったら知らせ（元に戻す = 切り替える前の値）を出す。設定の表示も新しい出口に戻す
- かけ方: ワーカーの `ApplySettings`（エコー除去 → ノイズ除去 → 強さの順に書き、1 回だけ読み直す）。書き込みは今までと同じ `wpctl settings --save frame-mic.echo-cancel|noise-suppression` と `pw-cli set-param <ns_capture の id> Props`。強さは「かけたい値」にもなり、ノードがまだ無ければ見つかりしだいかける
- 外からの変化: 同じ出口のまま、読み直した値が覚えた値と違えば、今の出口の設定として覚える（ほかのツール・aux ボタン・`wpctl settings`）。ただし、自分が頼んだ書き込みが終わる前の状態（`MicState::writesDone` が受付番号より小さい）・書き込みに失敗した状態は取り込まない（自動でかけた直後の古い値を読み違えない）。強さは、かけたい値を今のノードにかけ終えた（`nsApplied`）あとの値だけ取り込む（PipeWire が作り直した直後の 23 / 500 を覚えない）
- パネルの操作: 表示している出口が今の出口なら、書いて覚える。今の出口でなければ（音の出口を選ぶ画面で選んだ出口）、覚えるだけ
- 忘れる: つながっていない出口の設定を `outputs` から消す
- `--print` と `--dump-png` は設定ファイルを読むだけで、移し替えもかけることもしない

## 既定の切り替えとミュート

- **ここから音を出す** / **このマイクを使う** / **Frame 内蔵マイクに戻す**: `wpctl set-default <id>`（Frame 内蔵はラッパーのノード）。なったかは `pw-metadata -n default` で最大 1.5 秒読み返して確かめ、ならなければ一番下の行に赤で「音の出口を切り替えられませんでした（wpctl）」「マイクを切り替えられませんでした（wpctl）」。音の出口が変われば、上の「出口が変わったとき」の流れで設定をかけ、知らせを出す
- **ミュートする**: `wpctl set-mute @DEFAULT_AUDIO_SOURCE@ 1`（確認なし）。`wpctl get-volume` で読み返す。**ミュートを解除** は同じく `0`
- この 3 つは、実機の既定の出口・マイク・ミュートを変えるので、開発中はヘッドセットで試していない（ユーザーが確かめる）

## 一覧のスクロール

- 音の出口を選ぶ・使うマイク・ささけんの Frame アプリの一覧は、入りきらないとスクロールする。見えている枠で切り抜き、右に幅 6 の細いスクロールバー（溝 #262a33・つまみは枠の色）、下に続きがあるときは下の端 28px をカードの色へぼかす
- スティック: ダッシュボードのオーバーレイに `VROverlayFlags_SendVRSmoothScrollEvents` を付け、`VREvent_ScrollSmooth`（と `ScrollDiscrete`）の `ydelta` 1 あたり 120px 動かす（上へ倒すと上へ）
- ドラッグ: 一覧の中を押したまま 8px 以上動かすとスクロール（指についていく）。一覧の中の行とボタンは、押したときではなく**離したときに**、ドラッグしていなければ押したことにする（行を押してそのまま送れるように）

## ほかのアプリ（frame-apps）

ささけんのほかの Steam Frame アプリの一覧と、インストーラーを Konsole で開く共通の部品（`vendor/frame-apps/`。frame-apps の README の「アプリへの入れ方」のとおり）。

- `AppsManager`: 自分 = `frame-mic-tuner`（一覧に出さない）、いっしょに使うアプリ = `frame-aux-shortcuts`（理由「aux ボタンで、マイクのミュートを切り替える」。中国語の理由はこのアプリの表から）
- 一覧を取りに行くのは「新しい版の確認」がオンのときだけ（`setFetchEnabled(config.update_check)` を毎回）。起動時と一覧を開いたときに `fetchIfDue()`（1 時間に 1 回まで、ほかのアプリと共有）。メインループで毎回 `tick()`。アプリと更新の画面を見ている間は `refreshIfStale(5)`
- 画面: アプリと更新の右のカードに、いっしょに使うアプリ（アイコン・名前・理由・**入れる** か状態の札）と「ささけんの Frame アプリ N 個のうち M 個が入っていません ›」（押すと一覧）と、Konsole・sudo なしの説明。一覧（アイコン・名前・このアプリで使うものには札・説明（簡体字中国語は英語の説明）・**入れる** / 動作中 / 入っています / Konsole で実行中…）と **インストーラーを開く**
- **入れる** と **インストーラーを開く** は、まず確認の画面で `installerCommand(key)` をそのまま見せ、**Konsole で開く** で `openInstaller(key, lang)`（簡体字中国語のときの Konsole の最後の 1 行は英語）。開けないときは理由（DISPLAY が無い・Konsole が無い・まだ開いている）を赤で出す。アプリ名とコマンドは、今の一覧から呼び名で引く
- 文言は frame-apps の strings.md のとおり（日英）。簡体字中国語はこのアプリの表で訳した

## ノイズ除去の強さ

ノイズ除去の段は、Valve の filter-chain（`/etc/pipewire/microphone-filter-chain/microphone-filter-chain-echo-cancel-cpu.conf`）の LADSPA `noise_suppressor_mono`。その control を、PipeWire を再起動せずにその場で変える。

| バー | param 名（`ns_capture` の Props の params） | 範囲（PropInfo） | SteamOS の値 | 丸め |
|---|---|---|---|---|
| 判定の厳しさ | `noise_suppressor_mono:VAD Threshold (%)` | 0〜99（プラグインの既定 49.5） | 23 | 1% |
| 余韻 | `noise_suppressor_mono:VAD Grace Period (ms)` | 0〜1000（既定 500） | 500 | 10ms |

- 送り先は `ns_capture`（filter-chain の入口のノード）。`ns_source` の Props には control は無い（2026-09-27 に `pw-cli enum-params <id> PropInfo` / `Props` で確認）
- 読む: `pw-dump ns_capture`（1 回で id と値が取れる。約 40ms）の `info.params.Props[].params`（`[名前, 値, 名前, 値, ...]`）
- 書く: `pw-cli set-param <id> Props '{ params = [ "noise_suppressor_mono:VAD Threshold (%)" 23.0 "noise_suppressor_mono:VAD Grace Period (ms)" 500.0 ] }'`。fork＋execvp・シェルなしで、数値はアプリで範囲に丸めて（1% / 10ms 単位）から文字列にする。2 つはいつもまとめて送る
- 同じ PropInfo に `Retroactive VAD Grace (ms)`（0〜200、SteamOS は 0）もあるが、画面には出していない
- SteamOS は PipeWire の起動のたびに filter-chain の設定の値（23 / 500）に戻す。そこで、アプリの `config.json` に音の出口ごとに保存して（`outputs` の各出口の `ns_vad_threshold_percent`・`ns_vad_grace_ms`。上の「音の出口ごとの設定」）、出口が変わったときと、次のときにかけ直す:
  - アプリが起動したとき（ワーカーがノードを探し、見つかるまで 5 秒おきに試す。かけたら止まる。パネルを閉じていても、この起動直後の分だけは動く）
  - パネルが開いている間の 1 秒おきの読み直しで、ノードの id が変わっていたとき（PipeWire がノードを作り直した）
  - パネルを閉じている間に PipeWire が作り直されたときは、次にパネルを開いたときにかけ直す
- 外から `pw-cli` で変えられたときは、かけ直さずに表示を追いつかせる（id が同じなら今の値を正とする）
- バーは押したところへ飛び、そのままドラッグできる（溝の上下に外れても横の位置で決める。当たり判定は行の高さ 48 いっぱい）。ドラッグ中は、値が変わっていれば 100ms おきに最後の値だけ送る（ワーカーの待ち行列でも、まだ実行していない前の値は捨てる）。離したとき（パネルから外れたときも）に必ず 1 回送って、表示している出口の設定として保存する。今の出口でない出口を表示しているときは送らず、保存だけ。書いたあとは読み直しが追いつくまで 1.5 秒、書いた値を表示に使う（ちらつかないように）
- WirePlumber のスクリプト（`contrib/wireplumber/`）は変えていない
- 声のチェックの履歴には、ノイズ除去がオンで録ったものだけ、そのときの強さ（例: `10%/800ms`）を出す。2 行目に収まらなければ 1 行目の長さの後ろ、そこにも収まらなければ省く

確かめたこと（2026-09-27、Frame。値は最後に 23 / 500 に戻した）:

- `--set-ns-vad 10` → `pw-cli enum-params ns_capture Props` が 10.0 / 500.0。`--set-ns-grace 800` → 10 / 800。範囲外（150 / -5）は 99 / 0 に丸まる。`--print` に「判定の厳しさ 10%・余韻 800ms（ns_capture の id 53）」と出る
- 保存した値（30% / 600ms）の設定ファイルで起動すると、ログに「ノード 53 に保存した値をかけます」と出て、`pw-cli` の値が 30 / 600 になった
- 効き方（参考。話していない静かな部屋、ノイズ除去オン・マイク使用中、0.5 秒ごとのレベル）: 判定の厳しさ 0% の間は背景の音が rms -57〜-85 dBFS で通り、99% と 23% の間はほぼ無音（-90 dBFS）

## 声のチェック

イヤホンとスピーカーで、アプリに届く声がどう違うかをその場で聞き比べる。

- 「● 録音」で開始。10 秒で自動で止まり、途中でもう一度押しても止まる（ボタンは「■ 停止」に変わる）
- 録音中は「● 録音中」の文字（赤の色だけにしない）、経過と残りの秒数、音量のピークのメーター（-60〜0 dBFS と数字）
- 履歴は新しい順に 5 件。6 件目を録るといちばん古いものを消す。各行に録った時刻（例: 00:41）、長さ、録ったときの出口と設定（「AB13X・エコー除去オフ」「スピーカー・エコー除去オン・ノイズ除去オン 10%/800ms」。外付けのマイクなら「AB13X のマイク（処理なし）」。出口・マイクの名前は機器の名前から「USB Audio」などを外したもの）、小さい波形、▶ 再生 / ■ 停止。再生中の行は、再生済みの部分と位置の線がアクセント色で動く。再生中にほかの行の ▶ を押すとそちらに切り替わる
- **録るのはアプリに届く最終の音**: libpipewire-0.3 の `pw_stream` を、target を付けずに既定の入力（`alsa_loopback_device.alsa_input...HiFi__Mic__source`）へつなぐ（`--target` で名前を渡すと smart filter の後ろに付け替えられる罠があるため）。形式は mono・int16・48kHz
- 録音のストリームには `node.virtual` を**付けない**。tracker にマイク使用中と数えてもらってフィルターを入れ、本番と同じ音で録るため（録音中は「使用中」の表示になる）。つないだ直後の 0.3 秒は、フィルターが入って切り替わるまでの音なので捨てる
- 再生は既定の出力（Frame のスピーカーかイヤホン）へ `pw_stream` で流す。出し切ったら `pw_stream_flush(drain)` → `drained` で片付ける。録音を始めると再生は止める（スピーカーの音を録らないように）
- PipeWire のループは専用のスレッド（`pw_thread_loop`）。録音か再生を始めたときに作り、**パネルを閉じたら録音・再生を止めて、ストリームとスレッドごと片付ける**（見えないところで録らない。閉じている間は PipeWire に何も作らない）
- **音声はメモリの中だけ**。ディスクにもログにも書かない（ログに出すのは長さとピークの dBFS だけ）。終了すると消える
- 外部コマンドの pw-record / pw-play は使わない。PipeWire・WirePlumber の再起動や設定の変更もしない

確かめたこと（2026-09-27、Frame）:

- `--test-record 3`: 録音 → 長さとピーク → メモリから再生、が動く（例: 2.57 秒・123424 サンプル・ピーク -24.5 dBFS、再生 2.7 秒で drained）。ほかに録音しているアプリがいないとき、録音中は `pw-metadata -n filters` の EQ とエコー除去が `false`（有効）になり、終わると `true` に戻った。`pw-link -l` で `alsa_loopback_device...:capture_FL/FR -> frame-mic-tuner-record` につながっていた
- 常駐で `--debug-record-on-open` を付けて開き、`--probe-switch-away` で別のダッシュボードのオーバーレイに切り替えると、ログに「パネルが閉じたので、録音・再生を止めます」→「PipeWire のストリームとスレッドを片付けました」と出て、録音のノードが消え、スレッドの数も元に戻った
- 10 秒に達すると自動で止まる（10.0 秒で止まった）
- マイクの音量（ALSA の `VA_DEC0 Volume` / `VA_DEC1 Volume`）が 0 に落ちていると、録音は最初の 0.5 秒のポップのあと -90 dBFS の無音になる（`pw-record` でも同じ。このアプリの問題ではない）。README の「うまく動かないとき」を参照

## 色とアクセシビリティ

- 色は `src/theme.h` の 1 か所にまとめてあり、共通の UI 部品 frame-ui（`vendor/frame-ui/cpp/frame_ui.h`）の定数から作る。描画はここの色だけを使い、`--contrast-report` が同じ定義から WCAG 2.x のコントラスト比を計算して、このアプリの組み合わせと frame-ui の組み合わせ（`frame_ui::contrastPairs()`）の比と合否を出す
- 背景の段: `#0e1015`（地）/ `#1a1d24`（カード・重ねた画面）/ `#14171c`（カードの中の暗い箱: つながり・一覧の行・更新の箱）/ `#20242c`（ボタン・プリセット）/ `#2a2f38`（乗っている）/ `#343944`（押している）。カードは見本どおり影なし、角丸 18
- イメージカラーは `#e07ef5`。選択中の塗り・通っている段・再生位置・メーターに使う。乗っている間 `#e99af8`、押している間 `#c865de`。薄い塗り（通っている段・札）は `#3b2445`
- 決まり: 文字は大きさによらずすべて 4.5:1 以上で確かめる。ボタンの枠・選択状態・図（WCAG 1.4.11）は 3:1 以上
- ボタン・切り替え・欄の枠は `#6e7681`（カードの上で 3.67:1、地の上で 4.14:1）。frame-ui の見本の `#4a505c` は 2.08:1 で足りなかったので、2026-10-10 にユーザーの判断で frame-ui ごと変えた
- アクセントの塗りの上の文字は `#1c0f22`（7.43:1）、補足は `#3a1f44`（5.81:1）
- 選択状態を色だけで伝えない: 選択中は ✓・塗り・太字、通っていない段は点線の枠・細字、録音中は ● と「録音中」の文字、使用中は ● / 未使用は ○ / ミュート中はマイクに斜線の絵と文字、いま使用中の出口は ● と文字
- 前に使った出口の行は、絵と名前だけ不透明度 0.7 にする（補足の灰色まで薄くすると 4.5:1 を切るため）。ノイズ除去がオフの間のバーは、塗り・つまみ・値を補足の灰色にする（見出しは薄くしない）
- スクロールバーのつまみは枠の色 `#6e7681`（溝 `#262a33` の上で 3.13:1）
- 2026-10-10 の結果: 97 組（このアプリ 67・frame-ui 30）すべて合格。このアプリの分でいちばん低いのは、スクロールバーのつまみの 3.13:1（部品は 3:1 以上）

## ビルド（Frame 上）

必要なもの（SteamOS に入っている）: cmake、ninja、g++、pkg-config、cairo、freetype2、libpipewire-0.3、Vulkan のヘッダとローダー（`vulkan` の pkg-config）、SteamVR（`/opt/steamvr/bin/linuxarm64/libopenvr_api.so`）。
`openvr.h` は `third_party/openvr/` に同梱（OpenVR SDK 2.15.6、BSD-3-Clause。[ライセンス](../third_party/openvr/LICENSE)）。

ヘッドセット上で:

```sh
cmake -G Ninja -S . -B build && ninja -C build
```

PC で編集してヘッドセットでビルドする例（PC 側の Git Bash で、このフォルダから）:

```sh
tar --exclude=build --exclude=out --exclude=.git -cf - . | ssh steamos@<headset-ip> 'mkdir -p ~/frame-mic-tuner && tar -xf - -C ~/frame-mic-tuner'
ssh steamos@<headset-ip> 'cd ~/frame-mic-tuner && cmake -G Ninja -S . -B build && ninja -C build'
```

実行ファイルは `build/frame-mic-tuner`。OpenVR ライブラリの場所は rpath に入っているので、別の場所にコピーしてもそのまま動く。`-Wall -Wextra` で警告ゼロ。版は `CMakeLists.txt` の `project(... VERSION ...)` で、`--version` で出る（`CHANGELOG.md` と合わせる）。

## 手動で動かす

```sh
./build/frame-mic-tuner          # ダッシュボードにパネルを出して常駐（Ctrl+C で終了）
```

- SteamVR が起動していなければ 3 秒おきに再試行して待つ（SteamVR を勝手に起動はしない）
- SteamVR が終了する（`VREvent_Quit`）と `AcknowledgeQuit_Exiting` で応えて静かに終了する（終了コード 0）
- Ctrl+C / SIGTERM は印をつけるだけで、メインループから同じ終了処理を通る（下の「画像の送り方と終了処理」）

確認用のオプション（`--probe` と `--probe-switch-away` 以外は OpenVR なしで動く）:

```sh
./build/frame-mic-tuner --print                  # 今の値・マイク使用中か・つながり・自動起動・音の出口とマイクの一覧・出口ごとの設定を表示
./build/frame-mic-tuner --set-echo off           # エコー除去を切ってから表示（イヤホン）。on で入れる（スピーカー）
./build/frame-mic-tuner --set-ns on              # ノイズ除去を入れてから表示
./build/frame-mic-tuner --set-autostart on       # systemctl --user enable してから表示（off で disable）
./build/frame-mic-tuner --set-ns-vad 10 --set-ns-grace 800   # ノイズ除去の強さをその場でかけてから表示（保存はしない）
./build/frame-mic-tuner --dump-png out/panel-ja_2026-09-27_00-00-00.png --language ja
./build/frame-mic-tuner --dump-png out/panel-en-error_2026-09-27_00-00-00.png --language en --fake-error write --fake-autostart missing
./build/frame-mic-tuner --thumbnail-png out/thumbnail_2026-09-27_00-00-00.png --thumbnail-size 256
./build/frame-mic-tuner --test-record 3          # 3 秒録音 → 長さとピーク → メモリから再生
./build/frame-mic-tuner --contrast-report        # 色の組み合わせごとのコントラスト比と合否
./build/frame-mic-tuner --self-test              # 判定の関数（自己修復のオーバーレイの判定・出力の読み取り・タブの保存・ドラッグの終わらせ方・音の出口・パネルの当たり判定）を決まった入力で試す
./build/frame-mic-tuner --probe                  # 常駐しているパネルを SteamVR 経由で探して状態を出す
./build/frame-mic-tuner --version
```

- `--print` は `pw-link -l` のうちマイクの通り道の行も出す（表示とリンクが合っているかを見比べる用）。音の出口とマイクは、キー・既定にするノード（id）・今の既定（`*`）と、設定ファイルの出口ごとの設定を出す（読むだけ。移し替えもかけることもしない）
- `--dump-png` は今の実際の値で描く（設定ファイルは読むだけ）。`--fake` か `--fake-*` を付けると実際の値を読まずにダミーで描く（`--fake-echo on|off`・`--fake-ns on|off`・`--fake-idle`・`--fake-loading`・`--fake-autostart on|off|missing|unknown`・`--fake-error read|not-installed|links|write|write-echo|write-ns|write-mute|write-mute-on|write-output|write-input|autostart`）。`--preview-quit` で「もう一度押すと終了」の状態、`--language ja|en|sc` で言語、`--tab quick|fine` でタブを指定
- 画面と重ねた画面: `--view settings|apps`・`--overlay output|mic|apps|confirm|list-confirm`（音の出口を選ぶ・使うマイク・ささけんの Frame アプリ・入れる前の確認・一覧から開いた確認）・`--preview-scroll PX|end`（重ねた画面の一覧のスクロール）
- 音の出口のダミー（`--fake` のとき。つながっている出口は Frame のスピーカー・AB13X USB Audio・WH-1000XM4、マイクは内蔵・AB13X）: `--fake-output speaker|earphones`（今の出口。earphones = AB13X。`--fake-echo` を付けなければ、その出口のプリセットに合わせる）・`--fake-past-outputs N`（前に使った出口の数。既定 3 で、一覧がスクロールする）・`--fake-switched`（自動で切り替えた知らせ）・`--fake-external-mic`（外付けのマイクを使っている）・`--fake-viewing ab13x|past`（今の出口でない出口の設定を表示）
- ほかのアプリのダミー（同梱の一覧。eye と perf は動作中、keyboard は入っている）: `--fake-aux missing|installed|running`・`--fake-apps-busy KEYS`（Konsole を開いている呼び名。menu はメニュー）・`--fake-apps-extra N`（一覧にダミーのアプリを足す）・`--fake-confirm KEY|menu`・`--fake-launch busy|nodisplay|nokonsole|failed`（Konsole を開けなかった表示）
- 声のチェックの見た目: `--fake-recording`（録音中）・`--fake-history N`（ダミーの履歴 N 件）・`--fake-playing I`（I 件目を再生中）・`--fake-voice-error record|play`・`--preview-pressed earphone|speaker|record|mute|apps`（押している間。1 回描いてボタンの位置を知り、そこにポインターを置いて描き直す）
- ノイズ除去の強さの見た目: `--fake-ns-vad N`・`--fake-ns-grace N`（ダミーの値）・`--preview-drag-vad N`・`--preview-drag-grace N`（そのバーを N までドラッグしている）
- ミュートの見た目: `--fake-muted`（見出しが赤い「ミュート中」とミュートを解除）・`--fake-error write-mute`（解除に失敗した）・`--fake-error write-mute-on`（ミュートに失敗した）
- 更新の見た目（アプリと更新の画面）: `--fake-update unknown|uptodate|checking|available|manual|installing|installed|checkfailed|installfailed`・`--preview-update-confirm`
- `contrib/icons/frame-mic-tuner-{48,128,256}.png` は `--thumbnail-png` で書き出したもの（ダッシュボードのサムネイルと同じ絵）
- `--test-record [秒]` は録音の前・中・後の `pw-metadata -n filters` も出す。音声はメモリの中だけ
- ヘッドセットなしで閉じる動きを見る: 常駐に `--debug-record-on-open`（パネルが開いたら自動で録音）、別の端末から `--probe-switch-away [秒]`（アイコンの PNG を入れた一時的なダッシュボードのオーバーレイを作って `ShowDashboard` で切り替え、Mic のパネルを閉じた状態にする。画像の無いオーバーレイには切り替わらなかった）
- `--probe` は Background 型でつなぐだけ（オーバーレイも Vulkan も作らない）。`FindOverlay`・名前・幅・閉じるボタンのフラグ・表示中か・`GetOverlayTextureSize` を出す

## ＋（プログラムを起動）から使う

Frame の SteamVR ダッシュボードの「プログラムを起動」（＋）の一覧は、Steam クライアントが XDG の `.desktop` ファイル（`~/.local/share/applications/` など）を走査して作っている。SteamVR の `.vrmanifest` は関係ない。

`./install.sh` で入るファイル:

- `~/.local/bin/frame-mic-tuner`（実行ファイル。systemd のサービスもこれを使う）
- `~/.local/share/applications/frame-mic-tuner.desktop`（`Exec` を実行ファイルの絶対パスにしたもの。Steam から起動されると `PATH` に `~/.local/bin` が無いことがあるため）
- `~/.local/share/icons/hicolor/{48x48,128x128,256x256}/apps/frame-mic-tuner.png`（Steam は 256x256 だけではアイコンを見つけないことがあるので 3 サイズ）
- `~/.config/systemd/user/frame-mic-tuner.service`（置いて `systemctl --user daemon-reload` するだけ。**enable は `--autostart` を付けたときだけ**）
- WirePlumber のスクリプトと conf（上の「切り替えの仕組み」。同じ中身なら触らない）

＋から起動したときの動き:

- 常駐していないとき: そのまま常駐する
- すでに常駐しているとき（手で起動・systemd・＋のどれでも）: 2 つ目は SteamVR にはつながず、常駐しているほうに SIGUSR1 を送ってすぐ終わる（0.01 秒以内、終了コード 0）。常駐側は `IVROverlay::ShowDashboard("sasaken.frame-mic-tuner")` でダッシュボードを開き、Mic のパネルを出す
- 常駐の見分けは `$XDG_RUNTIME_DIR/frame-mic-tuner.lock`（無ければ `/run/user/<uid>/`）のロック（flock）と、そこに書いた PID。常駐が落ちるとロックは OS が外す
- 常駐を終わらせたいときは、ダッシュボードの「Mic」アイコンにホバーして「閉じる」、またはパネルの「終了」

削除は `./install.sh --uninstall`（設定と保存した `frame-mic.*` の値も消すなら `--purge`）。

## 自動起動（systemd ユーザーサービス）

パネルの「SteamVR と一緒に起動」で切り替える（`./install.sh` でユニットファイルを入れておく。`./install.sh --autostart` でも有効にできる）。

- オン = `systemctl --user enable frame-mic-tuner.service`（`~/.config/systemd/user/steamvr.service.wants/` にリンクができる）、オフ = `disable`。fork＋exec で固定の引数だけを渡す
- **`start` / `--now` はしない**。いま動いているインスタンスとぶつからないように。次に SteamVR（`steamvr.service`）が起動したときから効く
- 今の状態は `systemctl --user is-enabled frame-mic-tuner.service` で読む（`enabled` / `disabled` / `not-found`）。状態は systemd が持つので、このアプリの設定ファイルには保存しない
- ユニット: `After`・`PartOf`・`WantedBy=steamvr.service`、`Restart=always`、`RestartPreventExitStatus=3`、`SuccessExitStatus=3`、`ExecStart=%h/.local/bin/frame-mic-tuner`
- ログ: `journalctl --user -u frame-mic-tuner -f`

systemd から起動されたのに、すでに常駐がいるとき（手で起動したものが残っているなど）は、SIGUSR1 を送らずに**終了コード 3 で静かに終わる**（`Restart=always` で 5 秒ごとにパネルが開き続けるのを防ぐ）。

- 「systemd から起動された」は、環境変数 `INVOCATION_ID` があり、**かつ** `/proc/self/cgroup` が `.../frame-mic-tuner.service` のときだけとみなす
- `INVOCATION_ID` だけで決めない理由: Frame の Steam クライアント自体が `steam.service`（systemd のユーザーサービス）で動いていて `INVOCATION_ID` を持っており、＋から起動した子にも引き継がれる（2026-09-27 に `/proc/<steam の PID>/environ` で確認）。`INVOCATION_ID` だけで見ると、＋からの 2 回目の起動が「パネルを開く」にならずに黙って終わってしまう

## 設定

`~/.config/frame-mic-tuner/config.json`（`$XDG_CONFIG_HOME` があればその下）。**無ければ既定値で動く**。持つのは言語・タブ・新しい版の確認と、音の出口ごとの Frame マイクの設定。

```json
{
  "language": "ja",
  "tab": "quick",
  "update_check": true,
  "ns_vad_threshold_percent": 23,
  "ns_vad_grace_ms": 500,
  "outputs": {
    "alsa_output.platform-sound.HiFi__Speaker__sink": {"name": "Built-in Audio", "kind": "speaker", "echo": true, "ns": false, "ns_vad_threshold_percent": 23, "ns_vad_grace_ms": 500, "last_used": "2026-10-10"}
  }
}
```

| キー | 既定値 | 説明 |
|---|---|---|
| `outputs` | （無し） | 音の出口ごとの設定（上の「音の出口ごとの設定」）。キーは出口のキー、値は `name`（表示名）・`kind`（`"speaker"` / `"earphones"`）・`echo`・`ns`（true / false）・`ns_vad_threshold_percent`（0〜99）・`ns_vad_grace_ms`（0〜1000）・`last_used`（YYYY-MM-DD）。無い項目は初めての出口の値。**`outputs` が無いファイル（前の版）は、次の起動で今の設定を今の出口へ移して書く**。出口を全部忘れても `"outputs": {}` は残る |
| `update_check` | `true` | 新しい版の自動確認（起動時と 1 日 1 回）と、ほかのアプリの一覧の取得。アプリと更新の画面の「新しい版の確認」で切り替えると書かれる。オフでも「今すぐ確かめる」は押せる |
| `language` | Steam の言語 | 画面の文言の言語。無いときは Steam の言語設定（`~/.steam/registry.vdf` の `language`、読むだけ）が `japanese` なら日本語、`schinese`・`chinese` なら簡体字中国語、それ以外（`tchinese` を含む）は英語（Steam の値が読めなければ `LC_ALL`・`LC_MESSAGES`・`LANG` の `ja` / `zh_CN` も見る）。`"ja"`（日本語）、`"en"`（English）、`"sc"`（简体中文）。パネルの言語ボタンで変えると保存される（一時ファイルに書いてから置き換える） |
| `tab` | `"quick"` | 最後に見ていたタブ。`"quick"`（かんたん）か `"fine"`（細かく調整）。タブを切り替えたときに書かれ、パネルを開いたときにこのタブを出す |
| `ns_vad_threshold_percent` | （無し） | 前の版のノイズ除去の判定の厳しさ（0〜99。出口ごとに覚える前の 1 つだけの値）。前の版から初めて起動したときに今の出口へ移す。そのあとは今の出口の強さを写しておく（前の版に戻したとき用）。起動時、出口ごとの設定が分かるまではこの値をかけたい値にする。`ns_vad_grace_ms` と 2 つそろっているときだけ使う |
| `ns_vad_grace_ms` | （無し） | 前の版のノイズ除去の余韻（0〜1000ms）。同上 |

- 今かかっているマイクの設定（エコー除去・ノイズ除去）は WirePlumber が `~/.local/state/wireplumber/sm-settings` に保存する（`--save`）。再起動しても最後にかけた状態のまま。`outputs` は出口ごとに覚えておく分で、出口が変わるとそれをかける
- 別の設定ファイルを使うときは `--config パス`
- 文言は `src/i18n.cpp` の表にまとめてある（日本語・英語・簡体字中国語）。ログや `--print` の出力は日本語のまま

## 守っていること

- **PipeWire・WirePlumber を再起動しない**（再起動すると SteamVR・Steam Link・ゲームの音が戻らなくなる）。切り替えは `wpctl settings --save` だけ。既定の出口・マイクとミュートは、ユーザーが押したときだけ `wpctl set-default` / `wpctl set-mute` で変える
- ALSA のミキサー（amixer）を書き換えない（`VA DMIC MUX` を変えるとマイクの音量が 0 に落ちる罠がある）。`/etc`・Valve のスクリプトを書き換えない。sudo を使わない
- カメラ（`/dev/video*`）・GPIO・sysfs・`/persist` に触らない
- 外部コマンド（`wpctl`・`pw-link`・`systemctl`）は fork＋execvp で、シェルを通さず固定の引数だけで呼ぶ。標準出力と標準エラーはパイプで読み、2 秒（`systemctl enable/disable` は 5 秒）で終わらなければ SIGKILL、どの場合も `waitpid` で片付ける（ゾンビを残さない）。子プロセスではシグナルのマスクを空に戻してから exec する
- 外部コマンドはワーカースレッドで実行する（ポインターへの応答 33ms おきを止めない）。メインスレッドは mutex で状態の写しを読むだけ。SIGTERM・SIGINT・SIGUSR1 はワーカースレッドでは止めて、メインスレッドで受ける
- パネルを閉じている間に実行するのは、既定の出力・入力を見る `pw-metadata -n default`（2 秒おき、約 10ms）だけ。既定が変わったときだけ、一覧と今の設定を読んで、その出口の設定をかける。メインはイベントを 0.25 秒おきに見るだけ。ほかのアプリの一覧（frame-apps）は、「新しい版の確認」がオンのときに起動時と一覧を開いたときだけ取りに行く（1 時間に 1 回まで）

負荷（2026-09-27 に Frame で実測した前の版、`/proc/<pid>/stat` の CPU 時間。今の版では測っていない）:

| 状態 | 本体（全スレッド） | 子プロセス（wpctl・pw-link・systemctl） |
|---|---|---|
| パネルを閉じている | 10 秒で 0〜1 tick（10ms 程度） | なし（今の版は 2 秒おきの pw-metadata が増える。1 回 約 10ms） |
| パネルが開いている | 10 秒で約 100ms（1%） | 10 秒で約 190ms（1.9%）（今の版は 5 秒おきの pw-dump が増える。1 回 約 90ms） |

- 開いている間の読み直しは、`wpctl settings`（一覧 1 回で 2 つの設定を読む。キーごとに呼んでも 1 回の重さは同じなので、まとめて半分にした）・`pw-link -l`・`wpctl get-volume`・`pw-dump ns_capture` を 1 秒ごと、`systemctl --user is-enabled` と `pw-dump`（出口とマイクの一覧）を 5 秒ごと、`pw-metadata -n default` を 2 秒ごと
- 読み直した結果が前と同じなら描き直さない
- 録音中・再生中だけ、メーターと再生位置を動かすため 1 秒に 15 回描き直す

## 自己修復（ダッシュボードのオーバーレイが消えたとき）

2026-09-27 11:05 に、テストで 2 つ目のインスタンスを SteamVR につないで終わったあと、常駐のダッシュボードのオーバーレイが SteamVR から消えていた（常駐は動き続けていたが、`--probe` の `FindOverlay` が `VROverlayError_UnknownOverlay`。Mic のアイコンが見えず、SIGUSR1 で `ShowDashboard` を呼んでも何も開かなかった）。vrdashboard・vrcompositor の作り直しでも同じことが起きうるので、常駐が自分で気づいて作り直す。

- 確かめ方: `IVROverlay::FindOverlay("sasaken.frame-mic-tuner", &found)` を 1 回。判定は `judgeOverlay()`（OpenVR を呼ばない関数。`--self-test` で試せる）
  - `VROverlayError_None` で、`found` が自分の持っているハンドル → ある
  - それ以外のエラー（`UnknownOverlay` など）、または自分がハンドルを持っていない（前の作り直しに失敗）→ 消えている
  - `None` だが別のハンドル → 別のものがキーを持っている（作り直しを試すが、相手には触らない）
- 間隔: 3 秒おき（パネルを閉じている間も）。常駐のメインループは閉じている間 0.25 秒おきに回っているので、その中で時刻を見て呼ぶだけ。IPC 1 回なので、閉じている間の CPU はほぼ増えない。SIGUSR1（＋からの 2 回目の起動）で `ShowDashboard` を呼ぶ前にも確かめる
- 作り直しの手順（`VrOverlay::ensureOverlay()`）:
  1. 理由をログに出す（「自己修復: ダッシュボードのオーバーレイが SteamVR から消えています（FindOverlay(...) -> ...）。作り直します」）
  2. 自分の古いハンドルに `ClearOverlayTexture`（パネル・サムネイル）と `DestroyOverlay`。消えている前提なので、エラーは無視する
  3. `CreateDashboardOverlay` からやり直し（幅・入力・マウスの目盛り・閉じるボタンのフラグ。`connect()` と同じ関数 `createDashboardOverlay()`）
  4. 呼び出し側（main）がサムネイルとパネルの画像を描いて、今までどおり Vulkan の `SetOverlayTexture` で送り直す（Vulkan の画像はそのまま使い回す。`SetOverlayRaw` は使わない）
- 作れなかったとき（`KeyInUse` = 別のプロセスが同じキーを持っているなど）は、相手を消さずに、次の確かめ（3 秒後）で再試行する。同じ理由のログは連打しない。作れたら「作り直しました」と出す
- 実際に消えた状態を作るテストは、常駐を壊すので実機ではしていない（判定は `--self-test`、作り直しの流れはコードで確認）

## 画像の送り方と終了処理

- パネルとサムネイルの画像は Vulkan の `VkImage` を `IVROverlay::SetOverlayTexture` で渡す（画像は 2 枚を交互に使い、転送の完了を待ってから渡す）
- `SetOverlayRaw` は使わない。実機で、差し替えの直後 15〜40ms は画像が無い状態（`VROverlayError_InvalidTexture`）になってちらつき、共有メモリを使うので、プロセスが終わった直後の vrcompositor の SIGBUS の原因の疑いもあったため
- Vulkan は OpenVR が要求する拡張（`GetVulkanInstanceExtensionsRequired` / `GetVulkanDeviceExtensionsRequired`）と、HMD の GPU（`GetOutputDevice`）で作る
- パネルは見えているときだけ、変化があったときだけ描く。接続直後に 1 枚（読み込み中の表示）を入れておき、初めて選ばれたときに画像が無い瞬間を作らない
- ダッシュボードのアイコンの「閉じる」は `VROverlayFlags_EnableControlBarClose` → `VREvent_OverlayClosed` で届き、パネルの「終了」と同じく終了コード 3（systemd でも起動し直さない）。SteamVR 自体の終了（`VREvent_Quit`）は終了コード 0
- 診断で `GetOverlayImageData` を別のプロセスから呼ぶと、Vulkan のテクスチャが入ったオーバーレイでは**呼んだ側が SIGSEGV で落ちた**（2026-09-27。SteamVR 側は無事）。`--probe` では使わず、`GetOverlayTextureSize` で画像が入ったこと（1200x700・サムネイル 256x256）を確かめる

終了処理の順番（SIGTERM / SIGINT・`VREvent_Quit`・vrserver の消滅・「閉じる」・「終了」のどれでも同じ。各手順の戻り値はログに `[VR] 終了処理 ... -> VROverlayError_...` と出る）:

0. 声のチェックの録音・再生を止め、PipeWire のストリームとスレッドを片付ける（録った音はメモリごと消える）
1. `ClearOverlayTexture`（パネル・サムネイル）
2. `DestroyOverlay`（パネル。サムネイルは一緒に消える）
3. 400ms 待つ（コンポジタの約 36 フレーム分。外したテクスチャを手放してもらう）
4. `VR_Shutdown`
5. Vulkan の画像・バッファ・デバイス・インスタンスを壊す（OpenVR の決まりで `VR_Shutdown` の後）
6. ワーカースレッドを止めて待つ（実行中のコマンドがあれば、その終わりまで）

## ファイル

| ファイル | 役割 |
|---|---|
| `src/main.cpp` | コマンドライン、常駐のループ、二重起動（flock・SIGUSR1・サービスからの重複）、確認用オプション |
| `src/mic_state.*` | `wpctl`・`pw-link`・`systemctl` の出力を読む・書く（パーサ）。ミュートの読み書き |
| `src/outputs.*` | 音の出口とマイクの一覧（`pw-dump`・`pw-metadata -n default` のパーサ）、出口のキー、既定の切り替え（`wpctl set-default`）、出口ごとの設定の形 |
| `src/output_sync.*` | 起動したとき・出口が変わったときに出口の設定をかける・外からの変化を覚える・前の版からの移し替え（判断だけ。書き込みはワーカーに頼む） |
| `src/self_test.h`・`src/self_test_outputs.cpp` | `--self-test` の音の出口の分（Frame で取った `pw-dump`・`pw-metadata` の出力で試す） |
| `src/mic_worker.*` | 外部コマンドを実行するワーカースレッド（閉じている間の既定の見張りも） |
| `src/command.*` | fork＋execvp・パイプ・タイムアウト・waitpid |
| `src/mic_panel.*` | パネルの描画とボタンの当たり判定（描くたびに配置を作り直す。frame-ui の部品を使う）、重ねた画面と一覧のスクロール、サムネイル（マイクの絵） |
| `src/voice_check.*` | 声のチェック（pw_thread_loop・pw_stream での録音と再生、履歴 5 件、波形） |
| `src/theme.*` | 色の定義と WCAG のコントラスト比の計算（`--contrast-report`） |
| `src/vr_overlay.*` | OpenVR の接続、ダッシュボードのオーバーレイ、イベント、終了処理、`--probe` |
| `src/vk_texture.*`・`src/draw.*`・`src/json.*` | Vulkan の画像、描画の部品、JSON（同じ作者の別のオーバーレイと共通） |
| `src/i18n.*`・`src/config.*` | 文言の表（日本語・英語・簡体字中国語）、設定ファイル（言語・タブ・新しい版の確認・出口ごとの設定） |
| `vendor/frame-updater/` | 新しい版の確認と更新（共通のリポジトリからのコピー。直さない） |
| `vendor/frame-ui/` | 共通の UI 部品（色・寸法・切り替え・ボタン・カード・一番下の行・frame-ui の文言の表）。`sh ../frame-ui/sync.sh .` で写す。直さない |
| `vendor/frame-apps/` | ささけんのほかの Frame アプリの一覧と、インストーラーを Konsole で開く部品（アイコンを含む）。`sh ../frame-apps/sync.sh .` で写す。直さない |
| `contrib/` | `.desktop`・`.service`・アイコン・設定の例・`wireplumber/`（切り替えの仕組み） |
| `install.sh` | ビルドとインストール・アンインストール |
