// --self-test の音の出口の分。Frame（2026-10-10、AB13X USB Audio をつないだ状態）で取った出力で試す。
#include "self_test.h"

#include "config.h"
#include "output_sync.h"
#include "outputs.h"

#include <unistd.h>

#include <cstdlib>
#include <string>
#include <vector>

namespace {

// pw-dump（全部）の出力のうち、音の機器・ノードと default メタデータだけを残したもの（props は使う分だけ）。
// 既定の出力は Valve のラッパー（126）、中身は Frame のスピーカー（73）。AB13X（65・114）はつながっているが既定ではない
const char* const kPwDumpFrame = R"json([{"id":39,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"filter-chain-sink","node.description":"Filter Chain Sink","media.class":"Audio/Sink","node.virtual":true,"node.link-group":"filter-chain-1790-30","filter.smart":true}}},
{"id":44,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"filter-chain-playback","node.description":"Filter Chain Sink","media.class":"Stream/Output/Audio","node.virtual":true,"node.link-group":"filter-chain-1790-30"}}},
{"id":45,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"eq_capture","node.description":"Mic EQ","media.class":"Stream/Input/Audio","node.virtual":true,"node.link-group":"filter-chain-1790-31"}}},
{"id":46,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"eq_source","node.description":"Mic EQ","media.class":"Audio/Source","node.virtual":true,"node.link-group":"filter-chain-1790-31","filter.smart":true}}},
{"id":47,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"echo_cancel_capture","node.description":"Echo-Cancel Capture","media.class":"Stream/Input/Audio","node.virtual":true,"node.link-group":"echo-cancel-1790-32"}}},
{"id":48,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"echo_cancel_source","node.description":"Echo-Cancel Source","media.class":"Audio/Source","node.virtual":true,"node.link-group":"echo-cancel-1790-32","filter.smart":true}}},
{"id":49,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"echo_cancel_sink","node.description":"Echo-Cancel Sink","media.class":"Audio/Sink","node.virtual":true,"node.link-group":"echo-cancel-1790-32","filter.smart":true}}},
{"id":50,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"echo_cancel_playback","node.description":"Echo-Cancel Playback","media.class":"Stream/Output/Audio","node.virtual":true,"node.link-group":"echo-cancel-1790-32"}}},
{"id":51,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"ns_capture","node.description":"Noise Suppressor","media.class":"Stream/Input/Audio","node.virtual":true,"node.link-group":"filter-chain-1790-33"}}},
{"id":52,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"ns_source","node.description":"Noise Suppressor","media.class":"Audio/Source","node.virtual":true,"node.link-group":"filter-chain-1790-33","filter.smart":true}}},
{"id":57,"type":"PipeWire:Interface:Metadata","props":{"metadata.name":"default"},"metadata":[{"subject":0,"key":"default.configured.audio.sink","type":"Spa:String:JSON","value":{"name":"alsa_loopback_device.stereo.alsa_output.platform-sound.HiFi__Speaker__sink"}},{"subject":0,"key":"default.configured.audio.source","type":"Spa:String:JSON","value":{"name":"alsa_loopback_device.alsa_input.platform-sound.HiFi__Mic__source"}},{"subject":0,"key":"default.audio.sink","type":"Spa:String:JSON","value":{"name":"alsa_loopback_device.stereo.alsa_output.platform-sound.HiFi__Speaker__sink"}},{"subject":0,"key":"default.audio.source","type":"Spa:String:JSON","value":{"name":"alsa_loopback_device.alsa_input.platform-sound.HiFi__Mic__source"}},{"subject":0,"key":"default.video.source","type":"Spa:String:JSON","value":{"name":"gamescope"}}]},
{"id":66,"type":"PipeWire:Interface:Device","info":{"props":{"device.name":"alsa_card.platform-sound","device.description":"Built-in Audio","device.api":"alsa","media.class":"Audio/Device","device.form-factor":"internal"}}},
{"id":69,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"alsa_loopback_device.alsa_input.platform-sound.HiFi__Mic__source","node.description":"Built-in Audio Capture","media.class":"Audio/Source","device.id":66,"node.virtual":false,"node.link-group":"loopback-1794-13","alsa.loopback":true}}},
{"id":70,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"alsa_loopback_stream.alsa_input.platform-sound.HiFi__Mic__source","node.description":"ALSA internal stream for Built-in Audio Capture","media.class":"Stream/Input/Audio","node.virtual":true,"node.link-group":"loopback-1794-13","target.object":"alsa_input.platform-sound.HiFi__Mic__source","alsa.loopback":true}}},
{"id":73,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"alsa_output.platform-sound.HiFi__Speaker__sink","node.description":"Built-in Audio Playback","media.class":"Audio/Sink","device.id":66,"node.nick":"Speaker playback"}}},
{"id":74,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"alsa_input.platform-sound.HiFi__Mic__source","node.description":"Built-in Audio Capture","media.class":"Audio/Source","device.id":66,"node.nick":"Mic"}}},
{"id":127,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"alsa_loopback_stream.stereo.alsa_output.platform-sound.HiFi__Speaker__sink","node.description":"ALSA internal stream for Built-in Audio Playback","media.class":"Stream/Output/Audio","node.virtual":true,"node.link-group":"loopback-1794-17","target.object":"alsa_output.platform-sound.HiFi__Speaker__sink","alsa.loopback":true}}},
{"id":126,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"alsa_loopback_device.stereo.alsa_output.platform-sound.HiFi__Speaker__sink","node.description":"Built-in Audio Playback","media.class":"Audio/Sink","device.id":66,"node.virtual":true,"node.link-group":"loopback-1794-17","alsa.loopback":true}}},
{"id":115,"type":"PipeWire:Interface:Device","info":{"props":{"device.name":"alsa_card.usb-Generic_AB13X_USB_Audio_202405280846-00","device.description":"AB13X USB Audio","device.bus":"usb","device.api":"alsa","media.class":"Audio/Device"}}},
{"id":65,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"alsa_output.usb-Generic_AB13X_USB_Audio_202405280846-00.analog-stereo","node.description":"AB13X USB Audio Analog Stereo","media.class":"Audio/Sink","device.id":115,"node.nick":"AB13X USB Audio"}}},
{"id":114,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"alsa_input.usb-Generic_AB13X_USB_Audio_202405280846-00.analog-stereo","node.description":"AB13X USB Audio Analog Stereo","media.class":"Audio/Source","device.id":115,"node.nick":"AB13X USB Audio"}}}])json";

