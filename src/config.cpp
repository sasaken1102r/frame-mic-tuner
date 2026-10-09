// 設定の読み書きの実装。
#include "config.h"

#include "json.h"

#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

namespace {

/**
 * フォルダを親から順に作る（mkdir -p と同じ）。
 * @param dir 作るフォルダ
 * @return できた（元からあった）なら true
 */
bool makeDirectories(const std::string& dir) {
    for (size_t pos = 1; pos <= dir.size(); ++pos) {
        if (pos != dir.size() && dir[pos] != '/') continue;
        const std::string part = dir.substr(0, pos);
        if (::mkdir(part.c_str(), 0755) != 0 && errno != EEXIST) return false;
    }
    return true;
}

}  // namespace

std::string defaultConfigPath() {
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    std::string base;
    if (xdg != nullptr && xdg[0] != '\0') {
        base = xdg;
    } else {
        const char* home = std::getenv("HOME");
        base = std::string(home != nullptr ? home : ".") + "/.config";
    }
    return base + "/frame-mic-tuner/config.json";
}

bool loadConfig(const std::string& path, Config& out, std::vector<std::string>& warnings, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        out = Config();  // ファイルが無いときは既定値
        return true;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();

    JsonValue root;
    if (!parseJson(buffer.str(), root, error)) return false;
    if (!root.isObject()) {
        error = "一番外側は { ... } にしてください";
        return false;
    }

    Config config;  // 書かれていない項目は既定値
    for (const auto& member : root.members) {
        if (member.first != "language" && member.first != "tab" && member.first != "ns_vad_threshold_percent" &&
            member.first != "ns_vad_grace_ms" && member.first != "update_check") {
            warnings.push_back("知らないキーです: " + member.first);
        }
    }
    if (const JsonValue* language = root.get("language")) {
        if (!language->isString() || !parseLanguage(language->text, config.language)) {
            warnings.push_back("language は \"ja\"、\"en\"、\"sc\" のいずれかで書いてください");
        }
    }
    if (const JsonValue* tab = root.get("tab")) {
        if (tab->isString() && tab->text == "quick") {
            config.tab = PanelTab::Quick;
        } else if (tab->isString() && tab->text == "fine") {
            config.tab = PanelTab::Fine;
        } else {
            warnings.push_back("tab は \"quick\" か \"fine\" で書いてください");
        }
    }
    if (const JsonValue* updateCheck = root.get("update_check")) {
        if (updateCheck->isBool()) {
            config.updateCheck = updateCheck->boolean;
        } else {
            warnings.push_back("update_check は true か false で書いてください");
        }
    }
    // ノイズ除去の強さは 2 つそろっているときだけ使う（範囲外は丸める）
    const JsonValue* vad = root.get("ns_vad_threshold_percent");
    const JsonValue* grace = root.get("ns_vad_grace_ms");
    if (vad != nullptr || grace != nullptr) {
        if (vad != nullptr && grace != nullptr && vad->isNumber() && grace->isNumber()) {
            config.hasNsParams = true;
            config.nsVad = clampNsVad(vad->number);
            config.nsGrace = clampNsGrace(grace->number);
        } else {
            warnings.push_back("ns_vad_threshold_percent と ns_vad_grace_ms は 2 つとも数値で書いてください");
        }
    }
    out = config;
    return true;
}

Config loadConfigOrDefault(const std::string& path) {
    Config config;
    std::vector<std::string> warnings;
    std::string error;
    if (!loadConfig(path, config, warnings, error)) {
        std::fprintf(stderr, "[設定] %s を読めませんでした: %s（既定値で動きます）\n", path.c_str(), error.c_str());
        return Config();
    }
    for (const auto& warning : warnings) std::fprintf(stderr, "[設定] %s\n", warning.c_str());
    return config;
}

bool saveConfig(const std::string& path, const Config& config, std::string& error) {
    const size_t slash = path.find_last_of('/');
    if (slash != std::string::npos && !makeDirectories(path.substr(0, slash))) {
        error = "フォルダを作れません: " + std::string(std::strerror(errno));
        return false;
    }
    std::ostringstream out;
    out << "{\n"
        << "  \"language\": \"" << languageCode(config.language) << "\",\n"
        << "  \"tab\": \"" << (config.tab == PanelTab::Fine ? "fine" : "quick") << "\",\n"
        << "  \"update_check\": " << (config.updateCheck ? "true" : "false");
    if (config.hasNsParams) {
        char numbers[128];
        std::snprintf(numbers, sizeof(numbers), ",\n  \"ns_vad_threshold_percent\": %.0f,\n  \"ns_vad_grace_ms\": %.0f",
                      clampNsVad(config.nsVad), clampNsGrace(config.nsGrace));
        out << numbers;
    }
    out << "\n}\n";

    // 途中で止まっても壊れたファイルが残らないよう、一時ファイルに書いてから置き換える
    const std::string temp = path + ".tmp";
    {
        std::ofstream file(temp, std::ios::binary | std::ios::trunc);
        if (!file) {
            error = "書き込めません: " + temp;
            return false;
        }
        file << out.str();
        if (!file.flush()) {
            error = "書き込みに失敗しました: " + temp;
            return false;
        }
    }
    if (std::rename(temp.c_str(), path.c_str()) != 0) {
        error = "置き換えに失敗しました: " + std::string(std::strerror(errno));
        ::unlink(temp.c_str());
        return false;
    }
    return true;
}
