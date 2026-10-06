// マイクのフィルターの設定（WirePlumber の frame-mic.*）とつながり（pw-link）、自動起動（systemd）の読み書き。
// 外部コマンドは固定の引数だけで呼ぶ。PipeWire・WirePlumber の再起動はしない。
#pragma once

#include <cstdint>
#include <string>
#include <vector>

/** WirePlumber の設定のキー（contrib/wireplumber/frame-mic-tracker.lua が見ている）。 */
constexpr const char* kEchoCancelKey = "frame-mic.echo-cancel";
constexpr const char* kNoiseSuppressionKey = "frame-mic.noise-suppression";
/** 自動起動の systemd ユーザーサービスの名前。 */
constexpr const char* kServiceName = "frame-mic-tuner.service";

/** ノイズ除去の段のノード（filter-chain の入口。LADSPA の control はこのノードの Props の params にある）。 */
constexpr const char* kNsNodeName = "ns_capture";
/** ノイズ除去（LADSPA noise_suppressor_mono）の判定の厳しさ（VAD しきい値、%）の param 名と範囲。 */
constexpr const char* kNsVadParam = "noise_suppressor_mono:VAD Threshold (%)";
constexpr double kNsVadMin = 0.0;
constexpr double kNsVadMax = 99.0;
constexpr double kNsVadDefault = 23.0;   ///< SteamOS の既定（Valve の filter-chain の設定）
constexpr double kNsVadStep = 1.0;
/** 余韻（VAD の猶予、ms）の param 名と範囲。 */
constexpr const char* kNsGraceParam = "noise_suppressor_mono:VAD Grace Period (ms)";
constexpr double kNsGraceMin = 0.0;
constexpr double kNsGraceMax = 1000.0;
constexpr double kNsGraceDefault = 500.0;  ///< SteamOS の既定
constexpr double kNsGraceStep = 50.0;

/**
 * 判定の厳しさを範囲に収め、1% 単位に丸める。
 * @param value 値（%）
 * @return 丸めた値
 */
double clampNsVad(double value);

/**
 * 余韻を範囲に収め、10ms 単位に丸める。
 * @param value 値（ms）
 * @return 丸めた値
 */
double clampNsGrace(double value);

/** マイクの音の通り道の 1 段。 */
struct ChainStage {
    enum class Kind {
        Eq,                ///< EQ（音質補正。eq_capture / eq_source）
        EchoCancel,        ///< エコー除去（echo_cancel_capture / echo_cancel_source）
        NoiseSuppression,  ///< ノイズ除去（ns_* / dsp_*）
        Other,             ///< 知らないフィルター（name に接頭辞）
    };
    Kind kind = Kind::Other;
    std::string name;  ///< ノードの接頭辞（例: "echo_cancel"）
};

/** 自動起動（systemctl --user is-enabled）の状態。 */
enum class Autostart {
    Unknown,   ///< 読めない（systemctl が動かないなど）
    Missing,   ///< ユニットファイルが無い（not-found）
    Enabled,   ///< 有効（SteamVR と一緒に起動する）
    Disabled,  ///< 無効
};

/** 表示する失敗の種類（文言は i18n の表から引く）。 */
enum class MicError {
    None,
    ReadSettings,    ///< 設定を読めない（wpctl）
    NotInstalled,    ///< 設定が見つからない（切り替えの仕組みが入っていない）
    ReadLinks,       ///< つながりを読めない（pw-link）
    WriteSettings,   ///< 切り替えに失敗（wpctl）
    WriteAutostart,  ///< 自動起動の切り替えに失敗（systemctl）
    WriteNsParams,   ///< ノイズ除去の強さを変えられない（pw-cli）
    WriteEcho,       ///< プリセットのうち、エコー除去の切り替えに失敗（wpctl）
    WriteNs,         ///< プリセットのうち、ノイズ除去の切り替えに失敗（wpctl）
    WriteMute,       ///< ミュートを解除できない（wpctl set-mute）
};

/** ノイズ除去の段の今の値（pw-dump ns_capture から読む）。 */
struct NsParams {
    bool nodeKnown = false;   ///< ns_capture が見つかったか
    int nodeId = -1;          ///< そのノードの id（PipeWire がノードを作り直すと変わる）
    bool vadKnown = false;
    double vad = 0.0;         ///< 判定の厳しさ（VAD しきい値、%）
    bool graceKnown = false;
    double grace = 0.0;       ///< 余韻（VAD の猶予、ms）
};

