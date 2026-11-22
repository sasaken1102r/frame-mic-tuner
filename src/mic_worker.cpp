// ワーカースレッドの実装。
#include "mic_worker.h"

#include <signal.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

MicCommand MicCommand::applySettings(bool echo, NoiseMode mode, bool withNsParams, double vad, double grace,
                                     double steadyDb, bool onlyIfChanged) {
    MicCommand command {Kind::ApplySettings};
    command.value = echo;
    command.ns = mode == NoiseMode::Valve;
    command.steady = mode == NoiseMode::Steady;
    command.withNsParams = withNsParams;
    command.vad = clampNsVad(vad);
    command.grace = clampNsGrace(grace);
    command.steadyDb = clampSteadyDb(steadyDb);
    command.onlyIfChanged = onlyIfChanged;
    return command;
}

MicCommand MicCommand::noiseMode(NoiseMode mode) {
    MicCommand command {Kind::SetNoiseMode};
    command.ns = mode == NoiseMode::Valve;
    command.steady = mode == NoiseMode::Steady;
    return command;
}

MicCommand MicCommand::setDefault(bool output, const AudioEndpoint& endpoint) {
    MicCommand command {output ? Kind::SetDefaultOutput : Kind::SetDefaultInput};
    command.nodeId = endpoint.nodeId;
    command.nodeName = endpoint.nodeName;
    return command;
}

namespace {

/**
 * 出口とマイクの一覧を読む。pw-dump を読めなければ、既定の名前だけでも pw-metadata から入れておく
 * （名前の決まりでキーを作る。出口の設定はそれで続けられる）。
 * @return 一覧
 */
AudioDevices readDevicesWithFallback() {
    AudioDevices devices = readAudioDevices();
    if (devices.known && devices.defaultsKnown) return devices;
    std::string sink;
    std::string source;
    if (readDefaultNames(sink, source)) {
        devices.defaultsKnown = true;
        devices.defaultSinkName = sink;
        devices.defaultSourceName = source;
        devices.defaultOutputKey = sink.empty() ? std::string() : endpointKey(sink);
        devices.defaultInputKey = source.empty() ? std::string() : endpointKey(source);
    }
    return devices;
}

/**
 * ノイズ除去の種類を書く。オフにする方を先に書き、Valve のノイズ除去と一定の雑音を消すが同時にオンにならないようにする。
 * @param ns Valve のノイズ除去
 * @param steady 一定の雑音を消す
 * @param writeNs ノイズ除去を書くか
 * @param writeSteady 一定の雑音を消すを書くか（設定が無い = Frame をまだ再起動していないときは書かない）
 * @param nsOk ノイズ除去を書けたか（書かなければ true）
 * @param steadyOk 一定の雑音を消すを書けたか（書かなければ true）
 */
void writeNoiseMode(bool ns, bool steady, bool writeNs, bool writeSteady, bool& nsOk, bool& steadyOk) {
    nsOk = true;
    steadyOk = true;
    if (steady) {
        if (writeNs) nsOk = writeSetting(kNoiseSuppressionKey, ns);
        if (writeSteady) steadyOk = writeSetting(kSteadyNoiseKey, steady);
    } else {
        if (writeSteady) steadyOk = writeSetting(kSteadyNoiseKey, steady);
        if (writeNs) nsOk = writeSetting(kNoiseSuppressionKey, ns);
    }
}

}  // namespace

MicWorker::~MicWorker() {
    stop();
}

void MicWorker::start() {
    if (thread_.joinable()) return;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = false;
    }
    thread_ = std::thread([this] { run(); });
}

void MicWorker::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    wake_.notify_all();
    if (thread_.joinable()) thread_.join();
}

void MicWorker::setActive(bool active) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (active == active_) return;
        active_ = active;
        if (active) refreshNow_ = true;
    }
    wake_.notify_all();
}

void MicWorker::refreshDevicesNow() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        devicesNow_ = true;
    }
    wake_.notify_all();
}

