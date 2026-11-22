// 一定の雑音だけを引くノイズ除去の実装。
#include "spectral_denoiser.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

/** 雑音の見積もりを均すときの、前の値の重み（大きいほどゆっくり） */
constexpr float kSmoothAlpha = 0.85f;
/** 最小値から雑音の平均に直すための倍率（最小値は平均より小さく出るので） */
constexpr float kNoiseBias = 2.0f;
/** 「直前のフレームの結果」をどれだけ信じるか（decision-directed。1 に近いほど音が滑らか） */
constexpr float kDecisionDirected = 0.97f;
/** これより低い周波数は消す（エアコン・机の振動などのうなり） */
constexpr double kHighPassHz = 80.0;
/** 雑音の見積もりが 0 にならないための下限 */
constexpr float kNoiseEpsilon = 1e-12f;

}  // namespace

SpectralDenoiser::SpectralDenoiser(double sampleRate) : sampleRate_(sampleRate) {
    constexpr double kPi = 3.14159265358979323846;
    for (int i = 0; i < kFrame; ++i) {
        // 周期的なハン窓の平方根: 分析と合成の両方にかけると、50% 重ねて足したときにちょうど 1 になる
        window_[i] = static_cast<float>(std::sqrt(0.5 - 0.5 * std::cos(2.0 * kPi * i / kFrame)));
    }
    twiddle_.resize(kFrame / 2);
    for (int i = 0; i < kFrame / 2; ++i) {
        const double angle = -2.0 * kPi * i / kFrame;
        twiddle_[i] = {static_cast<float>(std::cos(angle)), static_cast<float>(std::sin(angle))};
    }
    bitReverse_.resize(kFrame);
    int bits = 0;
    while ((1 << bits) < kFrame) ++bits;
    for (int i = 0; i < kFrame; ++i) {
        int r = 0;
        for (int b = 0; b < bits; ++b) {
            if (i & (1 << b)) r |= 1 << (bits - 1 - b);
        }
        bitReverse_[i] = r;
    }
    for (int k = 0; k < kBins; ++k) {
        const double hz = sampleRate_ * k / kFrame;
        highPass_[k] = hz >= kHighPassHz ? 1.0f : 0.0f;
    }
    reset();
}

void SpectralDenoiser::reset() {
    input_.fill(0.0f);
    accum_.fill(0.0f);
    ready_.fill(0.0f);
    fill_ = kFrame - kHop;
    readPos_ = 0;
    smooth_.fill(0.0f);
    current_.fill(std::numeric_limits<float>::max());
    for (auto& sub : subMin_) sub.fill(std::numeric_limits<float>::max());
    prevGain_.fill(1.0f);
    prevPower_.fill(0.0f);
    subFrames_ = 0;
    subIndex_ = 0;
    first_ = true;
}

void SpectralDenoiser::setStrengthDb(float db) {
    if (!std::isfinite(db)) db = 0.0f;
    db = std::clamp(db, 0.0f, 40.0f);
    floorGain_ = std::pow(10.0f, -db / 20.0f);
}

void SpectralDenoiser::process(const float* in, float* out, std::size_t count) {
    for (std::size_t i = 0; i < count; ++i) {
        float sample = in[i];
        // 壊れた値（NaN・無限大）が 1 つ入ると、そのあとの見積もりがずっと壊れるので 0 にする
        if (!std::isfinite(sample)) sample = 0.0f;
        input_[fill_++] = sample;
        out[i] = ready_[readPos_++];
        if (fill_ == kFrame) {
            processFrame();
            fill_ = kFrame - kHop;
            readPos_ = 0;
        }
    }
}

void SpectralDenoiser::processFrame() {
    for (int i = 0; i < kFrame; ++i) spectrum_[i] = {input_[i] * window_[i], 0.0f};
    fft(spectrum_.data(), false);

    const bool bypass = floorGain_ >= 1.0f;
    std::array<float, kBins> power {};
    std::array<float, kBins> gain {};
    for (int k = 0; k < kBins; ++k) power[k] = std::norm(spectrum_[k]);

    // 雑音の見積もり: 均したパワーの、最近 kSubwindows × kSubwindowFrames フレームの最小値
    for (int k = 0; k < kBins; ++k) {
        smooth_[k] = first_ ? power[k] : kSmoothAlpha * smooth_[k] + (1.0f - kSmoothAlpha) * power[k];
        current_[k] = std::min(current_[k], smooth_[k]);
    }
    first_ = false;
    if (++subFrames_ == kSubwindowFrames) {
        subMin_[subIndex_] = current_;
        subIndex_ = (subIndex_ + 1) % kSubwindows;
        current_.fill(std::numeric_limits<float>::max());
        subFrames_ = 0;
    }

    for (int k = 0; k < kBins; ++k) {
        float noise = current_[k];
        for (const auto& sub : subMin_) noise = std::min(noise, sub[k]);
        noise = noise * kNoiseBias + kNoiseEpsilon;
        // 事後 SNR（いまのパワー ÷ 雑音）と、直前の結果も使った事前 SNR から、ウィーナーの係数を出す
        const float post = power[k] / noise;
        const float prior = kDecisionDirected * prevGain_[k] * prevGain_[k] * prevPower_[k] / noise +
                            (1.0f - kDecisionDirected) * std::max(post - 1.0f, 0.0f);
        gain[k] = prior / (1.0f + prior);
    }
    // となりの周波数と均す（1 本だけ飛び出した係数が「キュルキュル」に聞こえるのを抑える）
    std::array<float, kBins> smoothed {};
    for (int k = 0; k < kBins; ++k) {
        const float left = k > 0 ? gain[k - 1] : 0.0f;
        const float right = k + 1 < kBins ? gain[k + 1] : 0.0f;
        smoothed[k] = (left + gain[k] + right) / 3.0f;
    }
    for (int k = 0; k < kBins; ++k) {
        const float g = bypass ? 1.0f : std::max(smoothed[k], floorGain_) * highPass_[k];
        prevGain_[k] = g;
        prevPower_[k] = power[k];
        spectrum_[k] *= g;
        if (k > 0 && k < kFrame / 2) spectrum_[kFrame - k] = std::conj(spectrum_[k]);
    }
    fft(spectrum_.data(), true);

    const float scale = 1.0f / kFrame;
    for (int i = 0; i < kFrame; ++i) accum_[i] += spectrum_[i].real() * scale * window_[i];
    std::copy(accum_.begin(), accum_.begin() + kHop, ready_.begin());
    std::copy(accum_.begin() + kHop, accum_.end(), accum_.begin());
    std::fill(accum_.begin() + (kFrame - kHop), accum_.end(), 0.0f);
    std::copy(input_.begin() + kHop, input_.end(), input_.begin());
}

void SpectralDenoiser::fft(std::complex<float>* data, bool inverse) const {
    for (int i = 0; i < kFrame; ++i) {
        const int j = bitReverse_[i];
        if (j > i) std::swap(data[i], data[j]);
    }
    for (int size = 2; size <= kFrame; size <<= 1) {
        const int half = size / 2;
        const int step = kFrame / size;
        for (int start = 0; start < kFrame; start += size) {
            for (int j = 0; j < half; ++j) {
                std::complex<float> w = twiddle_[j * step];
                if (inverse) w = std::conj(w);
                const std::complex<float> t = w * data[start + j + half];
                data[start + j + half] = data[start + j] - t;
                data[start + j] += t;
            }
        }
    }
}
