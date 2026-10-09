// 音の出口ごとの設定をかける係の実装。
#include "output_sync.h"

#include <cmath>
#include <cstdio>
#include <utility>

namespace {

/**
 * 前の版のキー（ns_vad_*）に、今の出口の強さを写す。
 * @param config 設定
 * @param profile 今の出口の設定
 * @return 変えたら true
 */
bool mirrorLegacyNs(Config& config, const OutputProfile& profile) {
    if (config.hasNsParams && std::fabs(config.nsVad - profile.nsVad) < 0.5 &&
        std::fabs(config.nsGrace - profile.nsGrace) < 0.5) {
        return false;
    }
    config.hasNsParams = true;
    config.nsVad = profile.nsVad;
    config.nsGrace = profile.nsGrace;
    return true;
}

/**
 * 最後に使った日を今日にする。
 * @param profile 設定
 * @param today 今日
 * @return 変えたら true
 */
bool touch(OutputProfile& profile, const std::string& today) {
    if (today.empty() || profile.lastUsed == today) return false;
    profile.lastUsed = today;
    return true;
}

/**
 * ログ用に設定を短くする。
 * @param p 設定
 * @return 例:「エコー除去オン・ノイズ除去オフ・23%/500ms」
 */
std::string describe(const OutputProfile& p) {
    char text[96];
    std::snprintf(text, sizeof(text), "エコー除去%s・ノイズ除去%s・%.0f%%/%.0fms", p.echo ? "オン" : "オフ",
                  p.ns ? "オン" : "オフ", p.nsVad, p.nsGrace);
    return text;
}

}  // namespace

OutputProfile& ensureProfile(Config& config, const std::string& key, const AudioDevices& devices, bool& changed) {
    const AudioEndpoint* endpoint = devices.findOutput(key);
    auto it = config.outputs.find(key);
    if (it == config.outputs.end()) {
        const OutputProfile first = firstProfile(key, endpoint != nullptr ? endpoint->name : key);
        std::fprintf(stderr, "[出口] 初めての出口 %s（%s）: %s から始めます\n", key.c_str(), first.name.c_str(),
                     first.kind == OutputKind::Speaker ? "スピーカーのプリセット" : "イヤホンのプリセット");
        it = config.outputs.emplace(key, first).first;
        changed = true;
    } else if (endpoint != nullptr && !endpoint->name.empty() && it->second.name != endpoint->name) {
        it->second.name = endpoint->name;
        changed = true;
    }
    const OutputKind kind = isBuiltinSpeaker(key) ? OutputKind::Speaker : OutputKind::Earphones;
    if (it->second.kind != kind) {
        it->second.kind = kind;
        changed = true;
    }
    return it->second;
}

OutputProfile liveProfile(const MicState& state, const OutputProfile& base) {
    OutputProfile live = base;
    if (state.echoKnown) live.echo = state.echo;
    if (state.nsKnown) live.ns = state.ns;
    if (state.nsParams.vadKnown && state.nsParams.graceKnown) {
        live.nsVad = clampNsVad(state.nsParams.vad);
        live.nsGrace = clampNsGrace(state.nsParams.grace);
    }
    return live;
}

OutputSync::OutputSync(Request request) : request_(std::move(request)) {}

void OutputSync::noteWrite(uint64_t ticket) {
    if (ticket > waitTicket_) waitTicket_ = ticket;
}

void OutputSync::apply(const OutputProfile& profile, bool onlyIfChanged) {
    noteWrite(request_(MicCommand::applySettings(profile.echo, profile.ns, true, profile.nsVad, profile.nsGrace,
                                                 onlyIfChanged)));
}

