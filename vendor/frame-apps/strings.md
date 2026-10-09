# ほかのアプリの文言（日本語・英語）

各アプリの i18n 表に写して使う。キー名は C++ の `UiText` のフィールド名にそのまま使える形にしてある。`%s`・`%d` は printf の書式。`%1$d` のような番号付きは、言語で順番が違うところ（printf の位置指定）。
パネルは幅が狭いので、1 行に収まらないときは各アプリで言い回しを縮めてよい（意味は変えない）。

アプリの名前と説明（「視線とまぶたを OSC で VRChat に送る」など）は文言の表ではなく、`frame_apps::catalog()` が日英で持つ（インストーラーのメニューと同じ文）。Konsole の最後の「終わりました。Enter で閉じます」も `konsoleScript()` が出す。

## アプリタブ

| キー | いつ | 日本語 | English |
|---|---|---|---|
| `appsRelatedTitle` | このアプリで使うアプリの見出し | いっしょに使うアプリ | Companion apps |
| `appsRelatedHint` | その下の補足 | このアプリの機能で使うものだけを出しています | Only the apps this app uses |
| `appsAllTitle` | 一覧を開く行・一覧の見出し | ささけんの Frame アプリ | Frame apps by sasaken@ |
| `appsAllCountFormat` | 一覧を開く行の補足（`%1$d` = 一覧の数、`%2$d` = 入っていない数） | %1$d 個のうち %2$d 個が入っていません | %2$d of %1$d not installed |
| `appsAllInstalledFormat` | 同・全部入っているとき（`%d` = 一覧の数） | %d 個すべて入っています | All %d are installed |
| `appsInstall` | 入っていないアプリのボタン | 入れる | Install |
| `appsChipRunning` | `Running` | 動作中 | Running |
| `appsChipInstalled` | `Installed` | 入っています | Installed |
| `appsChipBusy` | `busy`（そのアプリの Konsole が開いている） | Konsole で実行中… | Running in Konsole… |
| `appsRelatedTag` | 一覧で、このアプリで使うものに付ける札 | このアプリで使う | Used by this app |

## 一覧

| キー | いつ | 日本語 | English |
|---|---|---|---|
| `appsListSub` | 見出しの下 | 入れる・更新する・消すは、Konsole のインストーラーで行います | Install, update and remove in the installer in Konsole |
| `appsListNote` | 下の補足 | 一覧は「新しい版の確認」がオンのとき、frame.sasaken1102s.net から取ってきます（1 時間に 1 回まで。取れなければ前回の分か同梱の一覧） | With update checks on, this list comes from frame.sasaken1102s.net (at most hourly; otherwise the last one or the built-in one) |
| `appsOpenInstaller` | 下のボタン | インストーラーを開く | Open the installer |
| `appsClose` | 右上の ✕（読み上げ用の名前） | 閉じる | Close |

## 確認

| キー | いつ | 日本語 | English |
|---|---|---|---|
| `appsConfirmTitleFormat` | 1 つを入れる（`%s` = アプリの名前） | %s を入れる | Install %s |
| `appsConfirmMenuTitle` | メニューを開く | インストーラーを開く | Open the installer |
| `appsConfirmLead` | コマンドの上 | Konsole が開いて、このコマンドを実行します | Konsole opens and runs this command |
| `appsConfirmProgress` | 1 つを入れるとき | 進み具合は Konsole に出ます（聞かれることがあれば、そこで答えます） | Konsole shows the progress (answer any questions there) |
| `appsConfirmMenu` | メニューのとき | メニューから、入れる・更新する・消すアプリを番号で選びます | In the menu, pick the apps to install, update or remove by number |
| `appsConfirmNoSudo` | いつも | sudo は使いません。入るのはホームフォルダの中だけです | No sudo; everything goes into your home folder |
| `appsConfirmClose` | いつも | 終わったら Konsole を閉じてね。このパネルの表示も変わります | Close Konsole when it's done; this panel updates too |
| `appsConfirmCancel` | ボタン | やめる | Cancel |
| `appsConfirmLaunch` | ボタン | Konsole で開く | Open in Konsole |

## 開けなかったとき（`LaunchResult`）

| キー | `LaunchResult` | 日本語 | English |
|---|---|---|---|
| `appsErrorBusy` | `Busy` | 同じアプリの Konsole がまだ開いています | A Konsole for this app is still open |
| `appsErrorNoDisplay` | `NoDisplay` | Konsole を開けませんでした: 画面が見つかりません（DISPLAY がありません） | Couldn't open Konsole: no screen (DISPLAY isn't set) |
| `appsErrorNoKonsole` | `NoKonsole` | Konsole を開けませんでした: Konsole が入っていません | Couldn't open Konsole: Konsole isn't installed |
| `appsErrorFailed` | `Failed`・`UnknownApp` | Konsole を開けませんでした | Couldn't open Konsole |