uint64_t MicWorker::request(MicCommand command) {
    uint64_t ticket = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (command.kind == MicCommand::Kind::SetNsParams || command.kind == MicCommand::Kind::SetSteadyDb) {
            // バーのドラッグ中の連打は、まだ実行していない前の値を捨てて最後の値だけにする（捨てた分は終わった扱い）
            const MicCommand::Kind kind = command.kind;
            const auto end = std::remove_if(queue_.begin(), queue_.end(), [kind](const MicCommand& c) {
                return c.kind == kind;
            });
            completed_ += static_cast<uint64_t>(queue_.end() - end);
            queue_.erase(end, queue_.end());
        }
        queue_.push_back(command);
        ticket = ++requested_;
    }
    wake_.notify_all();
    return ticket;
}

uint64_t MicWorker::completed() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return completed_;
}

void MicWorker::setDesiredNsParams(double vad, double grace) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        hasDesiredNs_ = true;
        desiredVad_ = clampNsVad(vad);
        desiredGrace_ = clampNsGrace(grace);
        appliedNodeId_ = -1;  // まだかけていない
    }
    wake_.notify_all();
}

uint64_t MicWorker::snapshot(MicState& state) const {
    std::lock_guard<std::mutex> lock(mutex_);
    state = state_;
    return version_;
}

uint64_t MicWorker::version() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return version_;
}

bool MicWorker::execute(const MicCommand& command, int nodeId) {
    switch (command.kind) {
        case MicCommand::Kind::SetEcho: return writeSetting(kEchoCancelKey, command.value);
        case MicCommand::Kind::SetNs: return writeSetting(kNoiseSuppressionKey, command.value);
        case MicCommand::Kind::SetAutostart: return writeAutostart(command.value);
        case MicCommand::Kind::SetNsParams: {
            if (nodeId < 0) nodeId = readNsParams().nodeId;
            return writeNsParams(nodeId, command.vad, command.grace);
        }
        case MicCommand::Kind::SetSteadyDb: {
            if (nodeId < 0) nodeId = readSteadyParams().nodeId;
            return writeSteadyParams(nodeId, command.steadyDb);
        }
        case MicCommand::Kind::SetMute: return writeMute(command.value);
        case MicCommand::Kind::SetDefaultOutput: return writeDefaultNode(command.nodeId, command.nodeName, true);
        case MicCommand::Kind::SetDefaultInput: return writeDefaultNode(command.nodeId, command.nodeName, false);
        case MicCommand::Kind::ApplySettings:  // run() で書き込みを 1 つずつ扱う
        case MicCommand::Kind::SetNoiseMode: break;
    }
    return false;
}

