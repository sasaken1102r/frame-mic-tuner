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
        return Language::En;
    }
    if (localeEnvVarsAreSetTo("ja")) return Language::Ja;
    if (localeEnvVarsAreSetTo("zh_CN")) return Language::Sc;
    return Language::En;
}

// 3 つの表は UiText のフィールドの順（i18n.h の区切りごと）に並べる
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
    "ささけんの Frame アプリ", "%1$d 個のうち %2$d 個が入っていません", "%d 個すべて入っています",
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
};

}  // namespace

const UiText& uiText(Language language) {
    if (language == Language::En) return kEnglish;
    if (language == Language::Sc) return kSimplifiedChinese;
    return kJapanese;
}

Language systemLanguage() {
    static const Language cached = detectSystemLanguage();
    return cached;
}

const char* languageCode(Language language) {
    if (language == Language::En) return "en";
    if (language == Language::Sc) return "sc";
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
    return false;
}