bool OutputSync::update(const MicState& state, Config& config, const std::string& today) {
    if (!state.loaded || !state.devices.defaultsKnown) return false;
    const std::string key = state.devices.defaultOutputKey;
    if (key.empty()) return false;
    if (!isDeviceOutputKey(key)) {
        // フィルターや仮想の出口（本物の機器ではない）が既定: 出口の設定は何もかけず、今のまま
        if (key != skippedKey_) std::fprintf(stderr, "[出口] 既定の出力 %s は機器の出口ではないので、設定はかけません\n", key.c_str());
        skippedKey_ = key;
        return false;
    }
    skippedKey_.clear();
    const bool liveKnown = state.echoKnown && state.nsKnown;
    // 頼んだ書き込みが終わり、その後に読んだ状態か（自動でかけた直後の値を「外からの変化」と読み違えない）
    const bool settled = state.writesDone >= waitTicket_ && state.writeError == MicError::None;
    bool changed = false;

    if (!config.outputsSaved) {
        // 前の版から初めての起動: 今かかっている設定を、今の出口の設定として移す（何もかけない）
        if (!liveKnown) return false;
        OutputProfile& profile = ensureProfile(config, key, state.devices, changed);
        profile = liveProfile(state, profile);
        if (config.hasNsParams) {
            // 前の版が保存していた強さ（起動のたびにかけ直していた値）を正とする（今はまだかけ直す前かもしれない）
            profile.nsVad = config.nsVad;
            profile.nsGrace = config.nsGrace;
        }
        touch(profile, today);
        config.outputsSaved = true;
        mirrorLegacyNs(config, profile);
        activeKey_ = key;
        started_ = true;
        std::fprintf(stderr, "[出口] 今の設定（%s）を、今の出口 %s の設定として覚えました\n", describe(profile).c_str(),
                     key.c_str());
        return true;
    }

    if (!started_ || key != activeKey_) {
        // 起動したとき・出口が変わったとき: その出口の覚えた設定をかける
        if (!liveKnown) return false;
        const bool switched = started_;
        if (switched && settled && !activeKey_.empty()) {
            // 前の出口: 切り替わる直前までの値（閉じている間に外から変えられた分も）を覚えておく
            auto old = config.outputs.find(activeKey_);
            if (old != config.outputs.end()) {
                OutputProfile live = liveProfile(state, old->second);
                if (!state.nsApplied) {
                    live.nsVad = old->second.nsVad;
                    live.nsGrace = old->second.nsGrace;
                }
                if (!live.sameSettings(old->second)) {
                    std::fprintf(stderr, "[出口] 前の出口 %s の設定が変わっていたので覚えます: %s\n", activeKey_.c_str(),
                                 describe(live).c_str());
                    old->second = live;
                    changed = true;
                }
            }
        }
        OutputProfile& profile = ensureProfile(config, key, state.devices, changed);
        const OutputProfile before = liveProfile(state, profile);
        std::fprintf(stderr, "[出口] %s %s: %s をかけます\n", switched ? "出口が変わりました →" : "起動: 今の出口は",
                     key.c_str(), describe(profile).c_str());
        apply(profile, true);
        if (before.echo != profile.echo || before.ns != profile.ns) {
            notice_ = {true, key, before};
        } else {
            notice_.shown = false;
        }
        changed |= touch(profile, today);
        changed |= mirrorLegacyNs(config, profile);
        activeKey_ = key;
        started_ = true;
        return changed;
    }

    // 同じ出口: 外から変わった値（ほかのツール・aux ボタン・wpctl settings）を覚える
    OutputProfile& profile = ensureProfile(config, key, state.devices, changed);
    if (liveKnown && settled) {
        OutputProfile live = liveProfile(state, profile);
        if (!state.nsApplied) {
            live.nsVad = profile.nsVad;
            live.nsGrace = profile.nsGrace;
        }
        if (!live.sameSettings(profile)) {
            std::fprintf(stderr, "[出口] 今の出口 %s の設定が外から変わったので覚えます: %s\n", key.c_str(),
                         describe(live).c_str());
            if (live.echo != profile.echo || live.ns != profile.ns) notice_.shown = false;  // 元に戻すの値はもう古い
            profile = live;
            changed = true;
            changed |= mirrorLegacyNs(config, profile);
        }
    }
    changed |= touch(profile, today);
    return changed;
}

bool OutputSync::undo(Config& config) {
    if (!notice_.shown) return false;
    notice_.shown = false;
    auto it = config.outputs.find(notice_.key);
    if (it == config.outputs.end()) return false;
    OutputProfile& profile = it->second;
    profile.echo = notice_.previous.echo;
    profile.ns = notice_.previous.ns;
    profile.nsVad = notice_.previous.nsVad;
    profile.nsGrace = notice_.previous.nsGrace;
    std::fprintf(stderr, "[出口] 元に戻す: %s を %s にします\n", notice_.key.c_str(), describe(profile).c_str());
    if (notice_.key == activeKey_) {
        apply(profile, false);
        mirrorLegacyNs(config, profile);
    }
    return true;
}
