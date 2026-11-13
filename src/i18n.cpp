// 画面に出す文言の表の中身。
#include "i18n.h"

#include <cstdlib>
#include <fstream>

namespace {

/**
 * Steam の言語設定を読む。~/.steam/registry.vdf の最初の "language" の値（"japanese" など）。
 * @return 値。読めなければ空
 */
std::string steamLanguage() {
    const char* home = std::getenv("HOME");
    if (home == nullptr || home[0] == '\0') return "";
    std::ifstream file(std::string(home) + "/.steam/registry.vdf");
    std::string line;
    while (std::getline(file, line)) {
        // 形式: <タブ>"language"<タブ>"japanese"
        const std::string key = "\"language\"";
        const size_t at = line.find(key);
        if (at == std::string::npos) continue;
        const size_t open = line.find('"', at + key.size());
        const size_t close = open == std::string::npos ? open : line.find('"', open + 1);
        if (close == std::string::npos) return "";
        return line.substr(open + 1, close - open - 1);
    }
    return "";
}

/**
 * ロケールの環境変数（LC_ALL → LC_MESSAGES → LANG の順で最初に空でないもの）が言語コードで始まるか。
 * @param languagePrefix 言語コード
 * @return 一致するなら true
 */
bool localeEnvVarsAreSetTo(const char* languagePrefix) {
    for (const char* name : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
        const char* value = std::getenv(name);
        if (value != nullptr && value[0] != '\0') return std::string(value).rfind(languagePrefix, 0) == 0;
    }
    return false;
}

/**
 * システム言語を調べる（systemLanguage の本体）。
 * @return 言語
 */
Language detectSystemLanguage() {
    const std::string steam = steamLanguage();
    if (!steam.empty()) {
        if (steam == "japanese") return Language::Ja;
        if (steam == "schinese" || steam == "chinese") return Language::Sc;
        if (steam == "german") return Language::De;
        if (steam == "french") return Language::Fr;
        if (steam == "spanish" || steam == "latam") return Language::Es;  // latam = スペイン語（中南米）
        if (steam == "italian") return Language::It;
        return Language::En;
    }
    if (localeEnvVarsAreSetTo("ja")) return Language::Ja;
    if (localeEnvVarsAreSetTo("zh_CN")) return Language::Sc;
    if (localeEnvVarsAreSetTo("de")) return Language::De;
    if (localeEnvVarsAreSetTo("fr")) return Language::Fr;
    if (localeEnvVarsAreSetTo("es")) return Language::Es;
    if (localeEnvVarsAreSetTo("it")) return Language::It;
    return Language::En;
}

// 7 つの表は UiText のフィールドの順（i18n.h の区切りごと）に並べる
const UiText kJapanese = {
    // 見出し
    "マイク", "使用中", "未使用", "ミュート中", "読み込み中…", "状態不明", "ミュートする", "ミュートを解除",
    "アプリと更新", "マイクの設定へ",
    // 音の出口・マイクの欄
    "音の出口", "マイク", "%d 個", "Frame のスピーカー", "スピーカー", "Frame 内蔵マイク", "Frame 内蔵", "%s のマイク",
    // タブ
    "かんたん", "細かく調整",
    // かんたん
    "%s で聞いているとき",
    "イヤホン・ヘッドホン", "Frame のスピーカー",
    "小さな音まで届く", "スピーカーの音を消す",
    "エコー除去 オフ", "エコー除去 オン", "ノイズ除去 オフ",
    "%s のときは、いつもこの設定になります。つないだら自動で切り替えます",
    "いまは「細かく調整」の設定を使っています",
    "%s をつないだので、覚えていた設定にしました", "元に戻す",
    // 細かく調整
    "エコー除去", "スピーカーの音を消す", "ノイズ除去", "口の音など小さい音も消える", "オン", "オフ",
    "判定の厳しさ", "低いほど声以外も通る", "余韻", "声のあと音を通す時間", "オフの間は効きません", "標準に戻す",
    "標準は 23%・500ms（SteamOS の値）",
    // 外付けのマイク
    "いまは %s を使っています",
    "エコー除去・ノイズ除去がかかるのは、Frame の内蔵マイクだけです。このマイクの音は、そのままアプリに届きます。",
    "%1$s のときの設定（%2$s）は、内蔵マイクに戻すと使います", "Frame 内蔵マイクに戻す",
    // 設定のまとめ
    "エコー除去オン", "エコー除去オフ", "・ノイズ除去オン", "・",
    // つながり
    "つながり", "マイク", "音質補正", "エコー除去", "ノイズ除去", "アプリへ", "使っていないので処理はお休み中",
    "つながりを読めません",
    // 声のチェック
    "声のチェック", "アプリに届く音を録って聞き比べ", "録音", "停止", "録音中", "秒", "%.1f 秒・残り %.1f 秒",
    "「録音」で最大 10 秒。いま使っているマイクで録ります", "まだ録音はありません", "%s のマイク（処理なし）", "設定不明",
    // 一番下の行の注意書き
    "押すとすぐ切り替わり、音の出口ごとに覚えます。録音はメモリの中だけで、終了すると消えます",
    "自動起動は準備されていません（./install.sh を実行してください）", "自動起動の状態を読めません",
    // 失敗
    "読み取りに失敗（wpctl）", "切り替えの仕組みが入っていません（./install.sh のあと再起動）",
    "つながりの読み取りに失敗（pw-link）", "切り替えに失敗（wpctl）", "自動起動の切り替えに失敗（systemctl）",
    "録音を始められません（PipeWire）", "再生できません（PipeWire）", "ノイズ除去の強さを変えられません（pw-cli）",
    "エコー除去の切り替えに失敗（wpctl）", "ノイズ除去の切り替えに失敗（wpctl）",
    "ミュートを解除できませんでした（wpctl）", "ミュートできませんでした（wpctl）",
    "音の出口を切り替えられませんでした（wpctl）", "マイクを切り替えられませんでした（wpctl）",
    // 音の出口を選ぶ
    "音の出口を選ぶ", "出口ごとに Frame マイクの設定を覚えて、つないだら自動で切り替えます",
    "つながっている出口", "前に使った出口（いまはつながっていない）",
    "いま使用中", "つながっています", "最後に使ったのは %s", "前に使った出口", "設定を表示中",
    "ここから音を出す", "忘れる",
    "新しい出口は、Frame のスピーカーなら「スピーカー」、それ以外は「イヤホン」の設定から始めます。"
    "「ここから音を出す」は SteamVR の音声の設定と同じものを切り替えます。多いときはスティックかドラッグで送れます",
    "頭切れを防ぐ",
    "頭切れを防ぐ：無音のときにアンプを切るアダプター向け。聞こえない音を流し続けるので、静かなときに小さくサーと聞こえることがあり、"
    "電池も少し使います",
    // 使うマイク
    "使うマイク", "エコー除去・ノイズ除去がかかるのは、Frame の内蔵マイクだけです", "エコー除去・ノイズ除去がかかります",
    "処理はかかりません（そのままアプリへ）", "このマイクを使う",
    // アプリと更新: このアプリ
    "このアプリ", "マイクのエコー除去・ノイズ除去を切り替える",
    "起動したときと 1 日 1 回、GitHub で新しい版を確かめます", "自動では確かめません。「今すぐ確かめる」で確かめられます",
    "新しい版の確認", "オフにすると、GitHub にもアプリの一覧（frame.sasaken1102s.net）にも取りに行きません",
    "使い方・不具合の報告",
    "ダウンロードして入れ替えます。終わったら、終了して起動し直すと新しい版になります",
    "%s を入れました。終了して起動し直すと新しい版になります",
    "WirePlumber のスクリプトも変わったときは、ヘッドセットも再起動してね（~/.cache/frame-mic-tuner/update.log に出ます）",
    "リリースページ: ",
    // アプリと更新: ほかのアプリ（frame-apps の strings.md のとおり）
    "いっしょに使うアプリ", "このアプリの機能で使うものだけを出しています", "aux ボタンで、マイクのミュートを切り替える",
    "ささけん＠の Frame アプリ", "%1$d 個のうち %2$d 個が入っていません", "%d 個すべて入っています",
    "入れる", "動作中", "入っています", "Konsole で実行中…", "このアプリで使う",
    "入れるときは Konsole が開いて、インストーラーが動きます。sudo は使わず、入るのはホームフォルダの中だけです",
    "入れる・更新する・消すは、Konsole のインストーラーで行います",
    "一覧は「新しい版の確認」がオンのとき、frame.sasaken1102s.net から取ってきます（1 時間に 1 回まで。取れなければ前回の分か同梱の一覧）",
    "インストーラーを開く",
    "%s を入れる", "インストーラーを開く", "Konsole が開いて、このコマンドを実行します",
    "進み具合は Konsole に出ます（聞かれることがあれば、そこで答えます）",
    "メニューから、入れる・更新する・消すアプリを番号で選びます",
    "sudo は使いません。入るのはホームフォルダの中だけです",
    "終わったら Konsole を閉じてね。このパネルの表示も変わります",
    "やめる", "Konsole で開く",
    "同じアプリの Konsole がまだ開いています", "Konsole を開けませんでした: 画面が見つかりません（DISPLAY がありません）",
    "Konsole を開けませんでした: Konsole が入っていません", "Konsole を開けませんでした",
    // 一番下の行
    "SteamVR と一緒に起動", "終了", "もう一度押すと終了",
};

const UiText kEnglish = {
    // Header
    "Mic", "In use", "Not in use", "Muted", "Loading…", "Unknown", "Mute", "Unmute", "Apps & updates", "Mic settings",
    // Output and mic buttons
    "Output", "Mic", "%d", "Frame speakers", "Speaker", "Frame built-in mic", "Built-in", "%s mic",
    // Tabs
    "Quick", "Fine-tune",
    // Quick
    "When listening on %s",
    "Earphones / headphones", "Frame speakers",
    "Quiet sounds come through", "Keeps speaker sound out of the mic",
    "Echo cancel: off", "Echo cancel: on", "Noise filter: off",
    "%s always uses this setting. It switches automatically when you connect it",
    "Using the Fine-tune settings now",
    "%s connected, so its saved settings are on", "Undo",
    // Fine-tune
    "Echo cancel", "Removes speaker sound", "Noise filter", "Also cuts quiet mouth sounds", "On", "Off",
    "Strictness", "Lower lets more through", "Hold", "Sound kept after speech", "No effect while off", "Default",
    "Default: 23% · 500 ms (SteamOS)",
    // External mic
    "Using %s now",
    "Echo cancel and the noise filter only work on the Frame's built-in mic. This mic's sound goes to apps as it is.",
    "The settings for %1$s (%2$s) apply when you switch back to the built-in mic", "Use the built-in mic",
    // Settings summary
    "Echo cancel on", "Echo cancel off", ", noise filter on", " · ",
    // Signal path
    "Signal path", "Mic", "EQ", "Echo cancel", "Noise filter", "To apps", "Not in use, filters are resting",
    "Can't read the path",
    // Voice check
    "Voice check", "Record what apps hear, then compare", "Record", "Stop", "Recording", "s", "%.1f s · %.1f s left",
    "Record up to 10 s with the mic in use", "No recordings yet", "%s mic (no processing)", "Unknown setting",
    // Footer note
    "Changes apply at once and are remembered per output. Recordings stay in memory and vanish on quit",
    "Autostart is not installed (run ./install.sh)", "Can't read the autostart state",
    // Errors
    "Read failed (wpctl)", "Mic switch is not installed (run ./install.sh, then reboot)",
    "Path read failed (pw-link)", "Switch failed (wpctl)", "Autostart change failed (systemctl)",
    "Can't start recording (PipeWire)", "Can't play (PipeWire)", "Can't change the noise filter (pw-cli)",
    "Echo cancel switch failed (wpctl)", "Noise filter switch failed (wpctl)",
    "Couldn't unmute (wpctl)", "Couldn't mute (wpctl)",
    "Couldn't switch the output (wpctl)", "Couldn't switch the mic (wpctl)",
    // Choose the output
    "Choose the output", "Remembers the Frame mic settings for each output and switches when you connect it",
    "Connected", "Used before (not connected now)",
    "In use now", "Connected", "Last used %s", "Used before", "Showing settings",
    "Play sound here", "Forget",
    "New outputs start from the Speaker setting for the Frame speakers and from Earphones for anything else. "
    "\"Play sound here\" changes the same output as SteamVR's audio settings. Scroll a long list with the stick or by dragging",
    "Stop cut-off",
    "Stop cut-off: for adapters that turn their amp off during silence. It keeps playing an inaudible tone, so you may hear "
    "a faint hiss when it's quiet, and it uses a little battery",
    // Mic to use
    "Mic to use", "Echo cancel and the noise filter only work on the Frame's built-in mic",
    "Echo cancel and noise filter apply", "No processing (straight to apps)", "Use this mic",
    // Apps & updates: this app
    "This app", "Switches the mic echo cancellation and noise suppression",
    "Checks GitHub for a new version at start and once a day", "Doesn't check by itself. Use \"Check now\"",
    "Check for updates", "When off, nothing is fetched from GitHub or the app list (frame.sasaken1102s.net)",
    "How to use · report a problem",
    "Downloads and installs the new version. Quit and start the app again to use it",
    "%s is installed. Quit and start the app again to use it",
    "If the WirePlumber script changed too, restart the headset (see ~/.cache/frame-mic-tuner/update.log)",
    "Release page: ",
    // Apps & updates: other apps (as in frame-apps' strings.md)
    "Companion apps", "Only the apps this app uses", "Toggles the mic mute with the aux button",
    "Frame apps by sasaken@", "%2$d of %1$d not installed", "All %d are installed",
    "Install", "Running", "Installed", "Running in Konsole…", "Used by this app",
    "Installing opens the installer in Konsole. No sudo; everything goes into your home folder",
    "Install, update and remove in the installer in Konsole",
    "With update checks on, this list comes from frame.sasaken1102s.net (at most hourly; otherwise the last one or the built-in one)",
    "Open the installer",
    "Install %s", "Open the installer", "Konsole opens and runs this command",
    "Konsole shows the progress (answer any questions there)",
    "In the menu, pick the apps to install, update or remove by number",
    "No sudo; everything goes into your home folder",
    "Close Konsole when it's done; this panel updates too",
    "Cancel", "Open in Konsole",
    "A Konsole for this app is still open", "Couldn't open Konsole: no screen (DISPLAY isn't set)",
    "Couldn't open Konsole: Konsole isn't installed", "Couldn't open Konsole",
    // 一番下の行
    "Start with SteamVR", "Quit", "Press again to quit",
};

const UiText kSimplifiedChinese = {
    // 标题
    "麦克风", "使用中", "未使用", "静音中", "加载中…", "状态未知", "静音", "取消静音", "应用与更新", "麦克风设置",
    // 声音输出与麦克风
    "声音输出", "麦克风", "%d 个", "Frame 扬声器", "扬声器", "Frame 内置麦克风", "Frame 内置", "%s 的麦克风",
    // 标签页
    "简单设置", "精细调整",
    // 简单设置
    "使用 %s 收听时",
    "耳机 / 头戴式耳机", "Frame 扬声器",
    "细小声音也会被收音", "消除麦克风收到的 Frame 扬声器的声音",
    "回声消除 关", "回声消除 开", "噪声抑制 关",
    "使用 %s 时始终采用此设置。连接后会自动切换",
    "当前使用“精细调整”的设置",
    "已连接 %s，已切换为记住的设置", "撤销",
    // 精细调整
    "回声消除", "消除 Frame 扬声器的声音", "噪声抑制", "消除细小声音，输出会变沉闷", "开", "关",
    "麦克风灵敏度", "越低越容易触发收音", "收音保持时间", "说完后继续收音的时间", "关闭时不生效", "恢复默认",
    "默认 23% · 500ms（SteamOS 的值）",
    // 外接麦克风
    "当前使用 %s",
    "回声消除和噪声抑制只对 Frame 内置麦克风生效。这个麦克风的声音会原样传给应用。",
    "%1$s 的设置（%2$s）会在切回内置麦克风后使用", "改回 Frame 内置麦克风",
    // 设置摘要
    "回声消除开", "回声消除关", "・噪声抑制开", "・",
    // 信号链路
    "信号链路", "麦克风", "音质补正", "回声消除", "噪声抑制", "输出", "未在使用，处理已暂停", "无法读取信号链路",
    // 声音检查
    "声音检查", "录下麦克风收音，对比试听", "录音", "停止", "录音中", "秒", "%.1f 秒 · 剩余 %.1f 秒",
    "按“录音”最多录 10 秒，使用当前的麦克风", "还没有录音", "%s 麦克风（无处理）", "设置未知",
    // 底部说明
    "按下即刻生效，并按声音输出分别记住。录音只保存在内存中，退出后即消失",
    "自启动尚未安装（请运行 ./install.sh）", "无法读取自启动状态",
    // 错误
    "读取失败（wpctl）", "切换组件尚未安装（运行 ./install.sh 后重启）",
    "信号链路读取失败（pw-link）", "切换失败（wpctl）", "自启动切换失败（systemctl）",
    "无法开始录音（PipeWire）", "无法播放（PipeWire）", "无法修改噪声抑制强度（pw-cli）",
    "回声消除切换失败（wpctl）", "噪声抑制切换失败（wpctl）",
    "无法取消静音（wpctl）", "无法静音（wpctl）",
    "无法切换声音输出（wpctl）", "无法切换麦克风（wpctl）",
    // 选择声音输出
    "选择声音输出", "按声音输出分别记住 Frame 麦克风的设置，连接后自动切换",
    "已连接的输出", "以前用过的输出（当前未连接）",
    "正在使用", "已连接", "上次使用：%s", "以前用过", "正在显示设置",
    "从这里输出", "忘记",
    "新的输出：Frame 扬声器从“扬声器”的设置开始，其他从“耳机”的设置开始。"
    "“从这里输出”切换的是与 SteamVR 音频设置相同的输出。列表较长时可用摇杆或拖动来滚动",
    "防止开头被切",
    "防止开头被切：适用于无声时会关闭功放的转接器。会持续播放听不见的声音，安静时可能听到轻微的沙沙声，也会多耗一点电",
    // 使用的麦克风
    "使用的麦克风", "回声消除和噪声抑制只对 Frame 内置麦克风生效", "会进行回声消除和噪声抑制",
    "不做处理（直接传给应用）", "使用这个麦克风",
    // 应用与更新：本应用
    "本应用", "切换麦克风的回声消除与噪声抑制",
    "启动时以及每天一次，在 GitHub 上检查新版本", "不会自动检查。可用“立即检查”来检查",
    "检查新版本", "关闭后，不会访问 GitHub，也不会获取应用列表（frame.sasaken1102s.net）",
    "使用说明 · 问题反馈",
    "将下载并替换文件。完成后退出并重新启动，即可使用新版本",
    "已安装 %s。退出并重新启动，即可使用新版本",
    "如果 WirePlumber 脚本也有变化，请同时重启头显（见 ~/.cache/frame-mic-tuner/update.log）",
    "发布页面：",
    // 应用与更新：其他应用（按 frame-apps 的 strings.md 翻译）
    "配合使用的应用", "只显示本应用的功能会用到的应用", "用 aux 按钮切换麦克风静音",
    "sasaken@ 的 Frame 应用", "共 %1$d 个，其中 %2$d 个未安装", "%d 个全部已安装",
    "安装", "运行中", "已安装", "正在 Konsole 中运行…", "本应用会用到",
    "安装时会打开 Konsole 运行安装程序。不使用 sudo，只安装到主文件夹中",
    "安装、更新和删除都在 Konsole 的安装程序中进行",
    "“检查新版本”打开时，会从 frame.sasaken1102s.net 获取此列表（最多每小时一次；获取不到时使用上次的列表或内置列表）",
    "打开安装程序",
    "安装 %s", "打开安装程序", "将打开 Konsole 并运行以下命令",
    "安装进度会显示在 Konsole 中（如有提问，请在那里回答）",
    "在菜单中用编号选择要安装、更新或删除的应用",
    "不使用 sudo，只安装到主文件夹中",
    "完成后请关闭 Konsole，此面板的显示也会随之更新",
    "取消", "在 Konsole 中打开",
    "这个应用的 Konsole 还开着", "无法打开 Konsole：找不到屏幕（没有 DISPLAY）",
    "无法打开 Konsole：没有安装 Konsole", "无法打开 Konsole",
    // 一番下の行
    "随 SteamVR 启动", "退出", "再按一次退出",
};

const UiText kGerman = {
    // Kopf
    "Mikrofon", "In Gebrauch", "Ungenutzt", "Stumm", "Laden…", "Unbekannt", "Stummschalten", "Stummschaltung aus",
    "Apps & Updates", "Mikrofoneinstellungen",
    // Ausgang und Mikrofon
    "Ausgang", "Mikrofon", "%d", "Frame-Lautsprecher", "Lautsprecher", "Frame-Mikrofon", "Eingebaut", "Mikrofon von %s",
    // Tabs
    "Schnell", "Feinjustierung",
    // Schnell
    "Beim Hören über %s",
    "Ohrhörer / Kopfhörer", "Frame-Lautsprecher",
    "Leise Töne kommen durch", "Hält Lautsprecherklang vom Mikrofon fern",
    "Echofilter aus", "Echofilter an", "Rauschfilter aus",
    "%s benutzt immer diese Einstellung. Sie wechselt automatisch, wenn du es anschließt",
    "Gerade gelten die Einstellungen der Feinjustierung",
    "%s angeschlossen, also gilt die gespeicherte Einstellung", "Rückgängig",
    // Feinjustierung
    "Echofilter", "Entfernt Lautsprecherklang", "Rauschfilter", "Schneidet auch leise Mundgeräusche", "Ein", "Aus",
    "Strenge", "Tiefer lässt mehr durch", "Nachklang", "Ton nach dem Sprechen", "Wirkt nicht, solange aus", "Standard",
    "Standard: 23 % · 500 ms (SteamOS)",
    // Externes Mikrofon
    "Gerade wird %s benutzt",
    "Echofilter und Rauschfilter gelten nur für das eingebaute Frame-Mikrofon. Der Ton dieses Mikrofons geht unverändert an Apps.",
    "Die Einstellung für %1$s (%2$s) gilt, wenn du zum eingebauten Mikrofon zurückwechselst", "Eingebautes Mikrofon benutzen",
    // Zusammenfassung
    "Echofilter an", "Echofilter aus", ", Rauschfilter an", " · ",
    // Signalweg
    "Signalweg", "Mikrofon", "EQ", "Echofilter", "Rauschfilter", "Zu Apps", "Nicht in Gebrauch, Filter ruhen",
    "Signalweg nicht lesbar",
    // Stimmtest
    "Stimmtest", "Nimm auf, was Apps hören, und vergleiche", "Aufnehmen", "Stopp", "Aufnahme", "s", "%.1f s · noch %.1f s",
    "Bis zu 10 s mit dem aktiven Mikrofon aufnehmen", "Noch keine Aufnahmen", "Mikrofon %s (ohne Verarbeitung)", "Unbekannte Einstellung",
    // Fußnote
    "Änderungen gelten sofort und werden je Ausgang gemerkt. Aufnahmen bleiben im Speicher und verschwinden beim Beenden",
    "Autostart ist nicht installiert (./install.sh ausführen)", "Autostart-Status nicht lesbar",
    // Fehler
    "Lesen fehlgeschlagen (wpctl)", "Mikrofon-Umschaltung ist nicht installiert (./install.sh, dann neu starten)",
    "Signalweg nicht lesbar (pw-link)", "Umschalten fehlgeschlagen (wpctl)", "Autostart-Änderung fehlgeschlagen (systemctl)",
    "Aufnahme startet nicht (PipeWire)", "Wiedergabe nicht möglich (PipeWire)", "Rauschfilter nicht änderbar (pw-cli)",
    "Echofilter-Umschaltung fehlgeschlagen (wpctl)", "Rauschfilter-Umschaltung fehlgeschlagen (wpctl)",
    "Stummschaltung nicht aufgehoben (wpctl)", "Stummschalten fehlgeschlagen (wpctl)",
    "Ausgang nicht gewechselt (wpctl)", "Mikrofon nicht gewechselt (wpctl)",
    // Ausgang wählen
    "Ausgang wählen", "Merkt die Frame-Mikrofoneinstellung je Ausgang und wechselt beim Anschließen",
    "Verbunden", "Früher benutzt (jetzt nicht verbunden)",
    "Gerade aktiv", "Verbunden", "Zuletzt benutzt %s", "Früher benutzt", "Einstellungen angezeigt",
    "Hier wiedergeben", "Vergessen",
    "Neue Ausgänge starten bei den Frame-Lautsprechern mit der Lautsprecher-Einstellung und sonst mit Ohrhörer. "
    "\"Hier wiedergeben\" wechselt denselben Ausgang wie die Audioeinstellung von SteamVR. Lange Listen mit dem Stick oder durch Ziehen scrollen",
    "Tonabriss verhindern",
    "Tonabriss verhindern: für Adapter, die in Stille ihren Verstärker abschalten. Spielt ständig einen unhörbaren Ton – "
    "in ruhigen Momenten ist evtl. ein leises Rauschen zu hören, und es braucht etwas Akku",
    // Mikrofon
    "Mikrofon wählen", "Echofilter und Rauschfilter gelten nur für das eingebaute Frame-Mikrofon",
    "Echofilter und Rauschfilter sind aktiv", "Keine Verarbeitung (direkt zu Apps)", "Mikrofon nutzen",
    // Apps
    "Diese App", "Schaltet Echofilter und Rauschfilter des Mikrofons um",
    "Prüft beim Start und einmal am Tag auf GitHub nach einer neuen Version", "Prüft nicht von selbst. „Jetzt prüfen“ benutzen",
    "Nach Updates suchen", "Wenn aus, wird nichts von GitHub oder der App-Liste (frame.sasaken1102s.net) geholt",
    "Anleitung · Problem melden",
    "Lädt die neue Version und installiert sie. Beenden und neu starten, um sie zu nutzen",
    "%s ist installiert. Beenden und neu starten, um sie zu nutzen",
    "Wenn sich auch das WirePlumber-Skript geändert hat, das Headset neu starten (siehe ~/.cache/frame-mic-tuner/update.log)",
    "Release-Seite: ",
    "Begleit-Apps", "Nur die Apps, die diese App benutzt", "Stummschaltung per Aux-Taste umschalten",
    "Frame-Apps von sasaken@", "%2$d von %1$d nicht installiert", "Alle %d sind installiert",
    "Installieren", "Läuft", "Installiert", "Läuft in Konsole…", "Von dieser App benutzt",
    "Beim Installieren öffnet sich das Installationsprogramm in Konsole. Kein sudo; alles landet im Home-Ordner",
    "Installieren, aktualisieren und entfernen im Installationsprogramm in Konsole",
    "Bei eingeschalteter Update-Prüfung kommt diese Liste von frame.sasaken1102s.net (höchstens stündlich; sonst die letzte oder die eingebaute)",
    "Installationsprogramm öffnen",
    "%s installieren", "Installationsprogramm öffnen", "Konsole öffnet sich und führt diesen Befehl aus",
    "Konsole zeigt den Fortschritt (Fragen dort beantworten)",
    "Im Menü die Apps zum Installieren, Aktualisieren oder Entfernen per Nummer wählen",
    "Kein sudo; alles landet im Home-Ordner",
    "Konsole schließen, wenn es fertig ist; dieses Fenster aktualisiert sich auch",
    "Abbrechen", "In Konsole öffnen",
    "Eine Konsole für diese App ist noch offen", "Konsole ließ sich nicht öffnen: kein Bildschirm (DISPLAY fehlt)",
    "Konsole ließ sich nicht öffnen: Konsole ist nicht installiert", "Konsole ließ sich nicht öffnen",
    // 一番下の行
    "Mit SteamVR starten", "Beenden", "Nochmal drücken zum Beenden",
};

const UiText kFrench = {
    // En-tête
    "Micro", "En cours", "Inutilisé", "Muet", "Chargement…", "Inconnu", "Couper le son", "Rétablir le son",
    "Apps et mises à jour", "Réglages du micro",
    // Sortie et micro
    "Sortie", "Micro", "%d", "Haut-parleurs Frame", "Haut-parleur", "Micro intégré Frame", "Intégré", "Micro %s",
    // Onglets
    "Rapide", "Réglage fin",
    // Rapide
    "Quand l'écoute passe par %s",
    "Écouteurs / casque", "Haut-parleurs Frame",
    "Les sons faibles passent", "Garde le son des haut-parleurs hors du micro",
    "Annulation d'écho : non", "Annulation d'écho : oui", "Filtre de bruit : non",
    "%s utilise toujours ce réglage. Il change tout seul quand tu le branches",
    "Les réglages fins sont utilisés pour l'instant",
    "%s est branché, donc son réglage enregistré est actif", "Annuler",
    // Réglage fin
    "Annulation d'écho", "Retire le son des haut-parleurs", "Filtre de bruit", "Coupe aussi les sons de bouche faibles", "Oui", "Non",
    "Sévérité", "Plus bas, plus ça passe", "Tenue", "Son gardé après la voix", "Sans effet si désactivé", "Défaut",
    "Défaut : 23 % · 500 ms (SteamOS)",
    // Micro externe
    "Utilisation de %s",
    "L'annulation d'écho et le filtre de bruit ne valent que pour le micro intégré du Frame. Le son de ce micro va aux apps tel quel.",
    "Le réglage pour %1$s (%2$s) s'applique quand tu reviens au micro intégré", "Utiliser le micro intégré",
    // Résumé
    "Annulation d'écho activée", "Annulation d'écho désactivée", ", filtre de bruit activé", " · ",
    // Chemin
    "Chemin du signal", "Micro", "EQ", "Annulation d'écho", "Filtre de bruit", "Vers les apps", "Inutilisé, les filtres se reposent",
    "Chemin illisible",
    // Voix
    "Contrôle de voix", "Enregistre ce que les apps entendent, puis compare", "Enregistrer", "Stop", "Enregistrement", "s",
    "%.1f s · %.1f s restantes",
    "Enregistre jusqu'à 10 s avec le micro en cours", "Pas encore d'enregistrement", "Micro %s (sans traitement)", "Réglage inconnu",
    // Note
    "Les changements s'appliquent tout de suite et sont mémorisés par sortie. Les enregistrements restent en mémoire et disparaissent à la fermeture",
    "Le démarrage auto n'est pas installé (lance ./install.sh)", "État du démarrage auto illisible",
    // Erreurs
    "Lecture échouée (wpctl)", "Le basculement du micro n'est pas installé (lance ./install.sh, puis redémarre)",
    "Lecture du chemin échouée (pw-link)", "Basculement échoué (wpctl)", "Changement du démarrage auto échoué (systemctl)",
    "Enregistrement impossible (PipeWire)", "Lecture impossible (PipeWire)", "Filtre de bruit non modifiable (pw-cli)",
    "Basculement de l'écho échoué (wpctl)", "Basculement du filtre de bruit échoué (wpctl)",
    "Impossible de rétablir le son (wpctl)", "Impossible de couper le son (wpctl)",
    "Impossible de changer la sortie (wpctl)", "Impossible de changer le micro (wpctl)",
    // Sortie
    "Choisir la sortie", "Mémorise les réglages du micro Frame pour chaque sortie et bascule au branchement",
    "Connectées", "Déjà utilisées (pas connectées)",
    "En cours", "Connectée", "Dernière utilisation %s", "Déjà utilisée", "Réglages affichés",
    "Jouer le son ici", "Oublier",
    "Les nouvelles sorties partent du réglage Haut-parleur pour les haut-parleurs Frame, et du réglage Écouteurs pour le reste. "
    "« Jouer le son ici » change la même sortie que les réglages audio de SteamVR. Fais défiler une longue liste au stick ou en glissant",
    "Éviter les coupures",
    "Éviter les coupures : pour les adaptateurs qui coupent leur ampli pendant les silences. Un son inaudible est joué en continu : "
    "un léger souffle peut s'entendre au calme, et cela utilise un peu la batterie",
    // Micro
    "Micro à utiliser", "L'annulation d'écho et le filtre de bruit ne valent que pour le micro intégré du Frame",
    "Annulation d'écho et filtre de bruit actifs", "Aucun traitement (direct vers les apps)", "Utiliser ce micro",
    // Apps
    "Cette app", "Bascule l'annulation d'écho et le filtre de bruit du micro",
    "Vérifie GitHub au démarrage et une fois par jour", "Ne vérifie pas tout seul. Utilise « Vérifier »",
    "Vérifier les mises à jour", "Si c'est désactivé, rien n'est pris sur GitHub ni dans la liste (frame.sasaken1102s.net)",
    "Mode d'emploi · signaler un problème",
    "Télécharge et installe la nouvelle version. Quitte et relance l'app pour l'utiliser",
    "%s est installé. Quitte et relance l'app pour l'utiliser",
    "Si le script WirePlumber a aussi changé, redémarre le casque (voir ~/.cache/frame-mic-tuner/update.log)",
    "Page de version : ",
    "Apps associées", "Seulement les apps que celle-ci utilise", "Sourdine du micro au bouton aux",
    "Apps Frame de sasaken@", "%2$d sur %1$d non installées", "Les %d sont installées",
    "Installer", "En cours", "Installée", "En cours dans Konsole…", "Utilisée par cette app",
    "L'installation ouvre l'installateur dans Konsole. Pas de sudo ; tout va dans le dossier personnel",
    "Installer, mettre à jour et retirer dans l'installateur de Konsole",
    "Avec la vérification activée, cette liste vient de frame.sasaken1102s.net (au plus une fois par heure ; sinon la dernière ou celle intégrée)",
    "Ouvrir l'installateur",
    "Installer %s", "Ouvrir l'installateur", "Konsole s'ouvre et lance cette commande",
    "Konsole montre la progression (réponds aux questions là)",
    "Dans le menu, choisis par numéro les apps à installer, mettre à jour ou retirer",
    "Pas de sudo ; tout va dans le dossier personnel",
    "Ferme Konsole quand c'est fini ; ce panneau se met à jour aussi",
    "Annuler", "Ouvrir dans Konsole",
    "Une Konsole pour cette app est encore ouverte", "Impossible d'ouvrir Konsole : pas d'écran (DISPLAY absent)",
    "Impossible d'ouvrir Konsole : Konsole n'est pas installé", "Impossible d'ouvrir Konsole",
    // 一番下の行
    "Démarrer avec SteamVR", "Quitter", "Appuyer encore pour quitter",
};

const UiText kSpanish = {
    // Encabezado
    "Micrófono", "En uso", "Sin uso", "Silenciado", "Cargando…", "Desconocido", "Silenciar", "Quitar silencio",
    "Apps y actualizaciones", "Ajustes del micrófono",
    // Salida y micrófono
    "Salida", "Micrófono", "%d", "Altavoces de Frame", "Altavoz", "Micrófono de Frame", "Integrado", "Micrófono de %s",
    // Pestañas
    "Rápido", "Ajuste fino",
    // Rápido
    "Al escuchar por %s",
    "Auriculares", "Altavoces de Frame",
    "Los sonidos bajos se oyen", "Evita que el altavoz entre en el micrófono",
    "Cancelación de eco: no", "Cancelación de eco: sí", "Filtro de ruido: no",
    "%s usa siempre este ajuste. Cambia solo al conectarlo",
    "Ahora se usan los ajustes finos",
    "%s conectado, así que vale su ajuste guardado", "Deshacer",
    // Ajuste fino
    // オン・オフは幅 180 の切り替え（新しい版の確認・SteamVR と一緒に起動）にも入るので短く（札の「: sí / no」と同じ）
    "Cancelación de eco", "Quita el sonido del altavoz", "Filtro de ruido", "También corta sonidos bajos de la boca", "Sí", "No",
    "Sensibilidad", "Más bajo deja pasar más", "Retención", "Sonido tras hablar", "No actúa si está apagado", "Predeterminado",
    "Predeterminado: 23 % · 500 ms (SteamOS)",
    // Micrófono externo
    "Ahora se usa %s",
    "La cancelación de eco y el filtro de ruido solo actúan en el micrófono integrado de Frame. El sonido de este micrófono llega a las apps tal cual.",
    "El ajuste de %1$s (%2$s) se aplica al volver al micrófono integrado", "Usar el micrófono integrado",
    // Resumen
    "Eco activado", "Eco apagado", ", filtro de ruido activado", " · ",
    // Ruta
    "Ruta de la señal", "Micrófono", "EQ", "Cancelación de eco", "Filtro de ruido", "A las apps", "Sin uso, los filtros descansan",
    "No se puede leer la ruta",
    // Voz
    "Prueba de voz", "Graba lo que oyen las apps y compáralo", "Grabar", "Parar", "Grabando", "s", "%.1f s · quedan %.1f s",
    "Graba hasta 10 s con el micrófono en uso", "Aún no hay grabaciones", "Micrófono %s (sin procesar)", "Ajuste desconocido",
    // Nota
    "Los cambios se aplican al momento y se recuerdan por salida. Las grabaciones se quedan en memoria y desaparecen al salir",
    "El inicio automático no está instalado (ejecuta ./install.sh)", "No se puede leer el inicio automático",
    // Errores
    "Error de lectura (wpctl)", "El cambio de micrófono no está instalado (ejecuta ./install.sh y reinicia)",
    "Error al leer la ruta (pw-link)", "El cambio falló (wpctl)", "El cambio del inicio automático falló (systemctl)",
    "No se puede grabar (PipeWire)", "No se puede reproducir (PipeWire)", "No se puede cambiar el filtro de ruido (pw-cli)",
    "Falló el cambio de eco (wpctl)", "Falló el cambio del filtro de ruido (wpctl)",
    "No se pudo quitar el silencio (wpctl)", "No se pudo silenciar (wpctl)",
    "No se pudo cambiar la salida (wpctl)", "No se pudo cambiar el micrófono (wpctl)",
    // Salida
    "Elegir la salida", "Recuerda los ajustes del micrófono de Frame para cada salida y cambia al conectarla",
    "Conectadas", "Usadas antes (ahora no conectadas)",
    "En uso", "Conectada", "Último uso %s", "Usada antes", "Mostrando ajustes",
    "Reproducir aquí", "Olvidar",
    "Las salidas nuevas empiezan con el ajuste de altavoz en los altavoces de Frame y con auriculares en lo demás. "
    "\"Reproducir aquí\" cambia la misma salida que el audio de SteamVR. Desplaza una lista larga con el stick o arrastrando",
    "Evitar cortes",
    "Evitar cortes: para adaptadores que apagan su amplificador en silencio. Reproduce un tono inaudible continuo, así que en "
    "silencio puede oírse un leve siseo, y gasta algo de batería",
    // Micrófono
    "Micrófono a usar", "La cancelación de eco y el filtro de ruido solo actúan en el micrófono integrado de Frame",
    "Se aplican eco y filtro de ruido", "Sin procesar (directo a las apps)", "Usar este micrófono",
    // Apps
    "Esta app", "Cambia la cancelación de eco y el filtro de ruido del micrófono",
    "Comprueba GitHub al iniciar y una vez al día", "No comprueba sola. Usa \"Comprobar ahora\"",
    "Buscar actualizaciones", "Si está apagado, no se pide nada a GitHub ni a la lista (frame.sasaken1102s.net)",
    "Cómo se usa · informar de un problema",
    "Descarga e instala la versión nueva. Sal y vuelve a abrir la app para usarla",
    "%s está instalada. Sal y vuelve a abrir la app para usarla",
    "Si el script de WirePlumber también cambió, reinicia el visor (mira ~/.cache/frame-mic-tuner/update.log)",
    "Página de la versión: ",
    "Apps de apoyo", "Solo las apps que usa esta", "Cambia el silencio con el botón aux",
    "Apps de Frame de sasaken@", "%2$d de %1$d sin instalar", "Las %d están instaladas",
    "Instalar", "En marcha", "Instalada", "En marcha en Konsole…", "La usa esta app",
    "Al instalar se abre el instalador en Konsole. Sin sudo; todo va a la carpeta personal",
    "Instalar, actualizar y quitar en el instalador de Konsole",
    "Con la búsqueda activada, esta lista viene de frame.sasaken1102s.net (como mucho cada hora; si no, la última o la incluida)",
    "Abrir el instalador",
    "Instalar %s", "Abrir el instalador", "Konsole se abre y ejecuta este comando",
    "Konsole muestra el avance (responde allí a las preguntas)",
    "En el menú, elige por número las apps a instalar, actualizar o quitar",
    "Sin sudo; todo va a la carpeta personal",
    "Cierra Konsole al terminar; este panel también se actualiza",
    "Cancelar", "Abrir en Konsole",
    "Todavía hay una Konsole de esta app abierta", "No se pudo abrir Konsole: no hay pantalla (falta DISPLAY)",
    "No se pudo abrir Konsole: Konsole no está instalado", "No se pudo abrir Konsole",
    // 一番下の行
    "Iniciar con SteamVR", "Salir", "Pulsar otra vez para salir",
};

const UiText kItalian = {
    // Intestazione
    "Microfono", "In uso", "Non in uso", "Silenziato", "Caricamento…", "Sconosciuto", "Silenzia", "Riattiva",
    "App e aggiornamenti", "Impostazioni microfono",
    // Uscita e microfono
    "Uscita", "Microfono", "%d", "Altoparlanti Frame", "Altoparlante", "Microfono integrato Frame", "Integrato", "Microfono %s",
    // Schede
    "Rapido", "Regolazione fine",
    // Rapido
    "Quando ascolti da %s",
    "Auricolari / cuffie", "Altoparlanti Frame",
    "Passano anche i suoni bassi", "Tiene il suono degli altoparlanti fuori dal microfono",
    "Cancellazione eco: no", "Cancellazione eco: sì", "Filtro rumore: no",
    "%s usa sempre questa impostazione. Cambia da solo quando lo colleghi",
    "Ora valgono le impostazioni della regolazione fine",
    "%s collegato, quindi vale l'impostazione salvata", "Annulla",
    // Regolazione fine
    // オン・オフは幅 180 の切り替え（新しい版の確認・SteamVR と一緒に起動）にも入るので短く（札の「: sì / no」と同じ）
    "Cancellazione eco", "Toglie il suono degli altoparlanti", "Filtro rumore", "Taglia anche i suoni bassi della bocca", "Sì", "No",
    "Severità", "Più basso, più passa", "Tenuta", "Coda dopo il parlato", "Non ha effetto se spento", "Predefinito",
    "Predefinito: 23% · 500 ms (SteamOS)",
    // Microfono esterno
    "Ora si usa %s",
    "La cancellazione dell'eco e il filtro rumore valgono solo per il microfono integrato di Frame. Il suono di questo microfono arriva alle app così com'è.",
    "L'impostazione per %1$s (%2$s) vale quando torni al microfono integrato", "Usa il microfono integrato",
    // Riepilogo
    "Eco attivo", "Eco spento", ", filtro rumore attivo", " · ",
    // Percorso
    "Percorso del segnale", "Microfono", "EQ", "Cancellazione eco", "Filtro rumore", "Alle app", "Non in uso, i filtri riposano",
    "Percorso illeggibile",
    // Voce
    "Controllo voce", "Registra ciò che sentono le app e confronta", "Registra", "Stop", "Registrazione", "s", "%.1f s · %.1f s rimasti",
    "Registra fino a 10 s con il microfono in uso", "Ancora nessuna registrazione", "Microfono %s (senza elaborazione)", "Impostazione sconosciuta",
    // Nota
    "Le modifiche valgono subito e si ricordano per uscita. Le registrazioni restano in memoria e spariscono all'uscita",
    "L'avvio automatico non è installato (esegui ./install.sh)", "Stato dell'avvio automatico illeggibile",
    // Errori
    "Lettura non riuscita (wpctl)", "Il cambio microfono non è installato (esegui ./install.sh, poi riavvia)",
    "Lettura del percorso non riuscita (pw-link)", "Cambio non riuscito (wpctl)", "Cambio dell'avvio automatico non riuscito (systemctl)",
    "Impossibile registrare (PipeWire)", "Impossibile riprodurre (PipeWire)", "Impossibile cambiare il filtro rumore (pw-cli)",
    "Cambio eco non riuscito (wpctl)", "Cambio del filtro rumore non riuscito (wpctl)",
    "Impossibile riattivare (wpctl)", "Impossibile silenziare (wpctl)",
    "Impossibile cambiare l'uscita (wpctl)", "Impossibile cambiare il microfono (wpctl)",
    // Uscita
    "Scegli l'uscita", "Ricorda le impostazioni del microfono Frame per ogni uscita e cambia al collegamento",
    "Collegate", "Usate prima (ora non collegate)",
    "In uso ora", "Collegata", "Ultimo uso %s", "Usata prima", "Impostazioni in vista",
    "Riproduci qui", "Dimentica",
    "Le uscite nuove partono dall'impostazione Altoparlante per gli altoparlanti Frame e da Auricolari per il resto. "
    "\"Riproduci qui\" cambia la stessa uscita delle impostazioni audio di SteamVR. Scorri un elenco lungo con lo stick o trascinando",
    "Evita tagli",
    "Evita tagli: per adattatori che spengono l'amplificatore nel silenzio. Riproduce di continuo un tono non udibile, quindi "
    "nel silenzio si può sentire un leggero fruscio, e usa un po' di batteria",
    // Microfono
    "Microfono da usare", "La cancellazione dell'eco e il filtro rumore valgono solo per il microfono integrato di Frame",
    "Si applicano eco e filtro rumore", "Nessuna elaborazione (dritto alle app)", "Usa questo microfono",
    // App
    "Questa app", "Cambia la cancellazione dell'eco e il filtro rumore del microfono",
    "Controlla GitHub all'avvio e una volta al giorno", "Non controlla da sola. Usa \"Controlla ora\"",
    "Cerca aggiornamenti", "Se spento, non si scarica nulla da GitHub né dall'elenco (frame.sasaken1102s.net)",
    "Come si usa · segnala un problema",
    "Scarica e installa la nuova versione. Esci e riavvia l'app per usarla",
    "%s è installata. Esci e riavvia l'app per usarla",
    "Se è cambiato anche lo script di WirePlumber, riavvia il visore (vedi ~/.cache/frame-mic-tuner/update.log)",
    "Pagina della versione: ",
    "App di supporto", "Solo le app che questa usa", "Cambia il muto con il tasto aux",
    "App Frame di sasaken@", "%2$d di %1$d non installate", "Tutte le %d sono installate",
    "Installa", "In esecuzione", "Installata", "In corso in Konsole…", "Usata da questa app",
    "L'installazione apre il programma in Konsole. Niente sudo; tutto va nella cartella home",
    "Installa, aggiorna e rimuovi nel programma in Konsole",
    "Con il controllo attivo, questo elenco arriva da frame.sasaken1102s.net (al massimo ogni ora; altrimenti l'ultimo o quello incluso)",
    "Apri il programma",
    "Installa %s", "Apri il programma di installazione", "Konsole si apre ed esegue questo comando",
    "Konsole mostra l'avanzamento (rispondi lì alle domande)",
    "Nel menu scegli per numero le app da installare, aggiornare o rimuovere",
    "Niente sudo; tutto va nella cartella home",
    "Chiudi Konsole quando ha finito; si aggiorna anche questo pannello",
    "Annulla", "Apri in Konsole",
    "Una Konsole per questa app è ancora aperta", "Impossibile aprire Konsole: nessuno schermo (DISPLAY assente)",
    "Impossibile aprire Konsole: Konsole non è installato", "Impossibile aprire Konsole",
    // 一番下の行
    "Avvia con SteamVR", "Esci", "Premi di nuovo per uscire",
};


}  // namespace

