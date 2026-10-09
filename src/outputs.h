// 音の出口（既定の出力）と使うマイク（既定の入力）: PipeWire の一覧の読み取り、覚えるときのキー、既定の切り替え。
// 出口ごとに Frame マイクの設定（エコー除去・ノイズ除去・ノイズ除去の強さ）を覚える（OutputProfile。保存は config.json）。
// 外部コマンドは固定の引数だけで呼ぶ。PipeWire・WirePlumber の再起動はしない。
#pragma once

#include <string>
#include <vector>

/** 音の出口かマイク 1 つ（PipeWire の Audio/Sink か Audio/Source のノードで、本物の機器のもの）。 */
struct AudioEndpoint {
    std::string key;       ///< 覚えるときのキー（機器のノード名。Valve のラッパーは中身の名前、Bluetooth は末尾のプロファイル番号を外す）
    std::string nodeName;  ///< 既定にするノードの名前（Frame 内蔵は Valve のラッパーがあればそちら。初めの既定と同じ）
    int nodeId = -1;       ///< そのノードの id（wpctl set-default に渡す）
    std::string name;      ///< 表示名（機器の説明。無ければノードの説明）
    bool builtin = false;  ///< Frame 内蔵（スピーカー・マイク）

    /** @return 中身が同じなら true */
    bool operator==(const AudioEndpoint& other) const {
        return key == other.key && nodeName == other.nodeName && nodeId == other.nodeId && name == other.name &&
               builtin == other.builtin;
    }
};

/** 音の出口とマイクの一覧と、今の既定。 */
struct AudioDevices {
    bool known = false;                  ///< 一覧を読めた（pw-dump）
    bool defaultsKnown = false;          ///< 既定を読めた（pw-metadata か pw-dump の default メタデータ）
    std::vector<AudioEndpoint> outputs;  ///< つながっている音の出口（Frame のスピーカーが先頭）
    std::vector<AudioEndpoint> inputs;   ///< つながっているマイク（Frame 内蔵が先頭）
    std::string defaultSinkName;         ///< default.audio.sink のノード名
    std::string defaultSourceName;       ///< default.audio.source のノード名
    std::string defaultOutputKey;        ///< 今の音の出口のキー（空 = 分からない）
    std::string defaultInputKey;         ///< 今のマイクのキー

    /**
     * @param key 出口のキー
     * @return つながっている出口（無ければ nullptr）
     */
    const AudioEndpoint* findOutput(const std::string& key) const;
    /**
     * @param key マイクのキー
     * @return つながっているマイク（無ければ nullptr）
     */
    const AudioEndpoint* findInput(const std::string& key) const;

    /** @return 中身が同じなら true */
    bool operator==(const AudioDevices& other) const;
    /** @return 中身が違えば true */
    bool operator!=(const AudioDevices& other) const { return !(*this == other); }
};

/** Frame のスピーカー（既定の出力）の機器のノード名。 */
constexpr const char* kBuiltinSpeakerKey = "alsa_output.platform-sound.HiFi__Speaker__sink";
/** Frame 内蔵マイク（音の通り道の出発点）の機器のノード名。 */
constexpr const char* kBuiltinMicKey = "alsa_input.platform-sound.HiFi__Mic__source";

/**
 * ノード名から、覚えるときのキーを作る（pw-metadata の名前だけで決められる、文字列の決まり）。
 * - Valve のラッパー alsa_loopback_device.[stereo.]alsa_output.X → alsa_output.X（中身の機器）
 * - Bluetooth bluez_output.<アドレス>.<番号> → bluez_output.<アドレス>（プロファイルが変わっても同じキー）
 * - ほか（alsa_output.usb-... など）はそのまま
 * @param nodeName ノード名
 * @return キー
 */
std::string endpointKey(const std::string& nodeName);

/**
 * @param key 出口のキー
 * @return Frame のスピーカーなら true（名前に HiFi__Speaker__sink を含む）
 */
bool isBuiltinSpeaker(const std::string& key);

/**
 * @param key マイクのキー
 * @return Frame 内蔵マイクなら true（名前に HiFi__Mic__source を含む）
 */
