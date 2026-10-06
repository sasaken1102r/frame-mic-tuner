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
 * ロケールの環境変数（LC_ALL → LC_MESSAGES → LANG の順で最初に空でないもの）が日本語か。
 * @return 日本語なら true
 */
bool localeIsJapanese() {
    for (const char* name : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
        const char* value = std::getenv(name);
        if (value != nullptr && value[0] != '\0') return std::string(value).rfind("ja", 0) == 0;
    }
    return false;
}

/**
 * システム言語を調べる（systemLanguage の本体）。
 * @return 言語
 */
Language detectSystemLanguage() {
    const std::string steam = steamLanguage();
    if (!steam.empty()) return steam == "japanese" ? Language::Ja : Language::En;
    return localeIsJapanese() ? Language::Ja : Language::En;
}

const UiText kJapanese = {
    "マイク", "使用中", "未使用", "読み込み中…",
    "イヤホン", "スピーカー",
    "小さな音まで届く", "スピーカーの音を消す",
    "エコー除去", "スピーカーの音を消す",
    "ノイズ除去", "口の音など小さい音も消える",
    "オン", "オフ",
    "つながり", "マイク", "音質補正", "エコー除去", "ノイズ除去", "アプリへ",
    "使っていないので処理はお休み中", "つながりを読めません",
    "声のチェック", "アプリに届く音を録って聞き比べ",
    "録音", "停止", "録音中", "秒", "%.1f 秒・残り %.1f 秒",
    "「録音」を押すと、最大 10 秒録ります", "まだ録音はありません",
    "＋ノイズ除去", "設定不明",
    "言語", "SteamVR と一緒に起動",
    "自動起動は準備されていません（./install.sh を実行してください）", "自動起動の状態を読めません",
    "終了", "もう一度押すと終了",
    "押すとすぐ切り替わり、再起動後も残ります。録音はメモリの中だけで、終了すると消えます",
    "読み取りに失敗（wpctl）", "切り替えの仕組みが入っていません（./install.sh のあと再起動）",
    "つながりの読み取りに失敗（pw-link）", "切り替えに失敗（wpctl）", "自動起動の切り替えに失敗（systemctl）",
    "録音を始められません（PipeWire）", "再生できません（PipeWire）",
    "判定の厳しさ", "低いほど声以外の音も通る", "余韻", "声のあと音を通す時間",
    "オフの間は効きません", "標準に戻す",
    "ノイズ除去の強さを変えられません（pw-cli）",
    "どこで音を聞いてる？", "選ぶとおすすめの設定になります",
    "イヤホン・ヘッドホン", "Frame のスピーカー",
    "エコー除去 オフ", "エコー除去 オン",
    "細かく調整",
    "今は細かく調整した設定です", "ノイズ除去 オフ",
    "エコー除去の切り替えに失敗（wpctl）", "ノイズ除去の切り替えに失敗（wpctl）",
    "かんたん", "細かく調整を見る →", "標準は 23%・500ms（SteamOS の値）",

    // 更新（vendor/frame-updater/strings.md のまま。ただし確認の補足と入れ終わりの文は、install.sh が常駐を
    // 再起動しないこのアプリの動きに合わせて変えている。入れ終わりの補足はこのアプリだけのもの）
    "新しい版の確認", "起動時と 1 日 1 回、GitHub に新しい版がないか見に行きます",
    "最新版です（%s）", "新しい版を確かめています…", "新しい版 %s があります",
    "更新する", "ここからは入れられない版です。GitHub から手で更新してね", "リリースページ: ",
    "%s に更新しますか？", "ダウンロードして入れ替えます。終わったら、終了して起動し直すと新しい版になります",
    "更新する", "やめる",
    "更新中: %s", "%s を入れました。終了して起動し直すと新しい版になります",
    "更新できませんでした（今の版のままです）:", "新しい版を確かめられませんでした:",
    "今すぐ確かめる", "もう一度", "閉じる", "くわしくは ~/.cache/<アプリ>/update.log",
    "WirePlumber のスクリプトも変わったときは、ヘッドセットも再起動してね（~/.cache/frame-mic-tuner/update.log に出ます）",
    "準備中", "ダウンロード中", "ファイルを確認中", "展開中", "入れ替え中",
    "GitHub につながりません", "GitHub の回数制限にかかりました。1 時間ほどあとで試してね",
    "公開されている版がありません", "GitHub の返事を読めませんでした", "版の番号を読めませんでした",
    "GitHub 以外の場所へ向かったので止めました", "必要なコマンド（python3）がありません",
    "この版には確認用の SHA256SUMS がありません。手で更新してね", "この版には入れるファイルがありません",
    "ダウンロードしたファイルが壊れています", "ファイルの中身が安全でないので止めました",
    "ファイルに install.sh がありません", "install.sh が失敗しました",
    "前回のインストールのオプションを読めません", "別の更新が動いています", "もう最新版です",
    "更新を始められませんでした（systemd-run）", "更新が途中で止まりました", "ファイルを書けませんでした",
    "うまくいきませんでした",

    // ミュート
    "ミュート中", "マイクがミュートされています", "ミュートを解除", "ミュートを解除できませんでした（wpctl）",
};

