// 音の出口とマイクの一覧・キー・既定の切り替えの実装。
#include "outputs.h"

#include "command.h"
#include "json.h"
#include "mic_state.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <map>
#include <sstream>
#include <thread>

namespace {

/** Valve のラッパー（既定の出力・入力。中身は同じ link-group のストリームの target.object）の名前の頭。 */
constexpr const char* kWrapperPrefix = "alsa_loopback_device.";
/** ラッパーと対になるループバックのストリームの名前の頭。 */
constexpr const char* kWrapperStreamPrefix = "alsa_loopback_stream.";

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
 * 本物の機器の出口か（alsa_output.* / bluez_output.*）。
 * @param name ノード名
 * @return そうなら true
 */
bool isDeviceSinkName(const std::string& name) {
    return startsWith(name, "alsa_output.") || startsWith(name, "bluez_output.");
}

/**
 * 本物の機器のマイクか（alsa_input.* / bluez_input.*）。
 * @param name ノード名
 * @return そうなら true
 */
bool isDeviceSourceName(const std::string& name) {
    return startsWith(name, "alsa_input.") || startsWith(name, "bluez_input.");
}

/**
 * オブジェクトの props から文字列を取る。
 * @param props props
 * @param key キー
 * @return 文字列（無い・文字列でなければ空）
 */
std::string propText(const JsonValue* props, const char* key) {
    const JsonValue* value = props != nullptr ? props->get(key) : nullptr;
    return value != nullptr && value->isString() ? value->text : std::string();
}

/**
 * オブジェクトの props から整数を取る。
 * @param props props
 * @param key キー
 * @return 値（無ければ -1）
 */
int propInt(const JsonValue* props, const char* key) {
    const JsonValue* value = props != nullptr ? props->get(key) : nullptr;
    return value != nullptr && value->isNumber() ? static_cast<int>(value->number) : -1;
}

/**
 * default メタデータの値（{"name":"..."}。pw-metadata では文字列で、pw-dump では JSON のオブジェクトで来る）から名前を取る。
 * @param value 値
 * @return 名前（読めなければ空）
 */
std::string defaultName(const JsonValue& value) {
    if (value.isObject()) return propText(&value, "name");
    if (value.isString()) {
        JsonValue inner;
        std::string error;
        if (parseJson(value.text, inner, error)) return propText(&inner, "name");
    }
    return "";
}

/**
 * 一覧の並び: Frame 内蔵が先頭、あとは表示名の順。
 * @param list 一覧
 */
void sortEndpoints(std::vector<AudioEndpoint>& list) {
    std::stable_sort(list.begin(), list.end(), [](const AudioEndpoint& a, const AudioEndpoint& b) {
        if (a.builtin != b.builtin) return a.builtin;
        return a.name < b.name;
    });
}

}  // namespace

const AudioEndpoint* AudioDevices::findOutput(const std::string& key) const {
    for (const AudioEndpoint& e : outputs) {
        if (e.key == key) return &e;
    }
    return nullptr;
}

const AudioEndpoint* AudioDevices::findInput(const std::string& key) const {
    for (const AudioEndpoint& e : inputs) {
        if (e.key == key) return &e;
    }
    return nullptr;
}

bool AudioDevices::operator==(const AudioDevices& other) const {
    return known == other.known && defaultsKnown == other.defaultsKnown && outputs == other.outputs &&
           inputs == other.inputs && defaultSinkName == other.defaultSinkName &&
           defaultSourceName == other.defaultSourceName && defaultOutputKey == other.defaultOutputKey &&
           defaultInputKey == other.defaultInputKey;
}

