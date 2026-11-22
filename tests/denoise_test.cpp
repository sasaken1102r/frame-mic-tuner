// ノイズ除去の確かめ（ctest の denoise）。
// 一定の雑音（色の付いたホワイトノイズ）に、息のような短い雑音のかたまりと、声のような音（倍音のある音）を重ねて通し、
// 「すき間の雑音は大きく下がる」「かたまりと声はほとんど削られない」「強さ 0 は素通し」を確かめる。
#include "denoise/spectral_denoiser.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

namespace {

constexpr double kRate = 48000.0;
constexpr double kPi = 3.14159265358979323846;
int failures = 0;

void check(bool ok, const char* what, double value) {
    std::printf("%s %s (%.2f)\n", ok ? "ok  " : "FAIL", what, value);
    if (!ok) ++failures;
}

/** 区間の平均パワー（dB）。 */
double powerDb(const std::vector<float>& x, double t0, double t1) {
    const size_t a = static_cast<size_t>(t0 * kRate);
    const size_t b = static_cast<size_t>(t1 * kRate);
    double sum = 0.0;
    for (size_t i = a; i < b; ++i) sum += static_cast<double>(x[i]) * x[i];
    return 10.0 * std::log10(sum / static_cast<double>(b - a) + 1e-20);
}

/** 通して、遅れ（kFrame）を詰めた出力を返す。 */
std::vector<float> run(const std::vector<float>& in, float strength) {
    SpectralDenoiser denoiser(kRate);
    denoiser.setStrengthDb(strength);
    std::vector<float> padded = in;
    padded.resize(in.size() + SpectralDenoiser::kFrame, 0.0f);
    std::vector<float> out(padded.size());
    for (size_t pos = 0; pos < padded.size(); pos += 480) {
        const size_t count = std::min<size_t>(480, padded.size() - pos);
        denoiser.process(padded.data() + pos, out.data() + pos, count);
    }
    return std::vector<float>(out.begin() + SpectralDenoiser::kFrame, out.end());
}

}  // namespace

