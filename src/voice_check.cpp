// 声のチェックの実装。
#include "voice_check.h"

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/pod/builder.h>

#include <pthread.h>
#include <signal.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

/** つないだ直後に捨てる長さ（秒）。tracker がフィルターを入れて、つなぎ方が落ち着くまで。 */
constexpr double kWarmupSec = 0.3;
/** これより短い録音は履歴に入れない（秒）。 */
constexpr double kMinClipSec = 0.2;
/** 再生を出し切ってから drained が来なくても片付けるまでの秒数。 */
constexpr double kDrainTimeoutSec = 2.0;

/**
 * 単調増加の時計で今の時刻を秒で返す。
 * @return 秒
 */
double nowSeconds() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

/**
 * PipeWire のライブラリを 1 回だけ初期化する。
 */
void initPipeWireOnce() {
    static bool done = false;
    if (done) return;
    pw_init(nullptr, nullptr);
    done = true;
}

/**
 * 録った音から、区間ごとのピーク（小さい波形）と全体のピークを作る。
 * @param clip 書き込む 1 件（samples は入っていること）
 */
void buildWave(VoiceClip& clip) {
    clip.wave.assign(kWaveBins, 0.0f);
    int peak = 0;
    const size_t count = clip.samples.size();
    for (size_t i = 0; i < count; ++i) {
        const int value = std::abs(static_cast<int>(clip.samples[i]));
        peak = std::max(peak, value);
        const size_t bin = std::min<size_t>(kWaveBins - 1, i * kWaveBins / std::max<size_t>(1, count));
        clip.wave[bin] = std::max(clip.wave[bin], value / 32768.0f);
    }
    clip.peakDb = peakToDb(peak / 32768.0f);
}

}  // namespace

float peakToDb(float peak) {
    if (peak <= 1e-6f) return -120.0f;
    return 20.0f * std::log10(peak);
}

VoiceCheck::~VoiceCheck() {
    shutdown();
}

bool VoiceCheck::ensureLoop() {
    if (loop_ != nullptr) return true;
    initPipeWireOnce();
    loop_ = pw_thread_loop_new("frame-mic-tuner-audio", nullptr);
    if (loop_ == nullptr) {
        std::fprintf(stderr, "[声] PipeWire のスレッドを作れません\n");
        return false;
    }
    // SIGTERM・SIGINT・SIGUSR1 はメインスレッドで受けたいので、止めた状態でスレッドを作る（マスクは引き継がれる）
    sigset_t blocked;
    sigset_t previous;
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGTERM);
    sigaddset(&blocked, SIGINT);
    sigaddset(&blocked, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &blocked, &previous);
    const int started = pw_thread_loop_start(loop_);
    pthread_sigmask(SIG_SETMASK, &previous, nullptr);
    if (started != 0) {
        std::fprintf(stderr, "[声] PipeWire のスレッドを始められません\n");
        pw_thread_loop_destroy(loop_);
        loop_ = nullptr;
        return false;
    }
    return true;
}

