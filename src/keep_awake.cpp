// 音の頭切れを防ぐ音の実装。
#include "keep_awake.h"

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/pod/builder.h>

#include <pthread.h>
#include <signal.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

/** ストリームのノード名（pw-dump・pw-top で見分ける）。 */
constexpr const char* kNodeName = "frame-mic-tuner-keep-awake";
/** ストリームの名前（media.name）。 */
constexpr const char* kMediaName = "Frame Mic Tuner keep-awake";
/** 続けて失敗したときに、ログを出す回数（そのあとは止めたことだけ）。 */
constexpr int kLoggedFailures = 3;

/**
 * PipeWire のライブラリを 1 回だけ初期化する（声のチェックも呼ぶが、pw_init は何回呼んでもよい）。
 */
void initPipeWireOnce() {
    static bool done = false;
    if (done) return;
    pw_init(nullptr, nullptr);
    done = true;
}

}  // namespace

std::string keepAwakeTarget(const AudioDevices& devices, const std::map<std::string, OutputProfile>& outputs) {
    if (!devices.defaultsKnown || devices.defaultSinkName.empty()) return "";
    const std::string& key = devices.defaultOutputKey;
    // フィルターや仮想の出口、Frame のスピーカー（アダプターが無い）には流さない
    if (key.empty() || !isDeviceOutputKey(key) || isBuiltinSpeaker(key)) return "";
    const auto it = outputs.find(key);
    const bool on = it != outputs.end() ? it->second.keepAwake : keepAwakeDefault(key);
    return on ? devices.defaultSinkName : "";
}

KeepAwake::~KeepAwake() {
    shutdown();
}

bool KeepAwake::ensureLoop() {
    if (loop_ != nullptr) return true;
    initPipeWireOnce();
    loop_ = pw_thread_loop_new("frame-mic-tuner-keep-awake", nullptr);
    if (loop_ == nullptr) {
        if (failures_ < kLoggedFailures) std::fprintf(stderr, "[頭切れ防止] PipeWire のスレッドを作れません\n");
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
        if (failures_ < kLoggedFailures) std::fprintf(stderr, "[頭切れ防止] PipeWire のスレッドを始められません\n");
        pw_thread_loop_destroy(loop_);
        loop_ = nullptr;
        return false;
    }
    return true;
}

const pw_stream_events& KeepAwake::events() {
    static const pw_stream_events events = [] {
        pw_stream_events e {};
        e.version = PW_VERSION_STREAM_EVENTS;
        e.state_changed = [](void* data, pw_stream_state, pw_stream_state state, const char* error) {
            // 流れ始めた: 続けて失敗した回数を 0 に戻してもらう（ログをまた出せるように）
            if (state == PW_STREAM_STATE_STREAMING) {
                static_cast<KeepAwake*>(data)->streaming_ = true;
                return;
            }
            // 失敗と切断（出口が消えた・PipeWire が止まった）。止めるのはメインスレッド。
            // 自分で壊したときも UNCONNECTED が来るが、そのときは先に stream_ を空にしてあるので数えない
            if (state != PW_STREAM_STATE_ERROR && state != PW_STREAM_STATE_UNCONNECTED) return;
            auto* self = static_cast<KeepAwake*>(data);
            if (self->stream_ == nullptr) return;
            self->failReason_ = std::string(state == PW_STREAM_STATE_ERROR ? "失敗: " : "切断: ") + (error != nullptr ? error : "?");
            self->failed_ = true;
        };
        e.process = [](void* data) {
            auto* self = static_cast<KeepAwake*>(data);
            pw_buffer* buffer = pw_stream_dequeue_buffer(self->stream_);
            if (buffer == nullptr) return;
            spa_data& d = buffer->buffer->datas[0];
            if (d.data == nullptr || d.chunk == nullptr) {
                pw_stream_queue_buffer(self->stream_, buffer);
                return;
            }
            // 5 Hz のサインを左右同じ値で。位相は呼び出しをまたいでつなぐ（つなぎ目で音が跳ねない）
            const uint32_t stride = sizeof(float) * kKeepAwakeChannels;
            uint64_t frames = d.maxsize / stride;
            if (buffer->requested > 0) frames = std::min<uint64_t>(frames, buffer->requested);
            auto* out = static_cast<float*>(d.data);
            const double step = 2.0 * M_PI * kKeepAwakeHz / kKeepAwakeRate;
            for (uint64_t i = 0; i < frames; ++i) {
                const float value = static_cast<float>(kKeepAwakeAmplitude * std::sin(self->phase_));
                for (int c = 0; c < kKeepAwakeChannels; ++c) out[i * kKeepAwakeChannels + c] = value;
                self->phase_ += step;
                if (self->phase_ >= 2.0 * M_PI) self->phase_ -= 2.0 * M_PI;
            }
            d.chunk->offset = 0;
            d.chunk->stride = static_cast<int32_t>(stride);
            d.chunk->size = static_cast<uint32_t>(frames * stride);
            pw_stream_queue_buffer(self->stream_, buffer);
        };
        return e;
    }();
    return events;
}

