// ノイズ除去を wav ファイルに通す道具（試し聞き・測定用。リリースには入れない）。
//   denoise-wav IN.wav OUT.wav [強さ dB]
// IN は 16 bit PCM（2ch なら左右の平均）。OUT は 16 bit 1ch。遅れ（kFrame サンプル）は詰めてそろえる。
#include "denoise/spectral_denoiser.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>

namespace {

/** wav の 16 bit PCM を読む。2ch 以上は平均して 1ch にする。 */
bool readWav(const char* path, std::vector<float>& samples, uint32_t& rate) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    char riff[12];
    if (!file.read(riff, 12) || std::memcmp(riff, "RIFF", 4) != 0 || std::memcmp(riff + 8, "WAVE", 4) != 0) return false;
    uint16_t channels = 0;
    uint16_t bits = 0;
    while (file) {
        char id[4];
        uint32_t size = 0;
        if (!file.read(id, 4) || !file.read(reinterpret_cast<char*>(&size), 4)) return false;
        if (std::memcmp(id, "fmt ", 4) == 0) {
            std::vector<char> fmt(size);
            file.read(fmt.data(), size);
            std::memcpy(&channels, fmt.data() + 2, 2);
            std::memcpy(&rate, fmt.data() + 4, 4);
            std::memcpy(&bits, fmt.data() + 14, 2);
        } else if (std::memcmp(id, "data", 4) == 0) {
            if (bits != 16 || channels == 0) return false;
            // pw-record は止められたとき data の大きさを書かないことがあるので、ファイルの終わりまで読む
            std::vector<int16_t> raw;
            int16_t value;
            while (file.read(reinterpret_cast<char*>(&value), 2)) raw.push_back(value);
            samples.resize(raw.size() / channels);
            for (size_t i = 0; i < samples.size(); ++i) {
                float sum = 0.0f;
                for (int c = 0; c < channels; ++c) sum += raw[i * channels + c] / 32768.0f;
                samples[i] = sum / channels;
            }
            return true;
        } else {
            file.seekg(size + (size & 1), std::ios::cur);
        }
    }
    return false;
}

/** 16 bit 1ch の wav を書く。 */
bool writeWav(const char* path, const std::vector<float>& samples, uint32_t rate) {
    std::ofstream file(path, std::ios::binary);
    if (!file) return false;
    const uint32_t dataSize = static_cast<uint32_t>(samples.size() * 2);
    const uint32_t riffSize = 36 + dataSize;
    const uint16_t format = 1;
    const uint16_t channels = 1;
    const uint32_t byteRate = rate * 2;
    const uint16_t align = 2;
    const uint16_t bits = 16;
    const uint32_t fmtSize = 16;
    file.write("RIFF", 4);
    file.write(reinterpret_cast<const char*>(&riffSize), 4);
    file.write("WAVEfmt ", 8);
    file.write(reinterpret_cast<const char*>(&fmtSize), 4);
    file.write(reinterpret_cast<const char*>(&format), 2);
    file.write(reinterpret_cast<const char*>(&channels), 2);
    file.write(reinterpret_cast<const char*>(&rate), 4);
    file.write(reinterpret_cast<const char*>(&byteRate), 4);
    file.write(reinterpret_cast<const char*>(&align), 2);
    file.write(reinterpret_cast<const char*>(&bits), 2);
    file.write("data", 4);
    file.write(reinterpret_cast<const char*>(&dataSize), 4);
    for (float s : samples) {
        float c = s < -1.0f ? -1.0f : (s > 1.0f ? 1.0f : s);
        const int16_t v = static_cast<int16_t>(c * 32767.0f);
        file.write(reinterpret_cast<const char*>(&v), 2);
    }
    return static_cast<bool>(file);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s IN.wav OUT.wav [strength_db]\n", argv[0]);
        return 2;
    }
    std::vector<float> samples;
    uint32_t rate = 0;
    if (!readWav(argv[1], samples, rate)) {
        std::fprintf(stderr, "cannot read %s (16 bit PCM wav only)\n", argv[1]);
        return 1;
    }
    SpectralDenoiser denoiser(rate);
    denoiser.setStrengthDb(argc > 3 ? static_cast<float>(std::atof(argv[3])) : 20.0f);
    samples.resize(samples.size() + SpectralDenoiser::kFrame, 0.0f);
    std::vector<float> out(samples.size());
    // filter-chain と同じく、小分けにして通す（480 サンプルずつ）
    for (size_t pos = 0; pos < samples.size(); pos += 480) {
        const size_t count = std::min<size_t>(480, samples.size() - pos);
        denoiser.process(samples.data() + pos, out.data() + pos, count);
    }
    out.erase(out.begin(), out.begin() + SpectralDenoiser::kFrame);
    if (!writeWav(argv[2], out, rate)) {
        std::fprintf(stderr, "cannot write %s\n", argv[2]);
        return 1;
    }
    return 0;
}
