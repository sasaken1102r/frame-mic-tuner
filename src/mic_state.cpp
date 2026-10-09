// マイクの状態の読み書きの実装。
#include "mic_state.h"

#include "command.h"
#include "json.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <map>
#include <set>
#include <sstream>

namespace {

/** Frame のマイク（ALSA のデジタルマイク）のノード。音の通り道の出発点。 */
constexpr const char* kMicSource = "alsa_input.platform-sound.HiFi__Mic__source";
/** 通り道の終点（ここから既定のマイク alsa_loopback_device... になってアプリに届く）。 */
constexpr const char* kLoopbackStreamPrefix = "alsa_loopback_stream.alsa_input.";
/** アプリが音を取る既定のマイク。 */
constexpr const char* kLoopbackDevicePrefix = "alsa_loopback_device.alsa_input.";
/** 通り道の段の上限（ループしてもここで止める）。 */
constexpr int kMaxStages = 8;
/** wpctl で既定のマイクを指す名前（ミュートの読み書き）。 */
constexpr const char* kDefaultSource = "@DEFAULT_AUDIO_SOURCE@";

/**
 * 文字列が接頭辞で始まるか。
 * @param text 文字列
 * @param prefix 接頭辞
 * @return 始まるなら true
 */
bool startsWith(const std::string& text, const std::string& prefix) {
    return text.compare(0, prefix.size(), prefix) == 0;
}

/**
 * 文字列が接尾辞で終わるか。
 * @param text 文字列
 * @param suffix 接尾辞
 * @return 終わるなら true
 */
bool endsWith(const std::string& text, const std::string& suffix) {
    return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

/**
 * 「ノード名:ポート名」からノード名を取り出す（ポート名に ':' は入らないので最後の ':' で切る）。
 * @param port ポートの表記
 * @return ノード名
 */
std::string nodeOf(const std::string& port) {
    const size_t colon = port.rfind(':');
    return colon == std::string::npos ? port : port.substr(0, colon);
}

/**
 * フィルターのノードの接頭辞から段の種類を決める（frame-mic-tracker.lua と同じ見分け方）。
 * @param prefix 接頭辞（例: "echo_cancel"）
 * @return 段
 */
ChainStage stageOf(const std::string& prefix) {
    ChainStage stage;
    stage.name = prefix;
    if (prefix == "eq") {
        stage.kind = ChainStage::Kind::Eq;
    } else if (startsWith(prefix, "echo_cancel")) {
        stage.kind = ChainStage::Kind::EchoCancel;
    } else if (startsWith(prefix, "ns") || startsWith(prefix, "dsp")) {
        stage.kind = ChainStage::Kind::NoiseSuppression;
    }
    return stage;
}

/**
 * 前後の空白と改行を取る。
 * @param text 文字列
 * @return 取ったもの
 */
std::string trim(const std::string& text) {
    const size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return "";
    const size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

/**
 * 設定を 1 つ読む。
 * @param key 設定のキー
 * @param value 読めたときの書き込み先
 * @param error 失敗したときの種類の書き込み先（成功なら変えない）
 * @return 読めたら true
 */
bool readSetting(const char* key, bool& value, MicError& error) {
    const std::vector<std::string> argv = {"wpctl", "settings", key};
    const CommandResult result = runCommand(argv);
    if (result.ok() && parseSettingValue(result.out, value)) return true;
    // 設定が見つからない = WirePlumber に frame-mic-tracker と 90-frame-mic.conf が入っていない
    const bool notFound = result.ok() && result.out.find("not found") != std::string::npos;
    std::fprintf(stderr, "[マイク] 設定を読めません: %s%s\n", describeCommand(argv, result).c_str(),
                 notFound ? "（設定が見つかりません）" : "");
    error = notFound ? MicError::NotInstalled : MicError::ReadSettings;
    return false;
}

/**
 * 既定のマイクのミュートを読む（wpctl get-volume @DEFAULT_AUDIO_SOURCE@）。
 * @param muted 読めたときの書き込み先
 * @return 読めたら true
 */
bool readMute(bool& muted) {
    const std::vector<std::string> argv = {"wpctl", "get-volume", kDefaultSource};
    const CommandResult result = runCommand(argv);
    if (result.ok() && parseMute(result.out, muted)) return true;
    std::fprintf(stderr, "[マイク] ミュートを読めません: %s（出力: %s）\n", describeCommand(argv, result).c_str(),
                 trim(result.out).c_str());
    return false;
}

}  // namespace

bool parseSettingValue(const std::string& text, bool& value) {
    // "Value: true (Saved: true)" の形。一覧表示（wpctl settings）の "Value: true\t[Saved: true]" でも読める
    const size_t pos = text.find("Value: ");
    if (pos == std::string::npos) return false;
    const std::string rest = text.substr(pos + 7);
    if (startsWith(rest, "true")) {
        value = true;
        return true;
    }
    if (startsWith(rest, "false")) {
        value = false;
        return true;
    }
    return false;
}

bool parseSettingFromList(const std::string& text, const std::string& key, bool& value) {
    const std::string marker = "- Id: " + key + "\n";
    const size_t begin = text.find(marker);
    if (begin == std::string::npos) return false;
    const size_t end = text.find("- Id: ", begin + marker.size());
    return parseSettingValue(text.substr(begin, end == std::string::npos ? std::string::npos : end - begin), value);
}

void parseLinks(const std::string& text, MicState& state) {
    state.chain.clear();
    state.users.clear();
    state.rawLinks.clear();
    state.linksKnown = false;
    state.inUse = false;

    // ノードからノードへのつながり（出力側 → 入力側）を集める。
    // 形: 行頭にポート「ノード:ポート」、続く行に「  |-> 相手のポート」（出ていく）か「  |<- 相手」（入ってくる）。
    // 同じリンクが両側に出てくるので、「|->」の行だけ使う
    std::map<std::string, std::set<std::string>> edges;
    std::istringstream in(text);
    std::string line;
    std::string currentPort;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (line[0] != ' ') {
            currentPort = trim(line);
            continue;
        }
        const std::string entry = trim(line);
        if (!startsWith(entry, "|-> ") || currentPort.empty()) continue;
        const std::string from = nodeOf(currentPort);
        const std::string to = nodeOf(trim(entry.substr(4)));
        edges[from].insert(to);
        // --print 用に、マイクの通り道に関わるリンクだけ控える（スピーカー側は出さない）
        if (from.find("Speaker") == std::string::npos && to.find("Speaker") == std::string::npos &&
            from.find("filter-chain") == std::string::npos) {
            state.rawLinks += currentPort + " -> " + trim(entry.substr(4)) + "\n";
        }
    }

    // マイクから出発して、フィルター（<名前>_capture に入り <名前>_source から出る）をたどる
    std::string current = kMicSource;
    for (int step = 0; step < kMaxStages; ++step) {
        const auto found = edges.find(current);
        if (found == edges.end()) break;
        std::string next;
        bool reachedOutput = false;
        for (const auto& to : found->second) {
            if (startsWith(to, kLoopbackStreamPrefix)) reachedOutput = true;
            if (endsWith(to, "_capture")) next = to;
        }
        if (reachedOutput) {
            state.linksKnown = true;
            break;
        }
        if (next.empty()) break;  // 途中で切れている
        const std::string prefix = next.substr(0, next.size() - 8);
        state.chain.push_back(stageOf(prefix));
        current = prefix + "_source";
    }
    if (!state.linksKnown) state.chain.clear();
    // マイクを使うアプリがいる間だけ、tracker がフィルター（少なくとも EQ）を入れる
    state.inUse = state.linksKnown && !state.chain.empty();

    // 音を取っているノード（録音しているアプリ）: 既定のマイクから取っているものと、
    // マイクのノードから直接取っているもの（フィルターと出力以外）
    for (const auto& edge : edges) {
        const bool fromDevice = startsWith(edge.first, kLoopbackDevicePrefix);
        const bool fromMic = edge.first == kMicSource;
        if (!fromDevice && !fromMic) continue;
        for (const auto& to : edge.second) {
            if (fromMic && (endsWith(to, "_capture") || startsWith(to, kLoopbackStreamPrefix))) continue;
            state.users.push_back(to);
        }
    }
}

bool parseMute(const std::string& text, bool& muted) {
    // "Volume: 1.00" か "Volume: 1.00 [MUTED]"（1 行だけ）。音量の数字が無いものは読めない扱い
    const std::string line = trim(text.substr(0, text.find('\n')));
    const std::string prefix = "Volume: ";
    if (!startsWith(line, prefix) || line.size() == prefix.size() ||
        !std::isdigit(static_cast<unsigned char>(line[prefix.size()]))) {
        return false;
    }
    const size_t numberEnd = line.find(' ', prefix.size());
    const std::string rest = numberEnd == std::string::npos ? "" : trim(line.substr(numberEnd));
    if (rest.empty()) {
        muted = false;
        return true;
    }
    if (rest == "[MUTED]") {
        muted = true;
        return true;
    }
    return false;  // 知らない印
}

Autostart parseAutostart(const std::string& text) {
    const std::string first = trim(text.substr(0, text.find('\n')));
    if (first == "not-found") return Autostart::Missing;
    if (first == "disabled") return Autostart::Disabled;
    if (startsWith(first, "enabled")) return Autostart::Enabled;  // enabled / enabled-runtime
    return Autostart::Unknown;  // masked・static など（このアプリでは使わない形）
}

MicState readMicState(bool withAutostart) {
    MicState state;
    state.loaded = true;
    // 2 つの設定は、一覧表示 1 回でまとめて読む（wpctl の起動 1 回ぶん、キーごとに呼ぶのと同じ重さ）
    const std::vector<std::string> settingsArgv = {"wpctl", "settings"};
    const CommandResult settings = runCommand(settingsArgv);
    if (settings.ok()) {
        state.echoKnown = parseSettingFromList(settings.out, kEchoCancelKey, state.echo);
        state.nsKnown = parseSettingFromList(settings.out, kNoiseSuppressionKey, state.ns);
        if (!state.echoKnown || !state.nsKnown) {
            // 一覧に無い = WirePlumber に frame-mic-tracker と 90-frame-mic.conf が入っていない
            std::fprintf(stderr, "[マイク] 設定 %s / %s が一覧にありません\n", kEchoCancelKey, kNoiseSuppressionKey);
            state.readError = MicError::NotInstalled;
        }
    } else {
        std::fprintf(stderr, "[マイク] 設定を読めません: %s\n", describeCommand(settingsArgv, settings).c_str());
        state.readError = MicError::ReadSettings;
    }

    const std::vector<std::string> linkArgv = {"pw-link", "-l"};
    const CommandResult links = runCommand(linkArgv);
    if (links.ok()) {
        parseLinks(links.out, state);
    } else {
        std::fprintf(stderr, "[マイク] つながりを読めません: %s\n", describeCommand(linkArgv, links).c_str());
        if (state.readError == MicError::None) state.readError = MicError::ReadLinks;
    }

    // ミュート（Steam・wpctl・aux ボタンなど、どこから入っても気づけるよう、毎回読む）。読めなくても失敗の表示はしない
    state.muteKnown = readMute(state.muted);

    state.nsParams = readNsParams();

    if (!withAutostart) return state;
    // is-enabled は「無効」「見つからない」でも 0 以外の終了コードを返すので、出力で見分ける
    const std::vector<std::string> enabledArgv = {"systemctl", "--user", "is-enabled", kServiceName};
    const CommandResult enabled = runCommand(enabledArgv);
    if (enabled.status == CommandResult::Status::Ok || enabled.status == CommandResult::Status::Failed) {
        state.autostart = parseAutostart(enabled.out);
    }
    if (state.autostart == Autostart::Unknown) {
        std::fprintf(stderr, "[自動起動] 状態を読めません: %s（出力: %s）\n", describeCommand(enabledArgv, enabled).c_str(),
                     trim(enabled.out).c_str());
    }
    return state;
}

bool sameMicState(const MicState& a, const MicState& b) {
    if (a.chain.size() != b.chain.size()) return false;
    for (size_t i = 0; i < a.chain.size(); ++i) {
        if (a.chain[i].kind != b.chain[i].kind || a.chain[i].name != b.chain[i].name) return false;
    }
    const NsParams& pa = a.nsParams;
    const NsParams& pb = b.nsParams;
    const bool sameNs = pa.nodeKnown == pb.nodeKnown && pa.nodeId == pb.nodeId && pa.vadKnown == pb.vadKnown &&
                        pa.vad == pb.vad && pa.graceKnown == pb.graceKnown && pa.grace == pb.grace;
    return a.loaded == b.loaded && a.echoKnown == b.echoKnown && a.echo == b.echo && a.nsKnown == b.nsKnown &&
           a.ns == b.ns && a.linksKnown == b.linksKnown && a.inUse == b.inUse && a.muteKnown == b.muteKnown &&
           a.muted == b.muted && a.autostart == b.autostart && sameNs && a.nsApplied == b.nsApplied &&
           a.devices == b.devices && a.readError == b.readError && a.writeError == b.writeError;
}

double clampNsVad(double value) {
    return std::round(std::clamp(value, kNsVadMin, kNsVadMax));
}

double clampNsGrace(double value) {
    return std::round(std::clamp(value, kNsGraceMin, kNsGraceMax) / 10.0) * 10.0;
}

NsParams parseNsDump(const std::string& text) {
    NsParams params;
    JsonValue root;
    std::string error;
    if (!parseJson(text, root, error) || root.type != JsonValue::Type::Array) return params;
    for (const JsonValue& object : root.items) {
        const JsonValue* info = object.get("info");
        const JsonValue* props = info != nullptr ? info->get("props") : nullptr;
        const JsonValue* name = props != nullptr ? props->get("node.name") : nullptr;
        if (name == nullptr || !name->isString() || name->text != kNsNodeName) continue;
        const JsonValue* id = object.get("id");
        if (id == nullptr || !id->isNumber()) continue;
        params.nodeKnown = true;
        params.nodeId = static_cast<int>(id->number);
        // info.params.Props は複数のオブジェクトの配列。LADSPA の control は "params": [名前, 値, 名前, 値, ...] にある
        const JsonValue* paramsObject = info->get("params");
        const JsonValue* propsList = paramsObject != nullptr ? paramsObject->get("Props") : nullptr;
        if (propsList == nullptr || propsList->type != JsonValue::Type::Array) break;
        for (const JsonValue& entry : propsList->items) {
            const JsonValue* list = entry.get("params");
            if (list == nullptr || list->type != JsonValue::Type::Array) continue;
            for (size_t i = 0; i + 1 < list->items.size(); i += 2) {
                const JsonValue& key = list->items[i];
                const JsonValue& value = list->items[i + 1];
                if (!key.isString() || !value.isNumber()) continue;
                if (key.text == kNsVadParam) {
                    params.vadKnown = true;
                    params.vad = value.number;
                } else if (key.text == kNsGraceParam) {
                    params.graceKnown = true;
                    params.grace = value.number;
                }
            }
        }
        break;
    }
    return params;
}

NsParams readNsParams() {
    const std::vector<std::string> argv = {"pw-dump", kNsNodeName};
    const CommandResult result = runCommand(argv);
    if (!result.ok()) {
        std::fprintf(stderr, "[ノイズ除去] 値を読めません: %s\n", describeCommand(argv, result).c_str());
        return NsParams();
    }
    return parseNsDump(result.out);
}

bool writeNsParams(int nodeId, double vad, double grace) {
    if (nodeId < 0) return false;
    // 数値はアプリで範囲に丸めてから文字列にする（外から来た文字列は混ぜない）
    char pod[256];
    std::snprintf(pod, sizeof(pod), "{ params = [ \"%s\" %.1f \"%s\" %.1f ] }", kNsVadParam, clampNsVad(vad),
                  kNsGraceParam, clampNsGrace(grace));
    const std::vector<std::string> argv = {"pw-cli", "set-param", std::to_string(nodeId), "Props", pod};
    const CommandResult result = runCommand(argv);
    std::fprintf(stderr, "[ノイズ除去] %s\n", describeCommand(argv, result).c_str());
    return result.ok();
}

bool writeSetting(const char* key, bool value) {
    const std::vector<std::string> argv = {"wpctl", "settings", "--save", key, value ? "true" : "false"};
    const CommandResult result = runCommand(argv);
    std::fprintf(stderr, "[マイク] %s\n", describeCommand(argv, result).c_str());
    if (!result.ok()) return false;
    // wpctl は知らないキーでも終了コード 0 のことがあるので、読み返して確かめる
    bool now = !value;
    MicError error = MicError::None;
    if (!readSetting(key, now, error) || now != value) {
        std::fprintf(stderr, "[マイク] 書いた値を読み返せません（%s）\n", key);
        return false;
    }
    return true;
}

bool writeMute(bool muted) {
    const std::vector<std::string> argv = {"wpctl", "set-mute", kDefaultSource, muted ? "1" : "0"};
    const CommandResult result = runCommand(argv);
    std::fprintf(stderr, "[マイク] %s\n", describeCommand(argv, result).c_str());
    if (!result.ok()) return false;
    // 書いたあとに読み返して、そうなったことを確かめる
    bool now = !muted;
    if (!readMute(now) || now != muted) {
        std::fprintf(stderr, "[マイク] %sことを確かめられません\n", muted ? "ミュートした" : "ミュートが外れた");
        return false;
    }
    return true;
}

bool writeAutostart(bool enable) {
    const std::vector<std::string> argv = {"systemctl", "--user", enable ? "enable" : "disable", kServiceName};
    const CommandResult result = runCommand(argv, 5000);
    std::fprintf(stderr, "[自動起動] %s\n", describeCommand(argv, result).c_str());
    return result.ok();
}

std::string describeChain(const MicState& state) {
    if (!state.linksKnown) return "（つながりを読めません）";
    std::string text = "マイク";
    for (const auto& stage : state.chain) {
        switch (stage.kind) {
            case ChainStage::Kind::Eq: text += " → EQ"; break;
            case ChainStage::Kind::EchoCancel: text += " → エコー除去"; break;
            case ChainStage::Kind::NoiseSuppression: text += " → ノイズ除去"; break;
            case ChainStage::Kind::Other: text += " → " + stage.name; break;
        }
    }
    return text + " → 出力";
}