// 作った例（実機の出力ではない）: AB13X を既定の出力と入力にし、Bluetooth のイヤホン（プロファイル番号 .1）もつないだ
const char* const kPwDumpAb13xAndBluetooth = R"json([
{"id":57,"type":"PipeWire:Interface:Metadata","props":{"metadata.name":"default"},"metadata":[{"subject":0,"key":"default.audio.sink","type":"Spa:String:JSON","value":{"name":"alsa_output.usb-Generic_AB13X_USB_Audio_202405280846-00.analog-stereo"}},{"subject":0,"key":"default.audio.source","type":"Spa:String:JSON","value":{"name":"alsa_input.usb-Generic_AB13X_USB_Audio_202405280846-00.analog-stereo"}}]},
{"id":66,"type":"PipeWire:Interface:Device","info":{"props":{"device.name":"alsa_card.platform-sound","device.description":"Built-in Audio","media.class":"Audio/Device"}}},
{"id":73,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"alsa_output.platform-sound.HiFi__Speaker__sink","node.description":"Built-in Audio Playback","media.class":"Audio/Sink","device.id":66}}},
{"id":126,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"alsa_loopback_device.stereo.alsa_output.platform-sound.HiFi__Speaker__sink","node.description":"Built-in Audio Playback","media.class":"Audio/Sink","device.id":66,"node.link-group":"loopback-1794-17"}}},
{"id":127,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"alsa_loopback_stream.stereo.alsa_output.platform-sound.HiFi__Speaker__sink","media.class":"Stream/Output/Audio","node.link-group":"loopback-1794-17","target.object":"alsa_output.platform-sound.HiFi__Speaker__sink"}}},
{"id":115,"type":"PipeWire:Interface:Device","info":{"props":{"device.name":"alsa_card.usb-Generic_AB13X_USB_Audio_202405280846-00","device.description":"AB13X USB Audio","media.class":"Audio/Device"}}},
{"id":65,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"alsa_output.usb-Generic_AB13X_USB_Audio_202405280846-00.analog-stereo","node.description":"AB13X USB Audio Analog Stereo","media.class":"Audio/Sink","device.id":115}}},
{"id":114,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"alsa_input.usb-Generic_AB13X_USB_Audio_202405280846-00.analog-stereo","node.description":"AB13X USB Audio Analog Stereo","media.class":"Audio/Source","device.id":115}}},
{"id":140,"type":"PipeWire:Interface:Device","info":{"props":{"device.name":"bluez_card.AA_BB_CC_DD_EE_FF","device.description":"WH-1000XM4","media.class":"Audio/Device"}}},
{"id":141,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"bluez_output.AA_BB_CC_DD_EE_FF.1","node.description":"WH-1000XM4","media.class":"Audio/Sink","device.id":140}}},
{"id":142,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"speaker-ref-sink","node.description":"Speaker Reference","media.class":"Audio/Sink","node.virtual":true}}}
])json";