const pw_stream_events& VoiceCheck::recordEvents() {
    static const pw_stream_events events = [] {
        pw_stream_events e {};
        e.version = PW_VERSION_STREAM_EVENTS;
        e.state_changed = [](void* data, pw_stream_state, pw_stream_state state, const char* error) {
            if (state != PW_STREAM_STATE_ERROR) return;
            std::fprintf(stderr, "[声] 録音のストリームが失敗しました: %s\n", error != nullptr ? error : "?");
            static_cast<VoiceCheck*>(data)->recordFailed_ = true;
        };
        e.process = [](void* data) {
            auto* self = static_cast<VoiceCheck*>(data);
            pw_buffer* buffer = pw_stream_dequeue_buffer(self->recordStream_);
            if (buffer == nullptr) return;
            const spa_data& d = buffer->buffer->datas[0];
            if (d.data != nullptr && d.chunk != nullptr) {
                const uint32_t offset = std::min(d.chunk->offset, d.maxsize);
                const uint32_t size = std::min(d.chunk->size, d.maxsize - offset);
                const auto* samples = reinterpret_cast<const int16_t*>(static_cast<const uint8_t*>(d.data) + offset);
                const size_t count = size / sizeof(int16_t);
                for (size_t i = 0; i < count; ++i) {
                    const float value = std::abs(static_cast<int>(samples[i])) / 32768.0f;
                    self->levelPeak_ = std::max(self->levelPeak_, value);
                    if (self->recordSkip_ > 0) {
                        --self->recordSkip_;
                        continue;
                    }
                    if (self->recordCount_ >= self->recordBuffer_.size()) {
                        self->recordFull_ = true;  // 10 秒に達した。止めるのはメインスレッド
                        break;
                    }
                    self->recordBuffer_[self->recordCount_++] = samples[i];
                }
            }
            pw_stream_queue_buffer(self->recordStream_, buffer);
        };
        return e;
    }();
    return events;
}

const pw_stream_events& VoiceCheck::playEvents() {
    static const pw_stream_events events = [] {
        pw_stream_events e {};
        e.version = PW_VERSION_STREAM_EVENTS;
        e.state_changed = [](void* data, pw_stream_state, pw_stream_state state, const char* error) {
            if (state != PW_STREAM_STATE_ERROR) return;
            std::fprintf(stderr, "[声] 再生のストリームが失敗しました: %s\n", error != nullptr ? error : "?");
            static_cast<VoiceCheck*>(data)->playFailed_ = true;
        };
        e.process = [](void* data) {
            auto* self = static_cast<VoiceCheck*>(data);
            pw_buffer* buffer = pw_stream_dequeue_buffer(self->playStream_);
            if (buffer == nullptr) return;
            spa_data& d = buffer->buffer->datas[0];
            if (d.data == nullptr || !self->playClip_) {
                pw_stream_queue_buffer(self->playStream_, buffer);
                return;
            }
            uint64_t frames = d.maxsize / sizeof(int16_t);
            if (buffer->requested > 0) frames = std::min<uint64_t>(frames, buffer->requested);
            const std::vector<int16_t>& samples = self->playClip_->samples;
            const size_t left = samples.size() - std::min(self->playPos_, samples.size());
            const size_t count = std::min<size_t>(left, frames);
            if (count > 0) std::memcpy(d.data, samples.data() + self->playPos_, count * sizeof(int16_t));
            self->playPos_ += count;
            d.chunk->offset = 0;
            d.chunk->stride = sizeof(int16_t);
            d.chunk->size = static_cast<uint32_t>(count * sizeof(int16_t));
            pw_stream_queue_buffer(self->playStream_, buffer);
            // 全部渡したら、出し切るのを待つ（出し切ると drained が来る）
            if (self->playPos_ >= samples.size() && !self->playFlushed_) {
                self->playFlushed_ = true;
                self->playFlushedAt_ = nowSeconds();
                pw_stream_flush(self->playStream_, true);
            }
        };
        e.drained = [](void* data) { static_cast<VoiceCheck*>(data)->playDrained_ = true; };
        return e;
    }();
    return events;
}