std::string endpointKey(const std::string& nodeName) {
    std::string name = nodeName;
    if (startsWith(name, kWrapperPrefix)) {
        // alsa_loopback_device.stereo.alsa_output.X / alsa_loopback_device.alsa_input.X → 中身の機器の名前
        for (const char* inner : {"alsa_output.", "alsa_input.", "bluez_output.", "bluez_input."}) {
            const size_t at = name.find(inner, std::string(kWrapperPrefix).size());
            if (at != std::string::npos) {
                name = name.substr(at);
                break;
            }
        }
    }
    if (startsWith(name, "bluez_output.") || startsWith(name, "bluez_input.")) {
        // bluez_output.AA_BB_CC_DD_EE_FF.1 → bluez_output.AA_BB_CC_DD_EE_FF（プロファイルが変わっても同じ機器）
        const size_t first = name.find('.');
        const size_t second = name.find('.', first + 1);
        if (second != std::string::npos) name = name.substr(0, second);
    }
    return name;
}

bool isBuiltinSpeaker(const std::string& key) {
    return key.find("HiFi__Speaker__sink") != std::string::npos;
}

bool isDeviceOutputKey(const std::string& key) {
    return isDeviceSinkName(key);
}

bool isBuiltinMic(const std::string& key) {
    return key.find("HiFi__Mic__source") != std::string::npos;
}

bool parseDefaultMetadata(const std::string& text, std::string& sink, std::string& source) {
    sink.clear();
    source.clear();
    bool found = false;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        // update: id:0 key:'default.audio.sink' value:'{"name":"..."}' type:'Spa:String:JSON'
        const size_t keyAt = line.find("key:'");
        const size_t valueAt = line.find("value:'");
        if (keyAt == std::string::npos || valueAt == std::string::npos) continue;
        const size_t keyEnd = line.find('\'', keyAt + 5);
        const size_t valueEnd = line.find("' type:", valueAt + 7);
        if (keyEnd == std::string::npos || valueEnd == std::string::npos) continue;
        const std::string key = line.substr(keyAt + 5, keyEnd - keyAt - 5);
        JsonValue value;
        value.type = JsonValue::Type::String;
        value.text = line.substr(valueAt + 7, valueEnd - valueAt - 7);
        if (key == "default.audio.sink") {
            sink = defaultName(value);
            found = found || !sink.empty();
        } else if (key == "default.audio.source") {
            source = defaultName(value);
            found = found || !source.empty();
        }
    }
    return found;
}

