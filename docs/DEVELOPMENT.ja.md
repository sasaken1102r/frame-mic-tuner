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

| 行 | ボタン | 動き |
|---|---|---|
| タブ（左の列の上） | **かんたん** / **細かく調整** | 左の列の中身を切り替える（ピル型の切り替え。選択中はアクセントの塗り＋✓＋太字）。最後に見ていたタブを `config.json` の `tab` に保存し、次に開いたときもそのタブ（無ければかんたん）。バーのドラッグ中にタブを切り替えた・パネルを閉じたときは、先にドラッグを終わらせて最後の値を送って保存する。つながりのカードは両方のタブで下に出す |
| どこで音を聞いてる？（かんたんのタブ、プリセット） | **イヤホン・ヘッドホン** / **Frame のスピーカー** | 設定をまとめて切り替えるプリセット。イヤホン = エコー除去オフ・ノイズ除去オフ、スピーカー = エコー除去オン・ノイズ除去オフ（押すと 1 つのコマンドとしてワーカーに渡し、`wpctl settings --save` をエコー除去 → ノイズ除去の順に 2 回書いてから、1 回だけ読み直して表示に反映する。途中の値でグレーが挟まらないよう、書き終わるまで（受付番号の書き込みが終わるまで、最長 8 秒）押したカードを選択中の見た目で保つ。片方が失敗しても、もう片方は書き、「エコー除去の切り替えに失敗（wpctl）」「ノイズ除去の切り替えに失敗（wpctl）」のどちらか（両方なら「切り替えに失敗（wpctl）」）を赤で出して、読み直した実際の値を表示する）。ノイズ除去の強さのバーは変えない。各カードに、押すと何になるかのチップ 2 つ（「エコー除去 オフ」「ノイズ除去 オフ」など）を出す。今の設定と一致するカードが選択中。一致の判定はエコー除去とノイズ除去の 2 つだけ（バーの値はノイズ除去がオフなら効かないので比べない）。どちらとも一致しないときは両方のカードをグレー（地の色の塗り＋点線の枠＋暗めの文字。押せるまま）にして、カードの下の行を「選ぶとおすすめの設定になります」から「今は細かく調整した設定です」（太字）と「細かく調整を見る →」のボタンに替える。値を読み込み中はグレーにしない |
| エコー除去（細かく調整のタブ） | オン / オフ | `frame-mic.echo-cancel` |
| ノイズ除去（細かく調整のタブ） | オン / オフ | `frame-mic.noise-suppression`。オンにすると小さい音（口の音など）は消える |
| 判定の厳しさ / 余韻（細かく調整のタブ） | バー（ドラッグ）、− / ＋、標準に戻す | ノイズ除去の強さ（下の「ノイズ除去の強さ」）。ノイズ除去がオフの間はグレー（押せるまま）で「オフの間は効きません」。「標準に戻す」はバーの下の行に、「標準は 23%・500ms（SteamOS の値）」と一緒に置く |
| つながり | （表示だけ） | 「マイク ─ 音質補正 ─ エコー除去 ─ ノイズ除去 ─ アプリへ」のパイプライン図。実際に通っている段はアクセントの薄い塗り＋実線の枠＋太字で、通り道の線もアクセント。通っていない段は点線の枠＋細字で、線は下を回って飛ばす |
| 声のチェック | ● 録音 / ■ 停止、履歴の ▶ / ■ | アプリに届く音を最大 10 秒録って、5 件まで聞き比べる（下の「声のチェック」） |
| 更新の帯（全幅のカード） | 今すぐ確かめる / 更新する（確認: やめる・更新する）/ もう一度・閉じる / 閉じる | `vendor/frame-updater` の `UpdateChecker` の状態を出す（文言は strings.md のまま）。最新・確認中・更新中・版だけ（未確認）は普段の枠。新しい版あり・確認・入れ終わりはアクセントの枠、更新の失敗は赤い枠（理由、もう一度・閉じる）。確認の失敗は文字だけ赤で枠は普段のまま（今の版は動いたままで、オフラインだと毎日出るため）。「更新する」の 1 回目は確認の表示（2 行目に補足、やめる・更新する）にするだけで、3 秒以内の 2 回目で `install()`。やめる・ほかのボタン・3 秒で取り消し。更新中は 2 行目に確認と同じ補足。確認の補足と入れ終わりの文は、install.sh が常駐を再起動しないこのアプリに合わせて strings.md から変えている（「終わったら、終了して起動し直すと新しい版になります」）。入れ終わりは 2 行目に「WirePlumber のスクリプトも変わったときはヘッドセットも再起動」。手で更新する版は 2 行目に理由とリリースページの URL。確かめている間はボタンを出さない。インストールは `frame-update.sh install --detach`（`frame-mic-tuner-update` の一時ユニット）で、`install.sh` は動いている常駐を再起動しないので、新しい版になるのは終了して起動し直したとき |
| 下の 1 行: 言語 | 日本語 / English / 简体中文 | 日本語・英語・簡体字中国語の文言をすぐ切り替える（設定ファイルに保存） |
| 下の 1 行: SteamVR と一緒に起動 | オン / オフ | `systemctl --user enable` / `disable frame-mic-tuner.service`（下の「自動起動」）。ユニットファイルが入っていないときはグレーで押せず、「準備されていません」と出る |
| 下の 1 行: 終了 | | 押すと 3 秒間「もう一度押すと終了」になり、その間にもう一度押すと終了（終了コード 3） |