void MicWorker::run() {
    // SIGTERM・SIGINT・SIGUSR1 はメインスレッドで受ける（子プロセスにはマスクを持ち越さない。command.cpp）
    sigset_t blocked;
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGTERM);
    sigaddset(&blocked, SIGINT);
    sigaddset(&blocked, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &blocked, nullptr);

    using Clock = std::chrono::steady_clock;
    /**
     * 今から seconds 秒後の時刻。
     */
    const auto after = [](double seconds) {
        return Clock::now() + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(seconds));
    };
    /**
     * 読んだ状態を覚える（mutex を持って呼ぶ）。画面に出す中身が変わったときだけ番号を進めて、描き直させる。
     */
    const auto store = [this](MicState fresh) {
        if (hasDesiredNs_ && hiddenNs_.nodeId >= 0) {
            const int hiddenNode = hiddenNs_.nodeId;
            showHiddenNs(fresh.nsParams, hiddenNs_, desiredVad_, desiredGrace_);
            if (hiddenNs_.nodeId < 0) {
                std::fprintf(stderr, "[ノイズ除去] ノード %d の読み返しが変わったので、読んだ値を出します\n", hiddenNode);
            }
        }
        if (hasDesiredSteady_ && hiddenSteady_.nodeId >= 0) {
            const int hiddenNode = hiddenSteady_.nodeId;
            showHiddenSteady(fresh.steadyParams, hiddenSteady_, desiredSteadyDb_);
            if (hiddenSteady_.nodeId < 0) {
                std::fprintf(stderr, "[一定の雑音] ノード %d の読み返しが変わったので、読んだ値を出します\n", hiddenNode);
            }
        }
        fresh.writesDone = completed_;
        fresh.nsApplied = !hasDesiredNs_ || (fresh.nsParams.nodeKnown && appliedNodeId_ == fresh.nsParams.nodeId);
        fresh.steadyApplied =
            !hasDesiredSteady_ || (fresh.steadyParams.nodeKnown && appliedSteadyNodeId_ == fresh.steadyParams.nodeId);
        if (!sameMicState(fresh, state_)) ++version_;
        state_ = fresh;
    };
    /**
     * 書いた直後に読んだ値で、書いた値が隠れているかを覚える（mutex を持って呼ぶ）。
     * @param read 書いた直後に読んだ値
     * @param nodeId 書いたノードの id
     * @param vad 書いた判定の厳しさ（%）
     * @param grace 書いた余韻（ms）
     */
    const auto noteWrite = [this](const NsParams& read, int nodeId, double vad, double grace) {
        const bool wasHidden = hiddenNs_.nodeId >= 0 && hiddenNs_.nodeId == nodeId;
        const NsHiddenWrite hidden = noteNsWrite(read, nodeId, vad, grace);
        // 隠れたままもう一度書いた（バーを動かしている途中）ときは、最初に見た前の値を持ち続ける
        if (!(wasHidden && hidden.nodeId >= 0)) hiddenNs_ = hidden;
        if (hidden.nodeId >= 0 && !wasHidden) {
            std::fprintf(stderr,
                         "[ノイズ除去] ノード %d の読み返しが前の値（%.0f%% / %.0fms）のままです。PipeWire はマイクを使い始めると"
                         "書いた値をかけるので、それまでは書いた値を出します\n",
                         nodeId, hidden.staleVad, hidden.staleGrace);
        }
    };
    /**
     * 一定の雑音を消す強さで、書いた値が隠れているかを覚える（noteWrite と同じ。mutex を持って呼ぶ）。
     * @param read 書いた直後に読んだ値
     * @param nodeId 書いたノードの id
     * @param db 書いた強さ（dB）
     */
    const auto noteSteady = [this](const SteadyParams& read, int nodeId, double db) {
        const bool wasHidden = hiddenSteady_.nodeId >= 0 && hiddenSteady_.nodeId == nodeId;
        const SteadyHiddenWrite hidden = noteSteadyWrite(read, nodeId, db);
        if (!(wasHidden && hidden.nodeId >= 0)) hiddenSteady_ = hidden;
        if (hidden.nodeId >= 0 && !wasHidden) {
            std::fprintf(stderr,
                         "[一定の雑音] ノード %d の読み返しが前の値（%.0f dB）のままです。PipeWire はマイクを使い始めると"
                         "書いた値をかけるので、それまでは書いた値を出します\n",
                         nodeId, hidden.staleDb);
        }
    };
    /**
     * 一定の雑音を消す強さの「かけたい値」があり、ノードがまだかけていないもの（見つかったばかり・サービスが起動し直した）なら、
     * かけて読み直す。mutex を持って呼び、実行の間だけ外す。
     * @param params 読んだばかりの値（かけたら読み直した値に置き換える）
     * @param lock 持っている mutex
     */
    const auto applyDesiredSteadyIfNeeded = [this, &noteSteady](SteadyParams& params, std::unique_lock<std::mutex>& lock) {
        if (!hasDesiredSteady_ || !params.nodeKnown || params.nodeId == appliedSteadyNodeId_) return;
        const int nodeId = params.nodeId;
        const double db = desiredSteadyDb_;
        lock.unlock();
        std::fprintf(stderr, "[一定の雑音] ノード %d に保存した値をかけます（%.0f dB）\n", nodeId, db);
        const bool ok = writeSteadyParams(nodeId, db);
        SteadyParams fresh = ok ? readSteadyParams() : params;
        lock.lock();
        if (ok) {
            appliedSteadyNodeId_ = nodeId;
            noteSteady(fresh, nodeId, db);
        }
        params = fresh;
    };
    /**
     * かけたい値があり、ノードがまだかけていないもの（見つかったばかり・作り直された）なら、かけて読み直す。
     * mutex を持って呼び、実行の間だけ外す。
     * @param params 読んだばかりのノイズ除去の値（かけたら読み直した値に置き換える）
     * @param lock 持っている mutex
     */
    const auto applyDesiredIfNeeded = [this, &noteWrite](NsParams& params, std::unique_lock<std::mutex>& lock) {
        if (!hasDesiredNs_ || !params.nodeKnown || params.nodeId == appliedNodeId_) return;
        const int nodeId = params.nodeId;
        const double vad = desiredVad_;
        const double grace = desiredGrace_;
        lock.unlock();
        std::fprintf(stderr, "[ノイズ除去] ノード %d に保存した値をかけます（%.0f%% / %.0fms）\n", nodeId, vad, grace);
        const bool ok = writeNsParams(nodeId, vad, grace);
        NsParams fresh = ok ? readNsParams() : params;
        lock.lock();
        if (ok) {
            appliedNodeId_ = nodeId;
            noteWrite(fresh, nodeId, vad, grace);
        }
        params = fresh;
    };

    Clock::time_point nextRefresh = Clock::now();
    Clock::time_point nextAutostart = Clock::now();
    Clock::time_point nextApplyTry = Clock::now();
    Clock::time_point nextDevices = Clock::now();
    Clock::time_point nextDefaultPoll = after(kDefaultPollSec);
    bool initial = true;  // 始めた直後に 1 回、設定と一覧をまとめて読む（メインが今の出口の設定をかけるため）
    int settingsRetries = 10;  // 始めた直後に設定を読めなかったときに、既定を見るついでに読み直す残りの回数
    std::unique_lock<std::mutex> lock(mutex_);
    while (!stop_) {
        if (!queue_.empty()) {
            // 書き込み → すぐ読み直し
            const MicCommand command = queue_.front();
            queue_.pop_front();
            const bool isNsParams = command.kind == MicCommand::Kind::SetNsParams;
            const bool isSteadyDb = command.kind == MicCommand::Kind::SetSteadyDb;
            const bool isApplyParams = command.kind == MicCommand::Kind::ApplySettings && command.withNsParams;
            const int nsNodeId = state_.nsParams.nodeKnown ? state_.nsParams.nodeId : -1;
            const int steadyNodeId = state_.steadyParams.nodeKnown ? state_.steadyParams.nodeId : -1;
            const int nodeId = isSteadyDb ? steadyNodeId : nsNodeId;
            if (isNsParams || isApplyParams) {
                // 書いた値が、これからの「かけたい値」になる
                hasDesiredNs_ = true;
                desiredVad_ = clampNsVad(command.vad);
                desiredGrace_ = clampNsGrace(command.grace);
                if (!isNsParams) appliedNodeId_ = -1;
            }
            if (isSteadyDb || isApplyParams) {
                hasDesiredSteady_ = true;
                desiredSteadyDb_ = clampSteadyDb(command.steadyDb);
                if (!isSteadyDb) appliedSteadyNodeId_ = -1;
            }
            const MicState before = state_;
            const NsHiddenWrite hidden = hiddenNs_;
            const SteadyHiddenWrite hiddenSteady = hiddenSteady_;
            lock.unlock();
            if (command.kind == MicCommand::Kind::SetNoiseMode) {
                // ノイズ除去の種類（パネル）: オフにする方を先に書く。一定の雑音を消すの設定が無い（Frame をまだ再起動していない）
                // ときは、そちらは書かない（オンにしたいときは失敗）
                bool nsOk = true;
                bool steadyOk = true;
                writeNoiseMode(command.ns, command.steady, true, !before.steadyMissing, nsOk, steadyOk);
                if (before.steadyMissing && command.steady) {
                    std::fprintf(stderr, "[一定の雑音] 設定 %s が無いので、オンにできません（Frame の再起動が要ります）\n",
                                 kSteadyNoiseKey);
                    steadyOk = false;
                }
                MicState fresh = readMicState(true);
                fresh.devices = before.devices;
                lock.lock();
                fresh.writeError = !steadyOk ? MicError::WriteSteady : (!nsOk ? MicError::WriteNs : MicError::None);
                ++completed_;
                store(fresh);
                nextRefresh = after(kRefreshSec);
                nextAutostart = after(kAutostartRefreshSec);
                continue;
            }
            if (command.kind == MicCommand::Kind::ApplySettings) {
                // 出口の設定: エコー除去 → ノイズ除去 → 強さの順に書き、書き終わってから 1 回だけ読み直す
                // （途中の値を表示しない）。片方が失敗しても、ほかは書く。自動の切り替えでは、今と同じものは書かない
                const bool skip = command.onlyIfChanged;
                bool echoOk = true;
                bool nsOk = true;
                bool steadyOk = true;
                bool paramsOk = true;
                int appliedNode = -1;
                bool wroteParams = false;
                int appliedSteadyNode = -1;
                bool wroteSteady = false;
                if (!skip || !before.echoKnown || before.echo != command.value) {
                    echoOk = writeSetting(kEchoCancelKey, command.value);
                }
                // ノイズ除去の種類: オフにする方を先に書く。一定の雑音を消すの設定が無い（Frame をまだ再起動していない）ときは
                // そちらは書かず、Valve のノイズ除去だけ書く（一定の雑音を消すにしたい出口は、使えるようになるまでオフ）
                const bool writeNs = !skip || !before.nsKnown || before.ns != command.ns;
                const bool writeSteady =
                    !before.steadyMissing && (!skip || !before.steadyKnown || before.steady != command.steady);
                if (before.steadyMissing && command.steady) {
                    std::fprintf(stderr, "[一定の雑音] 設定 %s が無いので、一定の雑音を消すはかけません（Frame の再起動が要ります）\n",
                                 kSteadyNoiseKey);
                }
                writeNoiseMode(command.ns, command.steady, writeNs, writeSteady, nsOk, steadyOk);
                if (command.withNsParams) {
                    const NsParams now = readNsParams();
                    if (now.nodeKnown) {
                        if (!needNsWrite(now, command.vad, command.grace, skip, hidden)) {
                            appliedNode = now.nodeId;
                        } else {
                            paramsOk = writeNsParams(now.nodeId, command.vad, command.grace);
                            if (paramsOk) appliedNode = now.nodeId;
                            wroteParams = paramsOk;
                        }
                    }
                    // ノードがまだ無いときは、見つかりしだいかける（起動直後の 5 秒おきの試し）
                    const SteadyParams steadyNow = readSteadyParams();
                    if (steadyNow.nodeKnown) {
                        if (!needSteadyWrite(steadyNow, command.steadyDb, skip, hiddenSteady)) {
                            appliedSteadyNode = steadyNow.nodeId;
                        } else {
                            const bool ok = writeSteadyParams(steadyNow.nodeId, command.steadyDb);
                            if (ok) appliedSteadyNode = steadyNow.nodeId;
                            wroteSteady = ok;
                            paramsOk = paramsOk && ok;
                        }
                    }
                    // frame-mic-tuner-denoise.service が動いていないときは、動きだしてノードが見つかりしだいかける
                }
                MicState fresh = readMicState(true);
                fresh.devices = before.devices;
                lock.lock();
                if (appliedNode >= 0) appliedNodeId_ = appliedNode;
                if (wroteParams) noteWrite(fresh.nsParams, appliedNode, command.vad, command.grace);
                if (appliedSteadyNode >= 0) appliedSteadyNodeId_ = appliedSteadyNode;
                if (wroteSteady) noteSteady(fresh.steadyParams, appliedSteadyNode, command.steadyDb);
                const bool noiseOk = nsOk && steadyOk;
                if (echoOk && noiseOk) {
                    fresh.writeError = paramsOk ? MicError::None : MicError::WriteNsParams;
                } else if (!echoOk && !noiseOk) {
                    fresh.writeError = MicError::WriteSettings;
                } else if (!echoOk) {
                    fresh.writeError = MicError::WriteEcho;
                } else {
                    fresh.writeError = nsOk ? MicError::WriteSteady : MicError::WriteNs;
                }
                ++completed_;
                store(fresh);
                nextRefresh = after(kRefreshSec);
                nextAutostart = after(kAutostartRefreshSec);
                continue;
            }
            const bool ok = execute(command, nodeId);
            const bool isDefault = command.kind == MicCommand::Kind::SetDefaultOutput ||
                                   command.kind == MicCommand::Kind::SetDefaultInput;
            MicState fresh;
            if (isNsParams) {
                // 強さを変えたときは、その値だけ読み直す（バーを動かしている間に重いものを読まない）
                fresh = before;
                fresh.nsParams = readNsParams();
            } else if (isSteadyDb) {
                fresh = before;
                fresh.steadyParams = readSteadyParams();
            } else {
                fresh = readMicState(true);
                // 既定を切り替えたときは、一覧も読み直す（メインが新しい出口の設定をかける）
                fresh.devices = isDefault ? readDevicesWithFallback() : before.devices;
            }
            lock.lock();
            if (isNsParams && ok && fresh.nsParams.nodeKnown) {
                appliedNodeId_ = fresh.nsParams.nodeId;
                noteWrite(fresh.nsParams, nodeId >= 0 ? nodeId : fresh.nsParams.nodeId, command.vad, command.grace);
            }
            if (isSteadyDb && ok && fresh.steadyParams.nodeKnown) {
                appliedSteadyNodeId_ = fresh.steadyParams.nodeId;
                noteSteady(fresh.steadyParams, nodeId >= 0 ? nodeId : fresh.steadyParams.nodeId, command.steadyDb);
            }
            if (ok) {
                fresh.writeError = MicError::None;
            } else {
                switch (command.kind) {
                    case MicCommand::Kind::SetNsParams: fresh.writeError = MicError::WriteNsParams; break;
                    case MicCommand::Kind::SetSteadyDb: fresh.writeError = MicError::WriteSteadyDb; break;
                    case MicCommand::Kind::SetMute:
                        fresh.writeError = command.value ? MicError::WriteMuteOn : MicError::WriteMute;
                        break;
                    case MicCommand::Kind::SetAutostart: fresh.writeError = MicError::WriteAutostart; break;
                    case MicCommand::Kind::SetDefaultOutput: fresh.writeError = MicError::WriteOutput; break;
                    case MicCommand::Kind::SetDefaultInput: fresh.writeError = MicError::WriteInput; break;
                    default: fresh.writeError = MicError::WriteSettings; break;
                }
            }
            ++completed_;
            store(fresh);
            if (!isNsParams && !isSteadyDb) {
                nextRefresh = after(kRefreshSec);
                nextAutostart = after(kAutostartRefreshSec);
            }
            if (isDefault) nextDevices = after(kDevicesRefreshSec);
            continue;
        }
        const Clock::time_point now = Clock::now();
        if (initial || refreshNow_ || (active_ && now >= nextRefresh)) {
            // 自動起動の状態（systemctl）はめったに変わらないので、開いた直後と 5 秒おきだけ読む。
            // 出口とマイクの一覧（pw-dump）は、開いた直後と 5 秒おき・頼まれたとき
            const bool withAutostart = initial || refreshNow_ || now >= nextAutostart;
            const bool withDevices = initial || refreshNow_ || devicesNow_ || now >= nextDevices;
            initial = false;
            refreshNow_ = false;
            devicesNow_ = false;
            const AudioDevices previousDevices = state_.devices;
            lock.unlock();
            MicState fresh = readMicState(withAutostart);
            fresh.devices = withDevices ? readDevicesWithFallback() : previousDevices;
            lock.lock();
            // ノードが作り直されていたら（id が変わった）、保存した値をかけ直す
            applyDesiredIfNeeded(fresh.nsParams, lock);
            applyDesiredSteadyIfNeeded(fresh.steadyParams, lock);
            if (withAutostart) {
                nextAutostart = after(kAutostartRefreshSec);
            } else {
                fresh.autostart = state_.autostart;
            }
            if (withDevices) {
                nextDevices = after(kDevicesRefreshSec);
                nextDefaultPoll = after(kDefaultPollSec);
            }
            fresh.writeError = state_.writeError;  // 書き込みの失敗は、次に書き込みが成功するまで出しておく
            store(fresh);
            nextRefresh = after(kRefreshSec);
            continue;
        }
        if (devicesNow_) {
            // 選ぶ画面を開いた: 一覧だけ読み直す
            devicesNow_ = false;
            lock.unlock();
            const AudioDevices devices = readDevicesWithFallback();
            lock.lock();
            MicState fresh = state_;
            fresh.devices = devices;
            store(fresh);
            nextDevices = after(kDevicesRefreshSec);
            nextDefaultPoll = after(kDefaultPollSec);
            continue;
        }
        if (now >= nextDefaultPoll) {
            // 既定の出力・入力を見る（閉じている間も 2 秒おき。軽い pw-metadata だけ）。変わっていたら、一覧と今の設定を読む
            nextDefaultPoll = after(kDefaultPollSec);
            lock.unlock();
            std::string sink;
            std::string source;
            const bool ok = readDefaultNames(sink, source);
            lock.lock();
            const AudioDevices& known = state_.devices;
            const bool same = known.defaultsKnown && sink == known.defaultSinkName && source == known.defaultSourceName;
            // 起動した直後に WirePlumber がまだで設定を読めなかったときは、しばらく（最大 10 回）読み直す
            const bool retry = settingsRetries > 0 && (!state_.echoKnown || !state_.nsKnown) &&
                               state_.readError != MicError::NotInstalled;
            if (!ok || (same && !retry)) continue;
            if (same) {
                --settingsRetries;
                std::fprintf(stderr, "[マイク] 設定をまだ読めていないので、読み直します\n");
            } else {
                std::fprintf(stderr, "[出口] 既定が変わりました: 出力 %s・入力 %s\n", sink.empty() ? "（なし）" : sink.c_str(),
                             source.empty() ? "（なし）" : source.c_str());
            }
            lock.unlock();
            MicState fresh = readMicState(false);
            fresh.devices = readDevicesWithFallback();
            lock.lock();
            applyDesiredIfNeeded(fresh.nsParams, lock);
            applyDesiredSteadyIfNeeded(fresh.steadyParams, lock);
            fresh.autostart = state_.autostart;
            fresh.writeError = state_.writeError;
            store(fresh);
            nextDevices = after(kDevicesRefreshSec);
            continue;
        }
        // 起動した直後: 保存した値をまだかけていなければ、ノードが見つかるまで 5 秒おきに試す（かけたら止める）。
        // 一定の雑音を消すは、設定がある（入れて Frame を再起動した）ときだけ試す（サービスが動きだすのを待つ）
        const bool nsPending = hasDesiredNs_ && appliedNodeId_ < 0;
        const bool steadyPending = hasDesiredSteady_ && appliedSteadyNodeId_ < 0 && state_.steadyKnown;
        const bool applyPending = nsPending || steadyPending;
        if (applyPending && now >= nextApplyTry) {
            lock.unlock();
            NsParams params = nsPending ? readNsParams() : NsParams();
            SteadyParams steadyParams = steadyPending ? readSteadyParams() : SteadyParams();
            lock.lock();
            if (nsPending) {
                applyDesiredIfNeeded(params, lock);
                if (appliedNodeId_ >= 0) {
                    MicState fresh = state_;
                    fresh.nsParams = params;
                    store(fresh);
                }
            }
            if (steadyPending) {
                applyDesiredSteadyIfNeeded(steadyParams, lock);
                if (appliedSteadyNodeId_ >= 0) {
                    MicState fresh = state_;
                    fresh.steadyParams = steadyParams;
                    store(fresh);
                }
            }
            nextApplyTry = after(kApplyRetrySec);
            continue;
        }
        // 次にすることの時刻まで待つ（パネルが見えていない間は、既定を見る 2 秒おきと、強さをかける試しだけ）
        Clock::time_point wakeAt = nextDefaultPoll;
        if (active_) wakeAt = std::min(wakeAt, nextRefresh);
        if (applyPending) wakeAt = std::min(wakeAt, nextApplyTry);
        wake_.wait_until(lock, wakeAt);
    }
}