const UiText& uiText(Language language) {
    switch (language) {
        case Language::En: return kEnglish;
        case Language::Sc: return kSimplifiedChinese;
        case Language::De: return kGerman;
        case Language::Fr: return kFrench;
        case Language::Es: return kSpanish;
        case Language::It: return kItalian;
        case Language::Ja: return kJapanese;
    }
    return kJapanese;
}

Language systemLanguage() {
    static const Language cached = detectSystemLanguage();
    return cached;
}

const char* languageCode(Language language) {
    switch (language) {
        case Language::En: return "en";
        case Language::Sc: return "sc";
        case Language::De: return "de";
        case Language::Fr: return "fr";
        case Language::Es: return "es";
        case Language::It: return "it";
        case Language::Ja: return "ja";
    }
    return "ja";
}

bool parseLanguage(const std::string& code, Language& language) {
    if (code == "ja") {
        language = Language::Ja;
        return true;
    }
    if (code == "en") {
        language = Language::En;
        return true;
    }
    if (code == "sc") {
        language = Language::Sc;
        return true;
    }
    if (code == "de") {
        language = Language::De;
        return true;
    }
    if (code == "fr") {
        language = Language::Fr;
        return true;
    }
    if (code == "es") {
        language = Language::Es;
        return true;
    }
    if (code == "it") {
        language = Language::It;
        return true;
    }
    return false;
}

const char* languageEndonym(Language language) {
    switch (language) {
        case Language::Ja: return "日本語";
        case Language::En: return "English";
        case Language::Sc: return "简体中文";
        case Language::De: return "Deutsch";
        case Language::Fr: return "Français";
        case Language::Es: return "Español";
        case Language::It: return "Italiano";
    }
    return "English";
}