pw_stream* VoiceCheck::connectStream(const char* name, bool capture) {
    pw_properties* props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY,
                                             capture ? "Capture" : "Playback", PW_KEY_NODE_NAME, name,
                                             PW_KEY_NODE_DESCRIPTION, "Frame Mic Tuner", PW_KEY_APP_NAME,
                                             "Frame Mic Tuner", nullptr);
    // node.virtual は付けない（録音は tracker にマイク使用中と数えてもらう）。target も付けない（既定の入力・出力）
    pw_stream* stream = pw_stream_new_simple(pw_thread_loop_get_loop(loop_), name, props,
                                             capture ? &recordEvents() : &playEvents(), this);
    if (stream == nullptr) return nullptr;

    uint8_t podBuffer[1024];
    spa_pod_builder builder;
    spa_pod_builder_init(&builder, podBuffer, sizeof(podBuffer));
    spa_audio_info_raw info {};
    info.format = SPA_AUDIO_FORMAT_S16;
    info.rate = kVoiceRate;
    info.channels = 1;
    info.position[0] = SPA_AUDIO_CHANNEL_MONO;
    const spa_pod* params[1] = {spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &info)};
    const auto flags = static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS);
    const int result =
        pw_stream_connect(stream, capture ? PW_DIRECTION_INPUT : PW_DIRECTION_OUTPUT, PW_ID_ANY, flags, params, 1);
    if (result < 0) {
        std::fprintf(stderr, "[声] pw_stream_connect に失敗: %s\n", std::strerror(-result));
        pw_stream_destroy(stream);
        return nullptr;
    }
    return stream;
}

void VoiceCheck::destroyStream(pw_stream*& stream) {
    if (stream == nullptr || loop_ == nullptr) return;
    pw_thread_loop_lock(loop_);
    pw_stream_destroy(stream);
    stream = nullptr;
    pw_thread_loop_unlock(loop_);
}

bool VoiceCheck::startRecording(const MicState& settings) {
    if (recordStream_ != nullptr) return true;
    stopPlayback();  // スピーカーから出した音を録らないように
    if (!ensureLoop()) {
        error_ = VoiceError::Record;
        return false;
    }
    pending_ = VoiceClip();
    pending_.recordedAt = std::time(nullptr);
    pending_.echoKnown = settings.echoKnown;
    pending_.echo = settings.echo;
    pending_.nsKnown = settings.nsKnown;
    pending_.ns = settings.ns;
    pending_.nsParamsKnown = settings.nsParams.vadKnown && settings.nsParams.graceKnown;
    pending_.nsVad = settings.nsParams.vad;
    pending_.nsGrace = settings.nsParams.grace;
    // 音の出口とマイク（履歴に「AB13X・エコー除去オフ」「AB13X のマイク（処理なし）」などと出す）
    const AudioDevices& devices = settings.devices;
    const AudioEndpoint* output = devices.findOutput(devices.defaultOutputKey);
    const AudioEndpoint* input = devices.findInput(devices.defaultInputKey);
    pending_.outputKnown = !devices.defaultOutputKey.empty();
    pending_.outputSpeaker = isBuiltinSpeaker(devices.defaultOutputKey);
    pending_.outputName = output != nullptr ? output->name : std::string();
    pending_.externalMic = !devices.defaultInputKey.empty() && !isBuiltinMic(devices.defaultInputKey);
    pending_.micName = input != nullptr ? input->name : std::string();
    recordBuffer_.assign(static_cast<size_t>(kVoiceMaxSec * kVoiceRate), 0);
    recordCount_ = 0;
    recordSkip_ = static_cast<size_t>(kWarmupSec * kVoiceRate);
    recordFull_ = false;
    recordFailed_ = false;
    levelPeak_ = 0.0f;

    pw_thread_loop_lock(loop_);
    recordStream_ = connectStream("frame-mic-tuner-record", true);
    pw_thread_loop_unlock(loop_);
    if (recordStream_ == nullptr) {
        error_ = VoiceError::Record;
        return false;
    }
    error_ = VoiceError::None;
    std::fprintf(stderr, "[声] 録音を始めました（既定の入力・最大 %.0f 秒）\n", kVoiceMaxSec);
    return true;
}

