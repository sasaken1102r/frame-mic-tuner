// 一定の雑音（空気清浄機・サーキュレーター・PC のファンなど）だけを周波数ごとに引くノイズ除去。
// Valve のノイズ除去（RNNoise の声らしさで判定する）と違って、息や囁きのような「声らしくないけれど急に出てくる音」は残す。
#pragma once

#include <array>
#include <complex>
#include <cstddef>
#include <vector>

/**
 * 一定の雑音を引くノイズ除去（1ch）。
 * 1024 サンプルの窓を 512 ずつずらして周波数に分け、各周波数の「最近 3 秒くらいの最小値」を雑音とみなす。
 * 雑音より大きく出ている成分は残し、雑音だけの成分は strength dB まで下げる（ゼロにはしない）。
 * 遅れは kFrame サンプル（48 kHz で約 21 ms）。
 */
class SpectralDenoiser {
public:
    static constexpr int kFrame = 1024;
    static constexpr int kHop = kFrame / 2;
    static constexpr int kBins = kFrame / 2 + 1;
    /** 雑音の見積もり: kSubwindows 個の小窓（それぞれ kSubwindowFrames フレーム）の最小値 */
    static constexpr int kSubwindows = 8;
    static constexpr int kSubwindowFrames = 36;

    /**
     * @param sampleRate サンプリング周波数（Hz）
     */
    explicit SpectralDenoiser(double sampleRate);

    /** 雑音の見積もりや途中のデータを捨てて、最初の状態に戻す。 */
    void reset();

    /**
     * 雑音をどこまで下げるか。
     * @param db 0〜40（0 なら素通し。大きいほど雑音が消えるが、削りすぎると音がこもる）
     */
    void setStrengthDb(float db);

    /**
     * 音を通す。in と out は同じ配列でもよい。
     * @param in 入力
     * @param out 出力（kFrame サンプル遅れて出てくる）
     * @param count サンプル数
     */
    void process(const float* in, float* out, std::size_t count);

private:
    /** たまった 1 フレームぶんを処理して、出力の足し込み先に足す。 */
    void processFrame();

    double sampleRate_;
    float floorGain_ = 1.0f;

    std::array<float, kFrame> window_ {};
    std::array<float, kFrame> input_ {};
    std::array<float, kFrame> accum_ {};
    std::array<float, kHop> ready_ {};
    int fill_ = kFrame - kHop;
    int readPos_ = 0;

    // 周波数ごとの状態
    std::array<float, kBins> smooth_ {};
    std::array<float, kBins> current_ {};
    std::array<std::array<float, kBins>, kSubwindows> subMin_ {};
    std::array<float, kBins> prevGain_ {};
    std::array<float, kBins> prevPower_ {};
    std::array<float, kBins> highPass_ {};
    int subFrames_ = 0;
    int subIndex_ = 0;
    bool first_ = true;

    // FFT の作業用
    std::vector<std::complex<float>> twiddle_;
    std::vector<int> bitReverse_;
    std::array<std::complex<float>, kFrame> spectrum_ {};

    /** その場で FFT する（inverse なら逆変換。1/N はかけない）。 */
    void fft(std::complex<float>* data, bool inverse) const;
};
