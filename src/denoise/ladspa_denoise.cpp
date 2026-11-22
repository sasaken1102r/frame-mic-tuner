// 一定の雑音だけを引くノイズ除去を、PipeWire の filter-chain から読める LADSPA プラグインにする。
//   ラベル: frame_mic_denoise（1ch 入力・1ch 出力）
//   操作: "Strength (dB)"（0〜40、既定 20。雑音をどこまで下げるか。0 なら素通し）
// LADSPA の決まり（ladspa.h、API 1.1）は小さいので、ヘッダーを使わずに同じ形をここで書く。
#include "spectral_denoiser.h"

#include <new>

namespace {

typedef float LADSPA_Data;
typedef int LADSPA_Properties;
typedef int LADSPA_PortDescriptor;
typedef int LADSPA_PortRangeHintDescriptor;

struct LADSPA_PortRangeHint {
    LADSPA_PortRangeHintDescriptor HintDescriptor;
    LADSPA_Data LowerBound;
    LADSPA_Data UpperBound;
};

typedef void* LADSPA_Handle;

struct LADSPA_Descriptor {
    unsigned long UniqueID;
    const char* Label;
    LADSPA_Properties Properties;
    const char* Name;
    const char* Maker;
    const char* Copyright;
    unsigned long PortCount;
    const LADSPA_PortDescriptor* PortDescriptors;
    const char* const* PortNames;
    const LADSPA_PortRangeHint* PortRangeHints;
    void* ImplementationData;
    LADSPA_Handle (*instantiate)(const LADSPA_Descriptor* descriptor, unsigned long sampleRate);
    void (*connect_port)(LADSPA_Handle instance, unsigned long port, LADSPA_Data* location);
    void (*activate)(LADSPA_Handle instance);
    void (*run)(LADSPA_Handle instance, unsigned long sampleCount);
    void (*run_adding)(LADSPA_Handle instance, unsigned long sampleCount);
    void (*set_run_adding_gain)(LADSPA_Handle instance, LADSPA_Data gain);
    void (*deactivate)(LADSPA_Handle instance);
    void (*cleanup)(LADSPA_Handle instance);
};

constexpr LADSPA_Properties kPropertyRealtime = 0x1;
constexpr LADSPA_Properties kPropertyHardRtCapable = 0x4;
constexpr LADSPA_PortDescriptor kPortInput = 0x1;
constexpr LADSPA_PortDescriptor kPortOutput = 0x2;
constexpr LADSPA_PortDescriptor kPortControl = 0x4;
constexpr LADSPA_PortDescriptor kPortAudio = 0x8;
constexpr LADSPA_PortRangeHintDescriptor kHintBoundedBelow = 0x1;
constexpr LADSPA_PortRangeHintDescriptor kHintBoundedAbove = 0x2;
constexpr LADSPA_PortRangeHintDescriptor kHintDefaultMiddle = 0xC0;
constexpr LADSPA_PortRangeHintDescriptor kHintDefaultMaximum = 0x140;

enum Port : unsigned long { kInput = 0, kOutput = 1, kStrength = 2, kLatency = 3, kPortCount = 4 };

const LADSPA_PortDescriptor kPortDescriptors[kPortCount] = {
    kPortInput | kPortAudio,
    kPortOutput | kPortAudio,
    kPortInput | kPortControl,
    kPortOutput | kPortControl,
};
const char* const kPortNames[kPortCount] = {"Input", "Output", "Strength (dB)", "latency"};
const LADSPA_PortRangeHint kPortHints[kPortCount] = {
    {0, 0.0f, 0.0f},
    {0, 0.0f, 0.0f},
    // 既定は真ん中（20 dB）
    {kHintBoundedBelow | kHintBoundedAbove | kHintDefaultMiddle, 0.0f, 40.0f},
    {kHintBoundedBelow | kHintBoundedAbove | kHintDefaultMaximum, 0.0f, static_cast<float>(SpectralDenoiser::kFrame)},
};

/** プラグインの 1 個ぶん（filter-chain のノードごとに作られる） */
struct Instance {
    explicit Instance(double rate) : denoiser(rate) {}
    SpectralDenoiser denoiser;
    LADSPA_Data* ports[kPortCount] = {};
    float appliedStrength = -1.0f;
};

LADSPA_Handle instantiate(const LADSPA_Descriptor*, unsigned long sampleRate) {
    return new (std::nothrow) Instance(static_cast<double>(sampleRate));
}

void connectPort(LADSPA_Handle handle, unsigned long port, LADSPA_Data* location) {
    if (port < kPortCount) static_cast<Instance*>(handle)->ports[port] = location;
}

void activate(LADSPA_Handle handle) {
    auto* self = static_cast<Instance*>(handle);
    self->denoiser.reset();
    self->appliedStrength = -1.0f;
}

void run(LADSPA_Handle handle, unsigned long sampleCount) {
    auto* self = static_cast<Instance*>(handle);
    if (self->ports[kStrength] != nullptr && *self->ports[kStrength] != self->appliedStrength) {
        self->appliedStrength = *self->ports[kStrength];
        self->denoiser.setStrengthDb(self->appliedStrength);
    }
    if (self->ports[kLatency] != nullptr) *self->ports[kLatency] = static_cast<float>(SpectralDenoiser::kFrame);
    if (self->ports[kInput] == nullptr || self->ports[kOutput] == nullptr) return;
    self->denoiser.process(self->ports[kInput], self->ports[kOutput], sampleCount);
}

void cleanup(LADSPA_Handle handle) {
    delete static_cast<Instance*>(handle);
}

const LADSPA_Descriptor kDescriptor = {
    0x46ad01,  // UniqueID（公開の ID 登録はしていない。ラベルで探されるので重なっても困らない）
    "frame_mic_denoise",
    kPropertyRealtime | kPropertyHardRtCapable,
    "Frame Mic Tuner steady-noise remover",
    "sasaken@",
    "MIT",
    kPortCount,
    kPortDescriptors,
    kPortNames,
    kPortHints,
    nullptr,
    instantiate,
    connectPort,
    activate,
    run,
    nullptr,
    nullptr,
    nullptr,
    cleanup,
};

}  // namespace

extern "C" __attribute__((visibility("default"))) const LADSPA_Descriptor* ladspa_descriptor(unsigned long index) {
    return index == 0 ? &kDescriptor : nullptr;
}