画面の並び（横長の 2 カラム）:

- 左のカラム = 切り替え: 見出し「マイク」と使用中 / 未使用のバッジ → タブ（かんたん / 細かく調整、高さ 48）→ タブの中身 → つながりのパイプライン図（両方のタブで共通）
  - かんたん: 「どこで音を聞いてる？」（19px）→ プリセットのカード 2 枚（全幅で縦に並べる。左に絵、真ん中に名前 28px 目安と効果 17px、右に縦に並べたチップ 2 つ 14px。選択中はアクセントの塗り＋光彩＋名前の前の ✓。チップは地の色のピルで、選択中の上ではアクセントの文字。どちらとも一致しないときは両方グレー）→ カードの下の 1 行（説明、または「今は細かく調整した設定です」と「細かく調整を見る →」）
  - 細かく調整: エコー除去・ノイズ除去のスライド式の切り替え（高さ 52、右に 1 行の説明 16px）→ 強さのバー 2 本（見出し 17px・説明 14px・− / ＋ 42px・値 19px）→ 「標準は 23%・500ms（SteamOS の値）」と「標準に戻す」
- 2026-09-27 に「プリセットだと分かりにくい」という声を受けて見出し・チップ・区切りを足し、そのあと「かんたん / 細かく調整」のタブに分けた（高さは 700 のまま。タブにしたぶん、カードの名前・チップ・バーの説明の文字を大きくした）
- 右のカラム = 声のチェック: 録音ボタンとメーターの行、履歴 5 件（1 行目に時刻と長さ、2 行目に録ったときの設定、右に横長の波形）
- 更新の帯（全幅のカード、高さ 70）: 左に状態の文（19px。補足があれば 2 行目に 15px の灰色）、右にボタン（高さ 50 のピル。「更新する」はアクセントの塗り）。色・角丸・ボタンはほかのカードと同じ theme のトークン
- 下の 1 行（全幅）: 言語・SteamVR と一緒に起動・終了。いちばん下は 1 行だけで、失敗（赤）があればそれ、無ければ自動起動が使えない理由、どちらも無ければ説明
- 大きさ: 1200×788 px を幅 2.8m で出す（高さ約 1.84m。1px あたり約 429 px/m）。ノイズ除去の強さのバー 2 本（約 90px）を足して 650 → 700、更新の行を足して 758、それを帯（カード）にして 788 にした

