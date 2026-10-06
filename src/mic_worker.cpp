// ワーカースレッドの実装。
#include "mic_worker.h"

#include <signal.h>

#include <algorithm>
#include <chrono>
#include <cstdio>

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

uint64_t MicWorker::request(MicCommand command) {
    uint64_t ticket = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (command.kind == MicCommand::Kind::SetNsParams) {
            // バーのドラッグ中の連打は、まだ実行していない前の値を捨てて最後の値だけにする（捨てた分は終わった扱い）
            const auto end = std::remove_if(queue_.begin(), queue_.end(), [](const MicCommand& c) {
                return c.kind == MicCommand::Kind::SetNsParams;
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
        case MicCommand::Kind::Unmute: return writeUnmute();
        case MicCommand::Kind::SetPreset: break;  // run() で 2 つの書き込みを別々に扱う
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
    const auto store = [this](const MicState& fresh) {
        if (!sameMicState(fresh, state_)) ++version_;
        state_ = fresh;
    };
    /**
     * かけたい値があり、ノードがまだかけていないもの（見つかったばかり・作り直された）なら、かけて読み直す。
     * mutex を持って呼び、実行の間だけ外す。
     * @param params 読んだばかりのノイズ除去の値（かけたら読み直した値に置き換える）
     * @param lock 持っている mutex
     */
    const auto applyDesiredIfNeeded = [this](NsParams& params, std::unique_lock<std::mutex>& lock) {
        if (!hasDesiredNs_ || !params.nodeKnown || params.nodeId == appliedNodeId_) return;
        const int nodeId = params.nodeId;
        const double vad = desiredVad_;
        const double grace = desiredGrace_;
        lock.unlock();
        std::fprintf(stderr, "[ノイズ除去] ノード %d に保存した値をかけます（%.0f%% / %.0fms）\n", nodeId, vad, grace);
        const bool ok = writeNsParams(nodeId, vad, grace);
        NsParams fresh = ok ? readNsParams() : params;
        lock.lock();
        if (ok) appliedNodeId_ = nodeId;
        params = fresh;
    };

    Clock::time_point nextRefresh = Clock::now();
    Clock::time_point nextAutostart = Clock::now();
    Clock::time_point nextApplyTry = Clock::now();
    std::unique_lock<std::mutex> lock(mutex_);
    while (!stop_) {
        if (!queue_.empty()) {
            // 書き込み → すぐ読み直し
            const MicCommand command = queue_.front();
            queue_.pop_front();
            const bool isNsParams = command.kind == MicCommand::Kind::SetNsParams;
            const int nodeId = state_.nsParams.nodeKnown ? state_.nsParams.nodeId : -1;
            if (isNsParams) {
                // 手で動かした値が、これからの「かけたい値」になる
                hasDesiredNs_ = true;
                desiredVad_ = clampNsVad(command.vad);
                desiredGrace_ = clampNsGrace(command.grace);
            }
            lock.unlock();
            if (command.kind == MicCommand::Kind::SetPreset) {
                // プリセット: エコー除去 → ノイズ除去の順に書き、2 つ書き終わってから 1 回だけ読み直す
                // （途中の「どちらとも一致しない」状態を表示しない）。片方が失敗しても、もう片方は書く
                const bool echoOk = writeSetting(kEchoCancelKey, command.value);
                const bool nsOk = writeSetting(kNoiseSuppressionKey, false);
                MicState fresh = readMicState(true);
                lock.lock();
                if (echoOk && nsOk) {
                    fresh.writeError = MicError::None;
                } else if (!echoOk && !nsOk) {
                    fresh.writeError = MicError::WriteSettings;
                } else {
                    fresh.writeError = echoOk ? MicError::WriteNs : MicError::WriteEcho;
                }
                store(fresh);
                ++completed_;
                nextRefresh = after(kRefreshSec);
                nextAutostart = after(kAutostartRefreshSec);
                continue;
            }
            const bool ok = execute(command, nodeId);
            MicState fresh;
            if (isNsParams) {
                // 強さを変えたときは、その値だけ読み直す（バーを動かしている間に重いものを読まない）
                fresh = state_;  // 読むのは mutex の外だが、state_ を書くのはこのスレッドだけ
                fresh.nsParams = readNsParams();
            } else {
                fresh = readMicState(true);
            }
            lock.lock();
            if (isNsParams && ok && fresh.nsParams.nodeKnown) appliedNodeId_ = fresh.nsParams.nodeId;
            if (ok) {
                fresh.writeError = MicError::None;
            } else if (isNsParams) {
                fresh.writeError = MicError::WriteNsParams;
            } else if (command.kind == MicCommand::Kind::Unmute) {
                fresh.writeError = MicError::WriteMute;
            } else {
                fresh.writeError =
                    command.kind == MicCommand::Kind::SetAutostart ? MicError::WriteAutostart : MicError::WriteSettings;
            }
            store(fresh);
            ++completed_;
            if (!isNsParams) {
                nextRefresh = after(kRefreshSec);
                nextAutostart = after(kAutostartRefreshSec);
            }
            continue;
        }
        if (refreshNow_ || (active_ && Clock::now() >= nextRefresh)) {
            // 自動起動の状態（systemctl）はめったに変わらないので、開いた直後と 5 秒おきだけ読む
            const bool withAutostart = refreshNow_ || Clock::now() >= nextAutostart;
            refreshNow_ = false;
            lock.unlock();
            MicState fresh = readMicState(withAutostart);
            lock.lock();
            // ノードが作り直されていたら（id が変わった）、保存した値をかけ直す
            applyDesiredIfNeeded(fresh.nsParams, lock);
            if (withAutostart) {
                nextAutostart = after(kAutostartRefreshSec);
            } else {
                fresh.autostart = state_.autostart;
            }
            fresh.writeError = state_.writeError;  // 書き込みの失敗は、次に書き込みが成功するまで出しておく
            store(fresh);
            nextRefresh = after(kRefreshSec);
            continue;
        }
        // 起動した直後: 保存した値をまだかけていなければ、ノードが見つかるまで 5 秒おきに試す（かけたら止める）
        const bool applyPending = hasDesiredNs_ && appliedNodeId_ < 0;
        if (applyPending && Clock::now() >= nextApplyTry) {
            lock.unlock();
            NsParams params = readNsParams();
            lock.lock();
            applyDesiredIfNeeded(params, lock);
            if (appliedNodeId_ >= 0) {
                MicState fresh = state_;
                fresh.nsParams = params;
                store(fresh);
            }
            nextApplyTry = after(kApplyRetrySec);
            continue;
        }
        // パネルが見えていなければ、頼まれるまで何もせずに待つ
        if (active_) {
            wake_.wait_until(lock, nextRefresh);
        } else if (applyPending) {
            wake_.wait_until(lock, nextApplyTry);
        } else {
            wake_.wait(lock);
        }
    }
}