bool isBuiltinMic(const std::string& key);

/**
 * 「pw-metadata -n default」の出力から、既定の出力と入力のノード名を読む。
 * 形: update: id:0 key:'default.audio.sink' value:'{"name":"..."}' type:'Spa:String:JSON'
 * @param text 出力
 * @param sink 既定の出力の書き込み先（無ければ空）
 * @param source 既定の入力の書き込み先（無ければ空）
 * @return default.audio.sink か default.audio.source のどちらかを読めたら true
 */
bool parseDefaultMetadata(const std::string& text, std::string& sink, std::string& source);

/**
 * 「pw-dump」（全部）の出力（JSON）から、本物の機器の音の出口とマイク、今の既定を読む。
 * 使うのは Audio/Sink・Audio/Source のうち alsa_* / bluez_* の機器のノードと、Valve のラッパー
 * （alsa_loopback_device.*。同じ node.link-group の alsa_loopback_stream の target.object が中身）。
 * フィルター（echo_cancel_sink・filter-chain-sink・smart filter）、ループバックのストリーム、モニターは入れない。
 * @param text 出力
 * @return 一覧（読めなければ known = false）
 */
AudioDevices parseAudioDevices(const std::string& text);

/**
 * 機器の名前を短くする（画面の「マイク」の欄・声のチェックの履歴用）。末尾の「USB Audio」「Analog Stereo」などを外す。
 * @param name 名前（例: "AB13X USB Audio"）
 * @return 短い名前（例: "AB13X"。外すと空になるならそのまま）
 */
std::string shortDeviceName(const std::string& name);

/**
 * 既定の出力と入力のノード名を読む（pw-metadata -n default。約 10ms。パネルを閉じている間も 2 秒おきに呼ぶ）。
 * @param sink 書き込み先
 * @param source 書き込み先
 * @return 読めたら true
 */
bool readDefaultNames(std::string& sink, std::string& source);

/**
 * 音の出口とマイクの一覧を読む（pw-dump。約 90ms。パネルが開いている間と、既定が変わったときだけ）。
 * @return 一覧
 */
AudioDevices readAudioDevices();

/**
 * 既定の出力か入力にする（wpctl set-default <id>）。なったかは pw-metadata で読み返して確かめる（最大 1.5 秒待つ）。
 * @param nodeId ノードの id
 * @param nodeName そのノードの名前（読み返しで比べる）
 * @param sink 出力なら true、入力なら false
 * @return 既定になったら true
 */
bool writeDefaultNode(int nodeId, const std::string& nodeName, bool sink);

/** 出口の種類（新しい出口の最初の設定と、絵）。 */
enum class OutputKind {
    Speaker,    ///< Frame のスピーカー（エコー除去オン・ノイズ除去オフから始める）
    Earphones,  ///< それ以外（イヤホン・ヘッドホン。エコー除去オフ・ノイズ除去オフから始める）
};

/** 出口ごとに覚える Frame マイクの設定（config.json の outputs）。 */
struct OutputProfile {
    std::string name;                     ///< 表示名（最後に見たときの機器の説明）
    OutputKind kind = OutputKind::Earphones;
    bool echo = false;                    ///< エコー除去
    bool ns = false;                      ///< ノイズ除去
    double nsVad = 23.0;                  ///< ノイズ除去の判定の厳しさ（%）
    double nsGrace = 500.0;               ///< ノイズ除去の余韻（ms）
    std::string lastUsed;                 ///< 最後に使った日（YYYY-MM-DD、ローカル時刻。空 = 分からない）

    /** @return エコー除去・ノイズ除去・強さが同じなら true（名前と日付は比べない） */
    bool sameSettings(const OutputProfile& other) const;
};

/**
 * 初めて見た出口の設定（Frame のスピーカーならスピーカーのプリセット、ほかはイヤホンのプリセット。強さは SteamOS の値）。
 * @param key 出口のキー
 * @param name 表示名
 * @return 設定
 */
OutputProfile firstProfile(const std::string& key, const std::string& name);

/**
 * 今日の日付（ローカル時刻の YYYY-MM-DD）。
 * @return 日付
 */
std::string todayText();