- 右上のバッジは「● 使用中」（緑）/「○ 未使用」（灰色）。最下行は、失敗したときだけ赤い文字（例:「切り替えに失敗（wpctl）」）になる
- ボタンの選択状態・使用中か・つながりは、実際の値から作る
  - エコー除去・ノイズ除去: `wpctl settings`（全設定の一覧）の `frame-mic.*` の `Value:`
  - つながり: `pw-link -l` の実際のリンク。マイク（`alsa_input.platform-sound.HiFi__Mic__source`）から出発して、`<名前>_capture` に入り `<名前>_source` から出るフィルターをたどり、`alsa_loopback_stream.alsa_input...`（既定のマイクの手前）に着くまでを並べる。`eq` = 音質補正（英語では EQ）、`echo_cancel` = エコー除去、`ns` / `dsp` = ノイズ除去
  - マイク使用中 = 通り道にフィルターが入っている。tracker は録音のストリーム（`Stream/Input/Audio`）が 1 つでもあると EQ を必ず入れるので、「使用中」は「tracker がフィルターを入れている」と同じ。**スピーカーの音を録るアプリ（画面録画の ffmpeg など）でも録音のストリームなので数えられる**
  - 未使用のときの `pw-link -l` は、マイクから `alsa_loopback_stream...` へ直接つながる形（「マイク → アプリへ」と「使っていないので処理はお休み中」）
- パネルがダッシュボードで**開いている間だけ** 1 秒ごとに読み直す（外から `wpctl settings --save ...` で変えられても、1 秒以内に表示が合う）。自動起動の状態（`systemctl --user is-enabled`）はめったに変わらないので、開いた直後と 5 秒おき
- ボタンを押したら、書く → すぐ読み直す → 表示に反映。書いた値は読み返して確かめ、違えば失敗として赤く出す
- スクリーンショット: `docs/v10-*-quick-speaker_*.png`（`--dump-png --tab quick --fake --fake-echo on --fake-ns off --fake-history 5 --fake-playing 1 --fake-update uptodate` で書き出したもの）と `docs/v10-*-fine_*.png`（`--dump-png --tab fine --fake --fake-echo on --fake-ns on --fake-ns-vad 10 --fake-ns-grace 800 --fake-history 5 --fake-playing 2 --fake-update uptodate`）。どちらも `--language ja|en|sc` で日本語・英語・簡体字中国語を撮る。`--tab quick|fine` で描くタブを選べる。更新の帯は `--fake-update` を付けないと「v0.2.0」（未確認）になるので、スクリーンショットでは `uptodate`（最新版です）にそろえる

## ノイズ除去の強さ

ノイズ除去の段は、Valve の filter-chain（`/etc/pipewire/microphone-filter-chain/microphone-filter-chain-echo-cancel-cpu.conf`）の LADSPA `noise_suppressor_mono`。その control を、PipeWire を再起動せずにその場で変える。

| バー | param 名（`ns_capture` の Props の params） | 範囲（PropInfo） | SteamOS の値 | 刻み（− / ＋） |
|---|---|---|---|---|
| 判定の厳しさ | `noise_suppressor_mono:VAD Threshold (%)` | 0〜99（プラグインの既定 49.5） | 23 | 1% |
| 余韻 | `noise_suppressor_mono:VAD Grace Period (ms)` | 0〜1000（既定 500） | 500 | 50ms |