bool KeepAwake::start() {
    if (!ensureLoop()) return false;
    // target.object で既定の出力のノードへ向け、node.dont-reconnect でほかの出口へ勝手に移らないようにする。
    // node.dont-fallback は、その出口が消えた直後（見張りが気づくまで）に作り直したとき、既定の出口へつながないため
    pw_properties* props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, "Playback", PW_KEY_MEDIA_NAME, kMediaName, PW_KEY_NODE_NAME,
        kNodeName, PW_KEY_NODE_DESCRIPTION, kMediaName, PW_KEY_APP_NAME, "Frame Mic Tuner", PW_KEY_TARGET_OBJECT,
        target_.c_str(), PW_KEY_NODE_DONT_RECONNECT, "true", "node.dont-fallback", "true", nullptr);
    pw_thread_loop_lock(loop_);
    pw_stream* stream = pw_stream_new_simple(pw_thread_loop_get_loop(loop_), kMediaName, props, &events(), this);
    int result = -EINVAL;
    if (stream != nullptr) {
        uint8_t podBuffer[1024];
        spa_pod_builder builder;
        spa_pod_builder_init(&builder, podBuffer, sizeof(podBuffer));
        spa_audio_info_raw info {};
        info.format = SPA_AUDIO_FORMAT_F32;
        info.rate = kKeepAwakeRate;
        info.channels = kKeepAwakeChannels;
        info.position[0] = SPA_AUDIO_CHANNEL_FL;
        info.position[1] = SPA_AUDIO_CHANNEL_FR;
        const spa_pod* params[1] = {spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &info)};
        const auto flags = static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS);
        stream_ = stream;  // process はループのロックを持って呼ばれるので、つなぐ前に入れておく
        result = pw_stream_connect(stream, PW_DIRECTION_OUTPUT, PW_ID_ANY, flags, params, 1);
        if (result < 0) {
            stream_ = nullptr;
            pw_stream_destroy(stream);
        }
    }
    failed_ = false;
    streaming_ = false;
    failReason_.clear();
    pw_thread_loop_unlock(loop_);
    if (stream_ == nullptr) {
        if (failures_ < kLoggedFailures) {
            std::fprintf(stderr, "[頭切れ防止] %s へのストリームを作れません: %s\n", target_.c_str(),
                         stream == nullptr ? "pw_stream_new_simple" : std::strerror(-result));
        }
        return false;
    }
    if (failures_ < kLoggedFailures) std::fprintf(stderr, "[頭切れ防止] %s へ聞こえない音を流し始めました\n", target_.c_str());
    return true;
}

std::string KeepAwake::stop() {
    if (stream_ == nullptr || loop_ == nullptr) return "";
    pw_thread_loop_lock(loop_);
    pw_stream* stream = stream_;
    stream_ = nullptr;  // 壊す途中の state_changed（UNCONNECTED）を失敗と数えない
    pw_stream_destroy(stream);
    std::string reason;
    reason.swap(failReason_);
    failed_ = false;
    pw_thread_loop_unlock(loop_);
    return reason;
}

void KeepAwake::update(const std::string& target, double now) {
    if (target != target_) {
        // 先が変わった（既定の出力が変わった・設定を変えた）: 止めて、すぐ向け直す
        if (stream_ != nullptr) std::fprintf(stderr, "[頭切れ防止] %s へ流すのを止めました\n", target_.c_str());
        stop();
        target_ = target;
        retryAt_ = now;
        failures_ = 0;
    }
    if (stream_ != nullptr && streaming_.exchange(false)) failures_ = 0;
    if (stream_ != nullptr && failed_) {
        // 失敗・切断: 止めて、次の見回り（2 秒後）でまた試す
        const std::string reason = stop();
        ++failures_;
        retryAt_ = now + kRetrySec;
        if (failures_ <= kLoggedFailures) {
            std::fprintf(stderr, "[頭切れ防止] %s へのストリームを止めました（%s）。%.0f 秒後にまた試します%s\n", target_.c_str(),
                         reason.c_str(), kRetrySec, failures_ == kLoggedFailures ? "（続くので、ここからはログを省きます）" : "");
        }
    }
    if (stream_ == nullptr && !target_.empty() && now >= retryAt_) {
        if (!start()) {
            ++failures_;
            retryAt_ = now + kRetrySec;
        }
    }
}

void KeepAwake::shutdown() {
    if (stream_ != nullptr) std::fprintf(stderr, "[頭切れ防止] %s へ流すのを止めました（終了）\n", target_.c_str());
    stop();
    target_.clear();
    if (loop_ != nullptr) {
        pw_thread_loop_stop(loop_);
        pw_thread_loop_destroy(loop_);
        loop_ = nullptr;
    }
}