// pw-metadata -n default の出力（Frame、2026-10-10）
const char* const kPwMetadataFrame =
    "Found \"default\" metadata 57\n"
    "update: id:0 key:'default.configured.audio.sink' "
    "value:'{\"name\":\"alsa_loopback_device.stereo.alsa_output.platform-sound.HiFi__Speaker__sink\"}' type:'Spa:String:JSON'\n"
    "update: id:0 key:'default.configured.audio.source' "
    "value:'{\"name\":\"alsa_loopback_device.alsa_input.platform-sound.HiFi__Mic__source\"}' type:'Spa:String:JSON'\n"
    "update: id:0 key:'default.audio.sink' "
    "value:'{\"name\":\"alsa_loopback_device.stereo.alsa_output.platform-sound.HiFi__Speaker__sink\"}' type:'Spa:String:JSON'\n"
    "update: id:0 key:'default.audio.source' "
    "value:'{\"name\":\"alsa_loopback_device.alsa_input.platform-sound.HiFi__Mic__source\"}' type:'Spa:String:JSON'\n"
    "update: id:0 key:'default.video.source' value:'{\"name\":\"gamescope\"}' type:'Spa:String:JSON'\n";

constexpr const char* kAb13xKey = "alsa_output.usb-Generic_AB13X_USB_Audio_202405280846-00.analog-stereo";

/** OutputSync が頼んだ書き込みを控える偽のワーカー。 */
struct FakeWorker {
    std::vector<MicCommand> commands;
    uint64_t ticket = 0;

    /** @return OutputSync に渡す関数 */
    OutputSync::Request request() {
        return [this](const MicCommand& command) {
            commands.push_back(command);
            return ++ticket;
        };
    }
};

/**
 * ワーカーが読んだ状態の代わり（設定は読めている・既定の出力は key）。
 * @param devices 一覧
 * @param echo エコー除去
 * @param ns ノイズ除去
 * @return 状態
 */
MicState stateWith(const AudioDevices& devices, bool echo, bool ns) {
    MicState state;
    state.loaded = state.echoKnown = state.nsKnown = true;
    state.echo = echo;
    state.ns = ns;
    state.nsParams.nodeKnown = state.nsParams.vadKnown = state.nsParams.graceKnown = true;
    state.nsParams.nodeId = 53;
    state.nsParams.vad = 23;
    state.nsParams.grace = 500;
    state.devices = devices;
    return state;
}

}  // namespace