- 送り先は `ns_capture`（filter-chain の入口のノード）。`ns_source` の Props には control は無い（2026-09-27 に `pw-cli enum-params <id> PropInfo` / `Props` で確認）
- 読む: `pw-dump ns_capture`（1 回で id と値が取れる。約 40ms）の `info.params.Props[].params`（`[名前, 値, 名前, 値, ...]`）
- 書く: `pw-cli set-param <id> Props '{ params = [ "noise_suppressor_mono:VAD Threshold (%)" 23.0 "noise_suppressor_mono:VAD Grace Period (ms)" 500.0 ] }'`。fork＋execvp・シェルなしで、数値はアプリで範囲に丸めて（1% / 10ms 単位）から文字列にする。2 つはいつもまとめて送る
- 同じ PropInfo に `Retroactive VAD Grace (ms)`（0〜200、SteamOS は 0）もあるが、画面には出していない
- SteamOS は PipeWire の起動のたびに filter-chain の設定の値（23 / 500）に戻す。そこで、アプリの `config.json` に保存して（`ns_vad_threshold_percent`・`ns_vad_grace_ms`。バーを動かすまでは書かない）、次のときにかけ直す:
  - アプリが起動したとき（ワーカーがノードを探し、見つかるまで 5 秒おきに試す。かけたら止まる。パネルを閉じていても、この起動直後の分だけは動く）
  - パネルが開いている間の 1 秒おきの読み直しで、ノードの id が変わっていたとき（PipeWire がノードを作り直した）
  - パネルを閉じている間に PipeWire が作り直されたときは、次にパネルを開いたときにかけ直す
- 外から `pw-cli` で変えられたときは、かけ直さずに表示を追いつかせる（id が同じなら今の値を正とする）
- バーは押したところへ飛び、そのままドラッグできる（溝の上下に外れても横の位置で決める）。ドラッグ中は、値が変わっていれば 100ms おきに最後の値だけ送る（ワーカーの待ち行列でも、まだ実行していない前の値は捨てる）。離したとき（パネルから外れたときも）に必ず 1 回送って保存する。書いたあとは読み直しが追いつくまで 1.5 秒、書いた値を表示に使う（ちらつかないように）
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
- 履歴は新しい順に 5 件。6 件目を録るといちばん古いものを消す。各行に録った時刻（例: 00:41）、長さ、録ったときの設定（「イヤホン」「スピーカー＋ノイズ除去」など）、小さい波形、▶ 再生 / ■ 停止。再生中の行は、再生済みの部分と位置の線がアクセント色で動く。再生中にほかの行の ▶ を押すとそちらに切り替わる
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