AudioDevices parseAudioDevices(const std::string& text) {
    AudioDevices devices;
    JsonValue root;
    std::string error;
    if (!parseJson(text, root, error) || root.type != JsonValue::Type::Array) return devices;
    devices.known = true;

    std::map<int, std::string> deviceNames;              // 機器（Device）の id → 説明
    std::map<std::string, std::string> wrapperTargets;   // link-group → ラッパーの中身（ストリームの target.object）
    struct NodeInfo {
        int id;
        std::string name;
        std::string description;
        std::string mediaClass;
        std::string linkGroup;
        int deviceId;
    };
    std::vector<NodeInfo> nodes;
    for (const JsonValue& object : root.items) {
        const JsonValue* type = object.get("type");
        const JsonValue* id = object.get("id");
        if (type == nullptr || !type->isString() || id == nullptr || !id->isNumber()) continue;
        if (type->text == "PipeWire:Interface:Metadata") {
            const JsonValue* props = object.get("props");
            const JsonValue* entries = object.get("metadata");
            if (propText(props, "metadata.name") != "default" || entries == nullptr) continue;
            for (const JsonValue& entry : entries->items) {
                const JsonValue* key = entry.get("key");
                const JsonValue* value = entry.get("value");
                if (key == nullptr || !key->isString() || value == nullptr) continue;
                if (key->text == "default.audio.sink") devices.defaultSinkName = defaultName(*value);
                if (key->text == "default.audio.source") devices.defaultSourceName = defaultName(*value);
            }
            devices.defaultsKnown = true;
            continue;
        }
        const JsonValue* info = object.get("info");
        const JsonValue* props = info != nullptr ? info->get("props") : nullptr;
        if (props == nullptr) continue;
        if (type->text == "PipeWire:Interface:Device") {
            deviceNames[static_cast<int>(id->number)] = propText(props, "device.description");
            continue;
        }
        if (type->text != "PipeWire:Interface:Node") continue;
        NodeInfo node {static_cast<int>(id->number), propText(props, "node.name"), propText(props, "node.description"),
                       propText(props, "media.class"), propText(props, "node.link-group"), propInt(props, "device.id")};
        if (startsWith(node.name, kWrapperStreamPrefix) && !node.linkGroup.empty()) {
            const std::string target = propText(props, "target.object");
            if (!target.empty()) wrapperTargets[node.linkGroup] = target;
        }
        nodes.push_back(node);
    }

    /**
     * ノードの表示名（機器の説明があればそれ、無ければノードの説明、それも無ければノード名）。
     */
    const auto displayName = [&](const NodeInfo& node) {
        const auto it = deviceNames.find(node.deviceId);
        if (it != deviceNames.end() && !it->second.empty()) return it->second;
        return node.description.empty() ? node.name : node.description;
    };
    // 1) 本物の機器のノード
    for (const NodeInfo& node : nodes) {
        const bool sink = node.mediaClass == "Audio/Sink" && isDeviceSinkName(node.name);
        const bool source = node.mediaClass == "Audio/Source" && isDeviceSourceName(node.name);
        if (!sink && !source) continue;
        AudioEndpoint e;
        e.key = endpointKey(node.name);
        e.nodeName = node.name;
        e.nodeId = node.id;
        e.name = displayName(node);
        e.builtin = startsWith(e.key, "alsa_output.platform-") || startsWith(e.key, "alsa_input.platform-");
        (sink ? devices.outputs : devices.inputs).push_back(e);
    }
    // 2) Valve のラッパー: 中身の機器の「既定にするノード」をラッパーにする（初めの既定がラッパーなので、それに戻せるように）
    for (const NodeInfo& node : nodes) {
        if (!startsWith(node.name, kWrapperPrefix)) continue;
        const bool sink = node.mediaClass == "Audio/Sink";
        const bool source = node.mediaClass == "Audio/Source";
        if (!sink && !source) continue;
        const auto target = wrapperTargets.find(node.linkGroup);
        const std::string inner = target != wrapperTargets.end() ? target->second : endpointKey(node.name);
        const std::string key = endpointKey(inner);
        std::vector<AudioEndpoint>& list = sink ? devices.outputs : devices.inputs;
        auto it = std::find_if(list.begin(), list.end(), [&](const AudioEndpoint& e) { return e.key == key; });
        if (it == list.end()) {
            // 中身の機器が見えないときは、ラッパーをその機器として出す
            AudioEndpoint e;
            e.key = key;
            e.name = displayName(node);
            e.builtin = startsWith(key, "alsa_output.platform-") || startsWith(key, "alsa_input.platform-");
            list.push_back(e);
            it = list.end() - 1;
        }
        it->nodeName = node.name;
        it->nodeId = node.id;
    }
    sortEndpoints(devices.outputs);
    sortEndpoints(devices.inputs);

    // 既定のノード名をキーに直す（ラッパーでも機器そのものでも）
    const auto keyOf = [](const std::vector<AudioEndpoint>& list, const std::string& nodeName,
                          const std::map<std::string, std::string>& targets, const std::vector<NodeInfo>& all) {
        if (nodeName.empty()) return std::string();
        for (const AudioEndpoint& e : list) {
            if (e.nodeName == nodeName || e.key == endpointKey(nodeName)) return e.key;
        }
        // 一覧に無いノード（フィルターなど）が既定のとき: ラッパーなら中身、ほかは名前の決まりで
        for (const NodeInfo& node : all) {
            if (node.name != nodeName) continue;
            const auto target = targets.find(node.linkGroup);
            if (target != targets.end()) return endpointKey(target->second);
        }
        return endpointKey(nodeName);
    };
    devices.defaultOutputKey = keyOf(devices.outputs, devices.defaultSinkName, wrapperTargets, nodes);
    devices.defaultInputKey = keyOf(devices.inputs, devices.defaultSourceName, wrapperTargets, nodes);
    return devices;
}