int main() {
    std::mt19937 rng(7);
    std::normal_distribution<float> gauss(0.0f, 1.0f);
    const size_t n = static_cast<size_t>(12.0 * kRate);
    // 一定の雑音: 低いほうが強い（1 次のローパス）、-55 dBFS くらい
    std::vector<float> noise(n);
    float lp = 0.0f;
    for (size_t i = 0; i < n; ++i) {
        lp = 0.95f * lp + 0.05f * gauss(rng);
        noise[i] = lp * 0.006f + gauss(rng) * 0.0005f;
    }
    // 息: 8〜9 秒に、帯域の広い雑音のかたまり（雑音より 10 dB くらい大きい）
    std::vector<float> breath(n, 0.0f);
    for (size_t i = static_cast<size_t>(8.0 * kRate); i < static_cast<size_t>(9.0 * kRate); ++i) {
        breath[i] = gauss(rng) * 0.004f;
    }
    // 声のような音: 10〜11 秒に、150 Hz の倍音
    std::vector<float> voice(n, 0.0f);
    for (size_t i = static_cast<size_t>(10.0 * kRate); i < static_cast<size_t>(11.0 * kRate); ++i) {
        float v = 0.0f;
        for (int h = 1; h <= 20; ++h) v += std::sin(2.0 * kPi * 150.0 * h * i / kRate) / h;
        voice[i] = v * 0.02f;
    }
    std::vector<float> mix(n);
    for (size_t i = 0; i < n; ++i) mix[i] = noise[i] + breath[i] + voice[i];

    const std::vector<float> out = run(mix, 20.0f);
    // すき間（4〜7 秒。見積もりが落ち着いたあと）の雑音
    const double noiseIn = powerDb(mix, 4.0, 7.0);
    const double noiseOut = powerDb(out, 4.0, 7.0);
    check(noiseIn - noiseOut >= 15.0, "steady noise drops by 15 dB or more", noiseIn - noiseOut);
    // 息と声は、雑音を引いたパワーで比べる
    const double pNoiseIn = std::pow(10.0, noiseIn / 10.0);
    const double pNoiseOut = std::pow(10.0, noiseOut / 10.0);
    const double breathIn = 10.0 * std::log10(std::pow(10.0, powerDb(mix, 8.1, 8.9) / 10.0) - pNoiseIn);
    const double breathOut = 10.0 * std::log10(std::pow(10.0, powerDb(out, 8.1, 8.9) / 10.0) - pNoiseOut);
    check(breathIn - breathOut <= 2.0, "breath burst loses 2 dB or less", breathIn - breathOut);
    const double voiceIn = 10.0 * std::log10(std::pow(10.0, powerDb(mix, 10.1, 10.9) / 10.0) - pNoiseIn);
    const double voiceOut = 10.0 * std::log10(std::pow(10.0, powerDb(out, 10.1, 10.9) / 10.0) - pNoiseOut);
    check(voiceIn - voiceOut <= 1.0, "voice loses 1 dB or less", voiceIn - voiceOut);

    // 強さ 0: 遅れを詰めれば入力と同じ（窓の重ね合わせで元に戻る）
    const std::vector<float> through = run(mix, 0.0f);
    double maxDiff = 0.0;
    for (size_t i = static_cast<size_t>(1.0 * kRate); i < n - static_cast<size_t>(1.0 * kRate); ++i) {
        maxDiff = std::max(maxDiff, static_cast<double>(std::fabs(through[i] - mix[i])));
    }
    check(maxDiff < 1e-4, "strength 0 passes the sound through", maxDiff);

    // 最初が無音でも、雑音の見積もりが 0 に張り付かない（無音 2 秒のあとに雑音が来ても 5 秒後には下がる）
    std::vector<float> late(n, 0.0f);
    for (size_t i = static_cast<size_t>(2.0 * kRate); i < n; ++i) late[i] = noise[i];
    const std::vector<float> lateOut = run(late, 20.0f);
    const double lateDrop = powerDb(late, 8.0, 11.0) - powerDb(lateOut, 8.0, 11.0);
    check(lateDrop >= 15.0, "noise after silence is still removed", lateDrop);

    // かぶっていないときの消音（ゼロではなく 80 dB くらい下がった音）が 6 秒続いたあと、雑音が戻って
    // 5 秒後には下がっている（見積もりの上がる速さを絞ったままだと、追いつくまで何十秒もかかる）
    std::vector<float> unmuted(n);
    for (size_t i = 0; i < n; ++i) unmuted[i] = noise[i] * (i < static_cast<size_t>(6.0 * kRate) ? 1e-4f : 1.0f);
    const std::vector<float> unmutedOut = run(unmuted, 20.0f);
    const double unmutedDrop = powerDb(unmuted, 11.0, 11.9) - powerDb(unmutedOut, 11.0, 11.9);
    check(unmutedDrop >= 15.0, "noise is removed within 5 s after the privacy mute ends", unmutedDrop);

    // 長く伸ばした声（歌・ハミング。200 Hz、5.5 Hz の揺れ、雑音より 20 dB くらい大きい）: 3 秒から伸ばし、
    // 6 秒目（最小値だけの見積もりなら 20 dB 下がっているころ）も 3 dB までしか下がらない
    std::vector<float> held(n);
    double phase = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double t = i / kRate;
        phase += 2.0 * kPi * 200.0 * (1.0 + 0.02 * std::sin(2.0 * kPi * 5.5 * t)) / kRate;
        float v = 0.0f;
        if (t >= 3.0 && t < 11.0) {
            for (int h = 1; h <= 15; ++h) v += static_cast<float>(std::sin(h * phase) / h);
        }
        held[i] = noise[i] + v * 0.02f;
    }
    const std::vector<float> heldOut = run(held, 20.0f);
    const double pHeldNoiseIn = std::pow(10.0, powerDb(held, 1.0, 2.8) / 10.0);
    const double pHeldNoiseOut = std::pow(10.0, powerDb(heldOut, 1.0, 2.8) / 10.0);
    const double heldIn = 10.0 * std::log10(std::pow(10.0, powerDb(held, 8.0, 8.9) / 10.0) - pHeldNoiseIn);
    const double heldOutDb = 10.0 * std::log10(std::pow(10.0, powerDb(heldOut, 8.0, 8.9) / 10.0) - pHeldNoiseOut);
    check(heldIn - heldOutDb <= 3.0, "a note held for 6 s loses 3 dB or less", heldIn - heldOutDb);

    // 雑音が途中で 10 dB 大きくなっても（ファンを強くした）、10 秒後には下がる
    std::vector<float> louder(n);
    for (size_t i = 0; i < n; ++i) louder[i] = noise[i] * (i < static_cast<size_t>(1.0 * kRate) ? 1.0f : 3.16f);
    const std::vector<float> louderOut = run(louder, 20.0f);
    const double louderDrop = powerDb(louder, 11.0, 11.9) - powerDb(louderOut, 11.0, 11.9);
    check(louderDrop >= 15.0, "noise that gets 10 dB louder is removed again within 10 s", louderDrop);

    // 壊れた値（NaN）が入っても、出力が NaN のままにならない
    std::vector<float> broken = mix;
    broken[static_cast<size_t>(3.0 * kRate)] = std::nanf("");
    const std::vector<float> brokenOut = run(broken, 20.0f);
    bool finite = true;
    for (size_t i = static_cast<size_t>(4.0 * kRate); i < n; ++i) finite = finite && std::isfinite(brokenOut[i]);
    check(finite, "a NaN input does not break the output", finite ? 1.0 : 0.0);

    std::printf("%s\n", failures == 0 ? "all passed" : "some checks failed");
    return failures == 0 ? 0 : 1;
}