void outputSelfTests(const Expect& expect) {
    // ---- キー ----
    expect("出口のキー: Valve のラッパー（stereo.）→ 中身の Frame のスピーカー",
           endpointKey("alsa_loopback_device.stereo.alsa_output.platform-sound.HiFi__Speaker__sink") == kBuiltinSpeakerKey);
    expect("出口のキー: マイクのラッパー → 中身の Frame 内蔵マイク",
           endpointKey("alsa_loopback_device.alsa_input.platform-sound.HiFi__Mic__source") == kBuiltinMicKey);
    expect("出口のキー: USB の機器はそのまま", endpointKey(kAb13xKey) == kAb13xKey);
    expect("出口のキー: Bluetooth はプロファイル番号を外す（.1 も .2 も同じキー）",
           endpointKey("bluez_output.AA_BB_CC_DD_EE_FF.1") == "bluez_output.AA_BB_CC_DD_EE_FF" &&
               endpointKey("bluez_output.AA_BB_CC_DD_EE_FF.2") == "bluez_output.AA_BB_CC_DD_EE_FF");
    expect("Frame のスピーカー・内蔵マイクの見分け",
           isBuiltinSpeaker(kBuiltinSpeakerKey) && !isBuiltinSpeaker(kAb13xKey) && isBuiltinMic(kBuiltinMicKey) &&
               !isBuiltinMic("alsa_input.usb-Generic_AB13X_USB_Audio_202405280846-00.analog-stereo"));
    expect("短い名前: 「AB13X USB Audio」→「AB13X」、「WH-1000XM4」はそのまま",
           shortDeviceName("AB13X USB Audio") == "AB13X" && shortDeviceName("AB13X USB Audio Analog Stereo") == "AB13X" &&
               shortDeviceName("WH-1000XM4") == "WH-1000XM4" && shortDeviceName("USB Audio") == "USB Audio");

    // ---- pw-metadata -n default ----
    std::string sink;
    std::string source;
    expect("pw-metadata: 既定の出力・入力（configured ではなく今の既定）を読む",
           parseDefaultMetadata(kPwMetadataFrame, sink, source) &&
               sink == "alsa_loopback_device.stereo.alsa_output.platform-sound.HiFi__Speaker__sink" &&
               source == "alsa_loopback_device.alsa_input.platform-sound.HiFi__Mic__source");
    expect("pw-metadata: 不正な出力は読めない", !parseDefaultMetadata("Found \"default\" metadata 57\n", sink, source));

    // ---- pw-dump（Frame の実際の出力） ----
    const AudioDevices frame = parseAudioDevices(kPwDumpFrame);
    expect("pw-dump: 出口は Frame のスピーカーと AB13X の 2 つ（フィルター・ストリームは入れない）",
           frame.known && frame.outputs.size() == 2 && frame.outputs[0].key == kBuiltinSpeakerKey &&
               frame.outputs[1].key == kAb13xKey);
    expect("pw-dump: Frame のスピーカーを既定にするノードは Valve のラッパー（126）",
           frame.outputs[0].nodeName == "alsa_loopback_device.stereo.alsa_output.platform-sound.HiFi__Speaker__sink" &&
               frame.outputs[0].nodeId == 126 && frame.outputs[0].builtin);
    expect("pw-dump: AB13X の名前は機器の説明（AB13X USB Audio）、既定にするノードは機器そのもの（65）",
           frame.outputs[1].name == "AB13X USB Audio" && frame.outputs[1].nodeId == 65 && !frame.outputs[1].builtin);
    expect("pw-dump: マイクは内蔵（ラッパー 69）と AB13X（114）",
           frame.inputs.size() == 2 && frame.inputs[0].key == kBuiltinMicKey && frame.inputs[0].nodeId == 69 &&
               frame.inputs[1].nodeId == 114);
    expect("pw-dump: 今の既定は Frame のスピーカーと内蔵マイク",
           frame.defaultsKnown && frame.defaultOutputKey == kBuiltinSpeakerKey && frame.defaultInputKey == kBuiltinMicKey);
    const AudioDevices other = parseAudioDevices(kPwDumpAb13xAndBluetooth);
    expect("pw-dump（作った例）: AB13X が既定、Bluetooth はキーからプロファイル番号を外す、speaker-ref-sink は入れない",
           other.defaultOutputKey == kAb13xKey && other.outputs.size() == 3 &&
               other.findOutput("bluez_output.AA_BB_CC_DD_EE_FF") != nullptr &&
               other.findOutput("bluez_output.AA_BB_CC_DD_EE_FF")->name == "WH-1000XM4" &&
               other.findOutput("bluez_output.AA_BB_CC_DD_EE_FF")->nodeName == "bluez_output.AA_BB_CC_DD_EE_FF.1" &&
               !isBuiltinMic(other.defaultInputKey));
    expect("pw-dump: 壊れた出力は読めない", !parseAudioDevices("[{\"id\":").known);

    // ---- 初めての出口の設定 ----
    const OutputProfile speakerFirst = firstProfile(kBuiltinSpeakerKey, "Built-in Audio");
    const OutputProfile earFirst = firstProfile(kAb13xKey, "AB13X USB Audio");
    expect("初めての出口: Frame のスピーカーはスピーカーのプリセット（エコー除去オン・ノイズ除去オフ・23%/500ms）",
           speakerFirst.kind == OutputKind::Speaker && speakerFirst.echo && !speakerFirst.ns &&
               speakerFirst.nsVad == kNsVadDefault && speakerFirst.nsGrace == kNsGraceDefault);
    expect("初めての出口: ほかはイヤホンのプリセット（両方オフ）",
           earFirst.kind == OutputKind::Earphones && !earFirst.echo && !earFirst.ns);
    const OutputProfile blank;
    expect("出口の設定の既定の強さは SteamOS の値", blank.nsVad == kNsVadDefault && blank.nsGrace == kNsGraceDefault);

    // ---- 出口の設定のかけ方（OutputSync） ----
    {
        // 前の版から初めて起動: 今の設定（エコー除去オフ・ノイズ除去オン、強さは前の版の 30%/600ms）を今の出口へ移す。何もかけない
        FakeWorker worker;
        OutputSync sync(worker.request());
        Config config;
        config.hasNsParams = true;
        config.nsVad = 30;
        config.nsGrace = 600;
        const bool changed = sync.update(stateWith(frame, false, true), config, "2026-10-10");
        const auto it = config.outputs.find(kBuiltinSpeakerKey);
        expect("移し替え: 今の設定を今の出口（Frame のスピーカー）の設定にし、何もかけない",
               changed && config.outputsSaved && it != config.outputs.end() && !it->second.echo && it->second.ns &&
                   it->second.nsVad == 30 && it->second.nsGrace == 600 && it->second.lastUsed == "2026-10-10" &&
                   worker.commands.empty() && !sync.notice().shown);
    }
    {
        // 起動: 覚えた設定（スピーカー = エコー除去オン）と今（オフ）が違う → かけて、知らせ（元に戻す = オフ）
        FakeWorker worker;
        OutputSync sync(worker.request());
        Config config;
        config.outputsSaved = true;
        config.outputs[kBuiltinSpeakerKey] = firstProfile(kBuiltinSpeakerKey, "Built-in Audio");
        MicState state = stateWith(frame, false, false);
        sync.update(state, config, "2026-10-10");
        expect("起動: 覚えた設定を、今と同じものは書かない形でかける（強さも）",
               worker.commands.size() == 1 && worker.commands[0].kind == MicCommand::Kind::ApplySettings &&
                   worker.commands[0].value && !worker.commands[0].ns && worker.commands[0].withNsParams &&
                   worker.commands[0].onlyIfChanged && sync.activeKey() == kBuiltinSpeakerKey);
        expect("起動: エコー除去が変わったので知らせを出す（元に戻すとオフ）",
               sync.notice().shown && sync.notice().key == kBuiltinSpeakerKey && !sync.notice().previous.echo);

        // かけ終わる前の読み直し（まだエコー除去オフ）は、外からの変化として取り込まない
        state.writesDone = 0;
        sync.update(state, config, "2026-10-10");
        expect("かけた直後: 書き込みが終わる前の値（オフ）を取り込まない", config.outputs[kBuiltinSpeakerKey].echo);

        // かけ終わった（オン）。そのあと外から（aux ボタンなど）ノイズ除去をオンにされた → 覚える
        state.writesDone = 1;
        state.echo = true;
        sync.update(state, config, "2026-10-10");
        state.ns = true;
        const bool adopted = sync.update(state, config, "2026-10-10");
        expect("外から変わった: 今の出口の設定として覚える", adopted && config.outputs[kBuiltinSpeakerKey].ns);

        // ノイズ除去の強さ: かけたい値をまだかけていない間（PipeWire が作り直した直後の 23/500）は取り込まない
        state.nsApplied = false;
        state.nsParams.vad = 99;
        config.outputs[kBuiltinSpeakerKey].nsVad = 40;
        sync.update(state, config, "2026-10-10");
        expect("強さ: かけ終わる前の値は取り込まない", config.outputs[kBuiltinSpeakerKey].nsVad == 40);
        state.nsApplied = true;
        sync.update(state, config, "2026-10-10");
        expect("強さ: かけ終わった後に外から変わった値は取り込む", config.outputs[kBuiltinSpeakerKey].nsVad == 99);

        // 出口が変わった（AB13X を既定にした）: 初めての出口なのでイヤホンのプリセットを作ってかけ、知らせを出す
        worker.commands.clear();
        AudioDevices switched = frame;
        switched.defaultSinkName = kAb13xKey;
        switched.defaultOutputKey = kAb13xKey;
        MicState after = state;
        after.devices = switched;
        sync.update(after, config, "2026-10-11");
        const auto ab13x = config.outputs.find(kAb13xKey);
        expect("出口が変わった: 初めての出口はイヤホンのプリセットで覚え、かける",
               ab13x != config.outputs.end() && !ab13x->second.echo && !ab13x->second.ns &&
                   ab13x->second.name == "AB13X USB Audio" && ab13x->second.lastUsed == "2026-10-11" &&
                   worker.commands.size() == 1 && !worker.commands[0].value && !worker.commands[0].ns);
        expect("出口が変わった: 知らせ（元に戻すと、前にかかっていたエコー除去オン・ノイズ除去オン）",
               sync.notice().shown && sync.notice().key == kAb13xKey && sync.notice().previous.echo &&
                   sync.notice().previous.ns && sync.activeKey() == kAb13xKey);

        // 元に戻す: 前の値をかけ、その出口の設定として覚える
        worker.commands.clear();
        const bool undone = sync.undo(config);
        expect("元に戻す: 前の値をかけて、AB13X の設定として覚え、知らせを消す",
               undone && config.outputs[kAb13xKey].echo && config.outputs[kAb13xKey].ns && worker.commands.size() == 1 &&
                   worker.commands[0].value && !worker.commands[0].onlyIfChanged && !sync.notice().shown);
    }
    {
        // 同じ設定の出口に変わったときは、何も変わらないので知らせない
        FakeWorker worker;
        OutputSync sync(worker.request());
        Config config;
        config.outputsSaved = true;
        config.outputs[kBuiltinSpeakerKey] = firstProfile(kBuiltinSpeakerKey, "Built-in Audio");
        sync.update(stateWith(frame, true, false), config, "2026-10-10");
        expect("起動: 今と同じ設定なら知らせない", !sync.notice().shown);
    }

    // ---- 設定ファイル（出口ごとの設定。一時ファイルで試して消す） ----
    {
        char path[] = "/tmp/frame-mic-tuner-selftest-outputs-XXXXXX";
        const int fd = ::mkstemp(path);
        if (fd >= 0) ::close(fd);
        Config saved;
        saved.outputsSaved = true;
        OutputProfile profile = firstProfile(kAb13xKey, "AB13X \"USB\" オーディオ");
        profile.ns = true;
        profile.nsVad = 12;
        profile.nsGrace = 650;
        profile.lastUsed = "2026-10-08";
        saved.outputs[kAb13xKey] = profile;
        saved.outputs[kBuiltinSpeakerKey] = firstProfile(kBuiltinSpeakerKey, "Built-in Audio");
        std::string error;
        std::vector<std::string> warnings;
        Config loaded;
        const bool ok = fd >= 0 && saveConfig(path, saved, error) && loadConfig(path, loaded, warnings, error);
        const auto it = loaded.outputs.find(kAb13xKey);
        expect("設定ファイル: 出口ごとの設定（名前の \" と日本語・強さ・最後に使った日）を保存して読み直すと同じ",
               ok && warnings.empty() && loaded.outputsSaved && loaded.outputs.size() == 2 && it != loaded.outputs.end() &&
                   it->second.name == profile.name && it->second.ns && it->second.nsVad == 12 &&
                   it->second.nsGrace == 650 && it->second.lastUsed == "2026-10-08" &&
                   it->second.kind == OutputKind::Earphones &&
                   loaded.outputs[kBuiltinSpeakerKey].kind == OutputKind::Speaker);
        Config empty;
        empty.outputsSaved = true;
        Config reloaded;
        const bool emptyOk = saveConfig(path, empty, error) && loadConfig(path, reloaded, warnings, error);
        expect("設定ファイル: 出口が 0 個（全部忘れた）でも outputs は残り、次の起動で移し替えをしない",
               emptyOk && reloaded.outputsSaved && reloaded.outputs.empty());
        ::unlink(path);
        Config fresh;
        expect("設定ファイル: outputs が無い（前の版）ときは移し替えの印が立たない", !fresh.outputsSaved);
    }
}