- 色は `src/theme.h` の 1 か所にまとめてある。描画はここの色だけを使い、`--contrast-report` が同じ定義から WCAG 2.x のコントラスト比を計算して、組み合わせごとの比と合否を出す
- 背景の段は GitHub ダーク系: `#0d1117`（地）/ `#161b22`（カード）/ `#21262d`（ボタン）/ `#30363d`（乗っている・押している）。カードは内側の 1px のハイライトと重ねた影で浮かせる。角はピル型
- イメージカラーは `#e27dfd`。選択中の塗り・通っている段・再生位置・メーターに使う。押している間は `#c45fe0`。薄い塗りや光彩は 20%
- 決まり: 文字は大きさによらずすべて 4.5:1 以上で確かめる。ボタンの枠・選択状態・図（WCAG 1.4.11）は 3:1 以上。押せないボタンの文字は WCAG では例外だが、読める程度（3:1 目安）にする
- アクセントの塗りの上の文字は濃い色 `#0d1117`（7.74:1）。白は 2.4:1 で使えない。暗い地の上のアクセント色の文字は 7.07〜7.74:1
- ボタンの枠は `#30363d` では 1.3:1 しかないので、`#6e7681`（カードの上で 3.77:1）にしている
- 補足の灰色 `#9198a1` はアクセントの薄い塗りの上では 4.17:1 で足りないので、その組み合わせは使わない（通っている段の文字は `#e6edf3`、10.33:1）
- 選択状態を色だけで伝えない: 選択中は ✓・塗り・太字、通っていない段は点線の枠・細字、録音中は ● と「録音中」の文字、使用中は ● / 未使用は ○ と文字
- どちらのプリセットとも一致しないときのグレーのカードは、押せるので非活性の例外にはせず、文字は `#7d8590`（地の上で 5.07:1、乗っている間の `#21262d` の上で 4.08:1）で読めるようにしている
- 2026-09-27 の結果: 65 組すべて合格。いちばん低いのは「チップの枠（選ばれていないカードに乗っている間）」`#6e7681` / `#21262d` の 3.31:1（部品は 3:1 以上）

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
./build/frame-mic-tuner --print                  # 今の値・マイク使用中か・つながり・自動起動を表示
./build/frame-mic-tuner --set-echo off           # エコー除去を切ってから表示（イヤホン）。on で入れる（スピーカー）
./build/frame-mic-tuner --set-ns on              # ノイズ除去を入れてから表示
./build/frame-mic-tuner --set-autostart on       # systemctl --user enable してから表示（off で disable）
./build/frame-mic-tuner --set-ns-vad 10 --set-ns-grace 800   # ノイズ除去の強さをその場でかけてから表示（保存はしない）
./build/frame-mic-tuner --dump-png out/panel-ja_2026-09-27_00-00-00.png --language ja
./build/frame-mic-tuner --dump-png out/panel-en-error_2026-09-27_00-00-00.png --language en --fake-error write --fake-autostart missing
./build/frame-mic-tuner --thumbnail-png out/thumbnail_2026-09-27_00-00-00.png --thumbnail-size 256
./build/frame-mic-tuner --test-record 3          # 3 秒録音 → 長さとピーク → メモリから再生
./build/frame-mic-tuner --contrast-report        # 色の組み合わせごとのコントラスト比と合否
./build/frame-mic-tuner --self-test              # 判定の関数（自己修復のオーバーレイの判定・出力の読み取り・タブの保存・ドラッグの終わらせ方）を決まった入力で試す
./build/frame-mic-tuner --probe                  # 常駐しているパネルを SteamVR 経由で探して状態を出す
./build/frame-mic-tuner --version
```

- `--print` は `pw-link -l` のうちマイクの通り道の行も出す（表示とリンクが合っているかを見比べる用）
- `--dump-png` は今の実際の値で描く。`--fake` か `--fake-*` を付けると実際の値を読まずにダミーで描く（`--fake-echo on|off`・`--fake-ns on|off`・`--fake-idle`・`--fake-loading`・`--fake-autostart on|off|missing|unknown`・`--fake-error read|not-installed|links|write|autostart`）。`--preview-quit` で「もう一度押すと終了」の状態、`--language ja|en|sc` で言語を指定
- 声のチェックの見た目: `--fake-recording`（録音中）・`--fake-history N`（ダミーの履歴 N 件）・`--fake-playing I`（I 件目を再生中）・`--fake-voice-error record|play`・`--preview-pressed earphone|speaker|record`（押している間）
- ノイズ除去の強さの見た目: `--fake-ns-vad N`・`--fake-ns-grace N`（ダミーの値）・`--preview-drag-vad N`・`--preview-drag-grace N`（そのバーを N までドラッグしている）
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

`~/.config/frame-mic-tuner/config.json`（`$XDG_CONFIG_HOME` があればその下）。**無ければ既定値で動く**。持つのは言語と、ノイズ除去の強さ（バーを動かしたときだけ）。

```json
{
  "language": "ja"
}
```

| キー | 既定値 | 説明 |
|---|---|---|
| `language` | Steam の言語 | 画面の文言の言語。無いときは Steam の言語設定（`~/.steam/registry.vdf` の `language`、読むだけ）が `japanese` なら日本語、`schinese`・`chinese` なら簡体字中国語、それ以外（`tchinese` を含む）は英語（Steam の値が読めなければ `LC_ALL`・`LC_MESSAGES`・`LANG` の `ja` / `zh_CN` も見る）。`"ja"`（日本語）、`"en"`（English）、`"sc"`（简体中文）。パネルの言語ボタンで変えると保存される（一時ファイルに書いてから置き換える） |
| `tab` | `"quick"` | 最後に見ていたタブ。`"quick"`（かんたん）か `"fine"`（細かく調整）。タブを切り替えたときに書かれ、パネルを開いたときにこのタブを出す |
| `ns_vad_threshold_percent` | （無し） | ノイズ除去の判定の厳しさ（0〜99）。バーを動かしたときに書かれ、アプリが起動したときにかけ直す。`ns_vad_grace_ms` と 2 つそろっているときだけ使う |
| `ns_vad_grace_ms` | （無し） | ノイズ除去の余韻（0〜1000ms）。同上。無いときは何もかけず、SteamOS の値（23 / 500）のまま |

- マイクの設定（エコー除去・ノイズ除去）は WirePlumber が `~/.local/state/wireplumber/sm-settings` に保存する（`--save`）。再起動しても最後に選んだ状態のまま
- 別の設定ファイルを使うときは `--config パス`
- 文言は `src/i18n.cpp` の表にまとめてある（日本語・英語・簡体字中国語）。ログや `--print` の出力は日本語のまま

## 守っていること

- **PipeWire・WirePlumber を再起動しない**（再起動すると SteamVR・Steam Link・ゲームの音が戻らなくなる）。切り替えは `wpctl settings --save` だけ
- ALSA のミキサー（amixer）を書き換えない（`VA DMIC MUX` を変えるとマイクの音量が 0 に落ちる罠がある）。`/etc`・Valve のスクリプトを書き換えない。sudo を使わない
- カメラ（`/dev/video*`）・GPIO・sysfs・`/persist` に触らない
- 外部コマンド（`wpctl`・`pw-link`・`systemctl`）は fork＋execvp で、シェルを通さず固定の引数だけで呼ぶ。標準出力と標準エラーはパイプで読み、2 秒（`systemctl enable/disable` は 5 秒）で終わらなければ SIGKILL、どの場合も `waitpid` で片付ける（ゾンビを残さない）。子プロセスではシグナルのマスクを空に戻してから exec する
- 外部コマンドはワーカースレッドで実行する（ポインターへの応答 33ms おきを止めない）。メインスレッドは mutex で状態の写しを読むだけ。SIGTERM・SIGINT・SIGUSR1 はワーカースレッドでは止めて、メインスレッドで受ける
- パネルを閉じている間は外部コマンドを 1 つも実行しない（ワーカーは頼まれるまで待つだけ）。メインはイベントを 0.25 秒おきに見るだけ

負荷（2026-09-27 に Frame で実測、`/proc/<pid>/stat` の CPU 時間）:

| 状態 | 本体（全スレッド） | 子プロセス（wpctl・pw-link・systemctl） |
|---|---|---|
| パネルを閉じている | 10 秒で 0〜1 tick（10ms 程度） | なし |
| パネルが開いている | 10 秒で約 100ms（1%） | 10 秒で約 190ms（1.9%） |

- 開いている間の読み直しは、`wpctl settings`（一覧 1 回で 2 つの設定を読む。キーごとに呼んでも 1 回の重さは同じなので、まとめて半分にした）と `pw-link -l` を 1 秒ごと、`systemctl --user is-enabled` を 5 秒ごと
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
| `src/mic_state.*` | `wpctl`・`pw-link`・`systemctl` の出力を読む・書く（パーサ） |
| `src/mic_worker.*` | 外部コマンドを実行するワーカースレッド |
| `src/command.*` | fork＋execvp・パイプ・タイムアウト・waitpid |
| `src/mic_panel.*` | パネルの描画とボタンの当たり判定（描くたびに配置を作り直す）、サムネイル（マイクの絵） |
| `src/voice_check.*` | 声のチェック（pw_thread_loop・pw_stream での録音と再生、履歴 5 件、波形） |
| `src/theme.*` | 色の定義と WCAG のコントラスト比の計算（`--contrast-report`） |
| `src/vr_overlay.*` | OpenVR の接続、ダッシュボードのオーバーレイ、イベント、終了処理、`--probe` |
| `src/vk_texture.*`・`src/draw.*`・`src/json.*` | Vulkan の画像、描画の部品、JSON（同じ作者の別のオーバーレイと共通） |
| `src/i18n.*`・`src/config.*` | 文言の表（日本語・英語・簡体字中国語）、設定ファイル（言語とノイズ除去の強さ） |
| `contrib/` | `.desktop`・`.service`・アイコン・設定の例・`wireplumber/`（切り替えの仕組み） |
| `install.sh` | ビルドとインストール・アンインストール |