/** ある時点のマイクの状態（ワーカーが読み、画面が写しを使う）。 */
struct MicState {
    bool loaded = false;        ///< 一度でも読んだか
    bool echoKnown = false;     ///< エコー除去の値が読めたか
    bool echo = false;          ///< エコー除去（true = オン）
    bool nsKnown = false;       ///< ノイズ除去の値が読めたか
    bool ns = false;            ///< ノイズ除去（true = オン）
    bool linksKnown = false;    ///< マイクから出力までのつながりをたどれたか
    bool inUse = false;         ///< マイク使用中（通り道にフィルターが入っている）
    bool muteKnown = false;     ///< 既定のマイクのミュートが読めたか
    bool muted = false;         ///< 既定のマイクがミュートされている（Steam・wpctl・aux ボタンなど、どこから入っても）
    std::vector<ChainStage> chain;   ///< マイクと出力の間に入っているフィルター（順番どおり）
    std::vector<std::string> users;  ///< マイクから音を取っているノード（--print 用）
    Autostart autostart = Autostart::Unknown;
    NsParams nsParams;          ///< ノイズ除去の強さ
    MicError readError = MicError::None;   ///< 直近の読み取りの失敗
    MicError writeError = MicError::None;  ///< 直近の書き込みの失敗（次に成功するまで残す）
    std::string rawLinks;       ///< pw-link -l の出力のうちマイクに関わる行（--print 用）
};

/**
 * 「wpctl settings キー」の出力から値を読む（例: "Value: true (Saved: true)"）。
 * @param text 出力
 * @param value 読めたときの書き込み先
 * @return 読めたら true（"Setting '...' not found" などは false）
 */
bool parseSettingValue(const std::string& text, bool& value);

/**
 * 「wpctl settings」（キーなし、全設定の一覧）の出力から、1 つの設定の値を読む。
 * 形: "- Id: キー" の行から次の "- Id:" までの間にある "Value: true\t[Saved: true]"。
 * @param text 出力
 * @param key 設定のキー
 * @param value 読めたときの書き込み先
 * @return 読めたら true（キーが一覧に無いときは false）
 */
bool parseSettingFromList(const std::string& text, const std::string& key, bool& value);

/**
 * 「pw-link -l」の出力から、マイクから出力（alsa_loopback_stream.alsa_input...）までの通り道をたどる。
 * @param text 出力
 * @param state chain・inUse・linksKnown・users・rawLinks を書き込む
 */
void parseLinks(const std::string& text, MicState& state);

/**
 * 「systemctl --user is-enabled」の出力を状態にする。
 * @param text 出力
 * @return 状態
 */
Autostart parseAutostart(const std::string& text);

/**
 * 「wpctl get-volume @DEFAULT_AUDIO_SOURCE@」の出力からミュートを読む（例: "Volume: 1.00 [MUTED]"）。
 * @param text 出力
 * @param muted 読めたときの書き込み先（[MUTED] があれば true）
 * @return 読めたら true（"Volume:" で始まらない出力は false）
 */
bool parseMute(const std::string& text, bool& muted);

/**
 * 既定のマイクのミュートを解除する（wpctl set-mute @DEFAULT_AUDIO_SOURCE@ 0）。解除できたかは読み返して確かめる。
 * @return 解除できたら true
 */
bool writeUnmute();

/**
 * 「pw-dump ns_capture」の出力（JSON）から、ノードの id と判定の厳しさ・余韻を読む。
 * @param text 出力
 * @return 読めた値（ノードが無ければ nodeKnown = false）
 */
NsParams parseNsDump(const std::string& text);

/**
 * ノイズ除去の段の今の値を読む（pw-dump ns_capture）。
 * @return 読めた値
 */
NsParams readNsParams();

/**
 * ノイズ除去の段に、判定の厳しさと余韻を書く（pw-cli set-param <id> Props '{ params = [ ... ] }'）。
 * 値は範囲に丸めてから文字列にする。PipeWire の再起動はしない（その場で効く）。
 * @param nodeId ns_capture の id
 * @param vad 判定の厳しさ（%）
 * @param grace 余韻（ms）
 * @return 書けたら true
 */
bool writeNsParams(int nodeId, double vad, double grace);

/**
 * 設定・つながり・ミュート・自動起動・ノイズ除去の強さをまとめて読む（外部コマンドを最大 6 回呼ぶ。1 回 10〜40ms 程度）。
 * @param withAutostart 自動起動の状態も読むか（false なら autostart は Unknown のまま）
 * @return 読んだ状態（writeError は None）
 */
MicState readMicState(bool withAutostart = true);

/**
 * 画面に出す中身が同じか（描き直しが要らないか）。
 * @param a 状態
 * @param b 状態
 * @return 同じなら true
 */
bool sameMicState(const MicState& a, const MicState& b);

/**
 * WirePlumber の設定を書き、保存する（wpctl settings --save キー true|false）。
 * @param key kEchoCancelKey か kNoiseSuppressionKey
 * @param value 書く値
 * @return 書けたら true
 */
bool writeSetting(const char* key, bool value);

/**
 * 自動起動を切り替える（systemctl --user enable|disable frame-mic-tuner.service）。
 * start / --now はしない（いま動いているインスタンスとぶつからないように）。
 * @param enable 有効にするなら true
 * @return 成功したら true
 */
bool writeAutostart(bool enable);

/**
 * つながりを「マイク → 音質補正 → … → 出力」の形の 1 行にする（ログ・--print 用、日本語）。
 * @param state 状態
 * @return 1 行
 */
std::string describeChain(const MicState& state);