const UiText kEnglish = {
    "Mic", "In use", "Not in use", "Loading…",
    "Earphones", "Speaker",
    "Quiet sounds come through", "Keeps speaker sound out of the mic",
    "Echo cancel", "Removes speaker sound",
    "Noise filter", "Also cuts quiet mouth sounds",
    "On", "Off",
    "Signal path", "Mic", "EQ", "Echo cancel", "Noise filter", "To apps",
    "Not in use, filters are resting", "Can't read the path",
    "Voice check", "Record what apps hear, then compare",
    "Record", "Stop", "Recording", "s", "%.1f s · %.1f s left",
    "Press Record to capture up to 10 s", "No recordings yet",
    " + noise filter", "Unknown setting",
    "Language", "Start with SteamVR",
    "Autostart is not installed (run ./install.sh)", "Can't read the autostart state",
    "Quit", "Press again to quit",
    "Applies instantly and survives restarts. Recordings stay in memory and vanish on quit",
    "Read failed (wpctl)", "Mic switch is not installed (run ./install.sh, then reboot)",
    "Path read failed (pw-link)", "Switch failed (wpctl)", "Autostart change failed (systemctl)",
    "Can't start recording (PipeWire)", "Can't play (PipeWire)",
    "Strictness", "Lower lets more through", "Hold", "Sound kept after speech",
    "No effect while off", "Default",
    "Can't change the noise filter (pw-cli)",
    "How are you listening?", "Sets the recommended settings",
    "Earphones / headphones", "Frame speakers",
    "Echo cancel: off", "Echo cancel: on",
    "Fine-tune",
    "Using fine-tuned settings", "Noise filter: off",
    "Echo cancel switch failed (wpctl)", "Noise filter switch failed (wpctl)",
    "Quick", "Open Fine-tune →", "Default: 23% · 500 ms (SteamOS)",

    // Updates (copied as-is from vendor/frame-updater/strings.md, except the confirmation hint and the
    // "installed" text, which match this app: install.sh doesn't restart it. The "installed" hint is ours)
    "Check for updates", "Looks on GitHub for a new version at start and once a day",
    "Up to date (%s)", "Checking for updates…", "Version %s is available",
    "Update", "This version can't be installed from here. Update by hand from GitHub", "Release page: ",
    "Update to %s?", "Downloads and installs the new version. Quit and start the app again to use it",
    "Update", "Cancel",
    "Updating: %s", "%s is installed. Quit and start the app again to use it",
    "The update failed (nothing was changed):", "Couldn't check for updates:",
    "Check now", "Try again", "Close", "Details: ~/.cache/<app>/update.log",
    "If the WirePlumber script changed too, restart the headset (see ~/.cache/frame-mic-tuner/update.log)",
    "Preparing", "Downloading", "Verifying", "Unpacking", "Installing",
    "Can't reach GitHub", "GitHub's rate limit was hit. Try again in an hour",
    "No published release", "Couldn't read GitHub's answer", "Couldn't read the version number",
    "Stopped: the download led outside GitHub", "A required command (python3) is missing",
    "This release has no SHA256SUMS. Update by hand", "This release has no file to install",
    "The download is corrupt (checksum mismatch)", "Stopped: the archive has unsafe contents",
    "The archive has no install.sh", "install.sh failed",
    "The saved install options are invalid", "Another update is running", "Already up to date",
    "Couldn't start the update (systemd-run)", "The update was interrupted", "Couldn't write files",
    "Something went wrong",

    // Mute
    "Muted", "Mic is muted", "Unmute", "Couldn't unmute (wpctl)",
};

}  // namespace

const UiText& uiText(Language language) {
    return language == Language::En ? kEnglish : kJapanese;
}

Language systemLanguage() {
    static const Language cached = detectSystemLanguage();
    return cached;
}

const char* languageCode(Language language) {
    return language == Language::En ? "en" : "ja";
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
    return false;
}
