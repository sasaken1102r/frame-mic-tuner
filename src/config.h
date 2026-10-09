// 設定（~/.config/frame-mic-tuner/config.json）の読み書き。持つのは言語と、ノイズ除去の強さ。
// エコー除去・ノイズ除去のオン・オフは WirePlumber が、自動起動の状態は systemd が持つので、ここには保存しない。
#pragma once

#include "i18n.h"
#include "mic_state.h"

#include <string>
#include <vector>

/** パネルの左の列のタブ。 */
enum class PanelTab {
    Quick,  ///< かんたん（プリセット）。キーの値は "quick"
    Fine,   ///< 細かく調整（1 つずつの設定）。キーの値は "fine"
};

/**
 * アプリの設定。ファイルが無いときはこの既定値で動く。
 */
struct Config {
    Language language = systemLanguage();  ///< 画面の文言の言語（"ja" / "en" / "sc"。既定は Frame のシステム言語）
    PanelTab tab = PanelTab::Quick;        ///< 最後に見ていたタブ（パネルを開いたときにこのタブを出す）
    // ノイズ除去の強さ。SteamOS は PipeWire の起動のたびに既定値に戻すので、アプリが保存してかけ直す。
    // バーを動かすまでは持たない（持たない間は何もかけず、SteamOS の値のまま）
    bool hasNsParams = false;
    double nsVad = kNsVadDefault;      ///< 判定の厳しさ（%）。キーは ns_vad_threshold_percent
    double nsGrace = kNsGraceDefault;  ///< 余韻（ms）。キーは ns_vad_grace_ms
    // 新しい版の自動確認（起動時と 1 日 1 回）。オフでも［確認］ボタンでは確かめられる。キーは update_check
    bool updateCheck = true;
};

/** 画面の文字に使うフォント（Noto Sans CJK。読めなければ fontconfig で探す）。 */
constexpr const char* kFontPath = "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc";
constexpr const char* kBoldFontPath = "/usr/share/fonts/noto-cjk/NotoSansCJK-Bold.ttc";

/**
 * 既定の設定ファイルの場所を返す（$XDG_CONFIG_HOME か ~/.config の下）。
 * @return 設定ファイルのパス
 */
std::string defaultConfigPath();

/**
 * 設定ファイルを読む。ファイルが無いときは既定値を返して成功扱いにする。
 * 知らないキー・知らない言語は warnings に入れる。
 * @param path 設定ファイルのパス
 * @param out 読んだ設定の書き込み先（失敗時は変更しない）
 * @param warnings 気づいたことの追加先
 * @param error 読めなかったときの理由
 * @return 成功（またはファイルが無い）なら true
 */
bool loadConfig(const std::string& path, Config& out, std::vector<std::string>& warnings, std::string& error);

/**
 * 設定ファイルを読んでログを出す。読めなければ既定値。
 * @param path 設定ファイルのパス
 * @return 設定
 */
Config loadConfigOrDefault(const std::string& path);

/**
 * 設定をファイルに保存する（同じフォルダに一時ファイルを書いてから置き換える）。
 * フォルダが無ければ作る。書くのはこの設定ファイルだけ。
 * @param path 設定ファイルのパス
 * @param config 保存する設定
 * @param error 失敗したときの理由
 * @return 保存できたら true
 */
bool saveConfig(const std::string& path, const Config& config, std::string& error);