void VoiceCheck::stopRecording() {
    if (recordStream_ == nullptr) return;
    destroyStream(recordStream_);
    auto clip = std::make_shared<VoiceClip>(pending_);
    clip->samples.assign(recordBuffer_.begin(), recordBuffer_.begin() + static_cast<long>(recordCount_));
    recordBuffer_.clear();
    recordBuffer_.shrink_to_fit();
    if (clip->seconds() < kMinClipSec) {
        std::fprintf(stderr, "[声] 録音を止めました（短すぎるので残しません）\n");
        return;
    }
    clip->id = nextId_++;
    buildWave(*clip);
    std::fprintf(stderr, "[声] 録音を止めました（%.1f 秒・ピーク %.1f dBFS）\n", clip->seconds(), clip->peakDb);
    clips_.push_front(clip);
    while (clips_.size() > kVoiceHistory) {
        if (playingId() == clips_.back()->id) stopPlayback();
        clips_.pop_back();  // いちばん古いものを消す（メモリから消えるだけ）
    }
}

bool VoiceCheck::play(uint64_t id) {
    stopRecording();
    stopPlayback();
    std::shared_ptr<const VoiceClip> clip;
    for (const auto& c : clips_) {
        if (c->id == id) clip = c;
    }
    if (!clip) return false;
    if (!ensureLoop()) {
        error_ = VoiceError::Play;
        return false;
    }
    playClip_ = clip;
    playPos_ = 0;
    playFlushed_ = false;
    playDrained_ = false;
    playFailed_ = false;
    pw_thread_loop_lock(loop_);
    playStream_ = connectStream("frame-mic-tuner-play", false);
    pw_thread_loop_unlock(loop_);
    if (playStream_ == nullptr) {
        playClip_.reset();
        error_ = VoiceError::Play;
        return false;
    }
    error_ = VoiceError::None;
    std::fprintf(stderr, "[声] 再生を始めました（%.1f 秒・既定の出力）\n", clip->seconds());
    return true;
}

void VoiceCheck::stopPlayback() {
    if (playStream_ == nullptr) return;
    destroyStream(playStream_);
    playClip_.reset();
}

void VoiceCheck::shutdown() {
    const bool wasBusy = busy();
    stopRecording();
    stopPlayback();
    if (loop_ != nullptr) {
        pw_thread_loop_stop(loop_);
        pw_thread_loop_destroy(loop_);
        loop_ = nullptr;
        std::fprintf(stderr, "[声] PipeWire のストリームとスレッドを片付けました%s\n", wasBusy ? "（録音・再生を止めました）" : "");
    }
}

bool VoiceCheck::update() {
    if (loop_ == nullptr) return false;
    bool full = false;
    bool recordFailed = false;
    bool playDone = false;
    bool playFailed = false;
    pw_thread_loop_lock(loop_);
    full = recordStream_ != nullptr && recordFull_;
    recordFailed = recordStream_ != nullptr && recordFailed_;
    if (playStream_ != nullptr) {
        playDone = playDrained_ || (playFlushed_ && nowSeconds() - playFlushedAt_ > kDrainTimeoutSec);
        playFailed = playFailed_;
    }
    pw_thread_loop_unlock(loop_);

    bool changed = false;
    if (full || recordFailed) {
        stopRecording();
        if (recordFailed) error_ = VoiceError::Record;
        changed = true;
    }
    if (playDone || playFailed) {
        if (playDone) std::fprintf(stderr, "[声] 再生が終わりました\n");
        stopPlayback();
        if (playFailed) error_ = VoiceError::Play;
        changed = true;
    }
    return changed;
}

VoiceView VoiceCheck::view() {
    VoiceView view;
    view.clips.assign(clips_.begin(), clips_.end());
    view.error = error_;
    if (loop_ == nullptr) return view;
    pw_thread_loop_lock(loop_);
    view.recording = recordStream_ != nullptr;
    view.recordSec = static_cast<double>(recordCount_) / kVoiceRate;
    view.levelDb = peakToDb(levelPeak_);
    levelPeak_ = 0.0f;
    view.playing = playStream_ != nullptr && playClip_ != nullptr;
    if (view.playing) {
        view.playingId = playClip_->id;
        view.playSec = static_cast<double>(std::min(playPos_, playClip_->samples.size())) / kVoiceRate;
    }
    pw_thread_loop_unlock(loop_);
    return view;
}