std::string shortDeviceName(const std::string& name) {
    std::string shortName = name;
    // 機器の種類を表すだけの末尾の言葉を、外せるだけ外す（「Audio」だけは外さない。「USB Audio」が「USB」になるため）
    static const char* const kSuffixes[] = {" Analog Stereo", " Digital Stereo", " Stereo", " USB Audio"};
    bool changed = true;
    while (changed) {
        changed = false;
        for (const char* suffix : kSuffixes) {
            if (endsWith(shortName, suffix) && shortName.size() > std::string(suffix).size()) {
                shortName.erase(shortName.size() - std::string(suffix).size());
                changed = true;
            }
        }
    }
    return shortName.empty() ? name : shortName;
}

bool readDefaultNames(std::string& sink, std::string& source) {
    const std::vector<std::string> argv = {"pw-metadata", "-n", "default"};
    const CommandResult result = runCommand(argv);
    if (!result.ok()) {
        std::fprintf(stderr, "[出口] 既定を読めません: %s\n", describeCommand(argv, result).c_str());
        return false;
    }
    return parseDefaultMetadata(result.out, sink, source);
}

AudioDevices readAudioDevices() {
    const std::vector<std::string> argv = {"pw-dump"};
    const CommandResult result = runCommand(argv);
    if (!result.ok()) {
        std::fprintf(stderr, "[出口] 一覧を読めません: %s\n", describeCommand(argv, result).c_str());
        return AudioDevices();
    }
    AudioDevices devices = parseAudioDevices(result.out);
    if (!devices.known) std::fprintf(stderr, "[出口] pw-dump の出力を読めません\n");
    return devices;
}

bool writeDefaultNode(int nodeId, const std::string& nodeName, bool sink) {
    if (nodeId < 0 || nodeName.empty()) return false;
    const std::vector<std::string> argv = {"wpctl", "set-default", std::to_string(nodeId)};
    const CommandResult result = runCommand(argv);
    std::fprintf(stderr, "[出口] %s\n", describeCommand(argv, result).c_str());
    if (!result.ok()) return false;
    // WirePlumber が default.audio.* を書き直すまで少しかかることがあるので、最大 1.5 秒読み返す
    for (int attempt = 0; attempt < 15; ++attempt) {
        std::string nowSink;
        std::string nowSource;
        if (readDefaultNames(nowSink, nowSource) && (sink ? nowSink : nowSource) == nodeName) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::fprintf(stderr, "[出口] 既定が %s にならないことを読み返しました\n", nodeName.c_str());
    return false;
}

bool OutputProfile::sameSettings(const OutputProfile& other) const {
    return echo == other.echo && ns == other.ns && std::fabs(nsVad - other.nsVad) < 0.5 &&
           std::fabs(nsGrace - other.nsGrace) < 0.5;
}

OutputProfile firstProfile(const std::string& key, const std::string& name) {
    OutputProfile profile;
    profile.name = name;
    profile.kind = isBuiltinSpeaker(key) ? OutputKind::Speaker : OutputKind::Earphones;
    // プリセット（ユーザーと決めたもの）: スピーカー = エコー除去オン・ノイズ除去オフ、イヤホン = 両方オフ
    profile.echo = profile.kind == OutputKind::Speaker;
    profile.ns = false;
    profile.nsVad = kNsVadDefault;
    profile.nsGrace = kNsGraceDefault;
    return profile;
}

std::string todayText() {
    const std::time_t now = std::time(nullptr);
    std::tm tm {};
    localtime_r(&now, &tm);
    char text[16];
    std::strftime(text, sizeof(text), "%Y-%m-%d", &tm);
    return text;
}
