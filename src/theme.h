// 画面の色の定義（ここ 1 か所にまとめる）と、WCAG 2.x のコントラスト比の計算。
// 描画コードはここの色だけを使い、--contrast-report は同じ色の組み合わせを計算して合否を出す。
#pragma once

#include "draw.h"

#include <string>
#include <vector>

/**
 * 16 進の RGB（例: 0xe27dfd）から色を作る。
 * @param rgb 0xRRGGBB
 * @return 色（0〜1）
 */
constexpr Color hexColor(unsigned rgb) {
    return {((rgb >> 16) & 0xFF) / 255.0, ((rgb >> 8) & 0xFF) / 255.0, (rgb & 0xFF) / 255.0};
}

/**
 * 2 つの色を混ぜる（不透明な背景の上に、半透明の色を重ねたときの見た目）。
 * @param top 上に重ねる色
 * @param bottom 下の色
 * @param alpha 上の色の不透明度（0〜1）
 * @return 混ざった色
 */
constexpr Color blendColor(Color top, Color bottom, double alpha) {
    return {top.r * alpha + bottom.r * (1 - alpha), top.g * alpha + bottom.g * (1 - alpha),
            top.b * alpha + bottom.b * (1 - alpha)};
}

// ---- 背景の段（GitHub ダーク系） ----
constexpr Color kBg = hexColor(0x0d1117);         ///< パネルの地
constexpr Color kCard = hexColor(0x161b22);       ///< カード
constexpr Color kControl = hexColor(0x21262d);    ///< ボタン・ピルの地
constexpr Color kControlHover = hexColor(0x30363d);  ///< ボタンにポインターが乗っている・押している
constexpr Color kDivider = hexColor(0x30363d);    ///< 飾りの線（部品の見分けには使わない）
// ---- 文字 ----
constexpr Color kText = hexColor(0xe6edf3);       ///< 本文
constexpr Color kTextMuted = hexColor(0x9198a1);  ///< 補足（説明・見出し）
constexpr Color kTextDisabled = hexColor(0x7d8590);  ///< 押せないボタンの文字
// ---- 部品の枠（背景に対して 3:1 以上。WCAG 1.4.11） ----
constexpr Color kBorder = hexColor(0x6e7681);
// ---- イメージカラー ----
constexpr Color kAccent = hexColor(0xe27dfd);         ///< 選択中・通っている段・再生位置
constexpr Color kAccentPressed = hexColor(0xc45fe0);  ///< 押している間（少し濃く）
constexpr Color kOnAccent = hexColor(0x0d1117);       ///< アクセントの塗りの上の文字（白は 2.4:1 で不可）
constexpr double kAccentTintAlpha = 0.20;             ///< アクセントの薄い塗り・光彩（15〜25%）
constexpr Color kAccentTint = blendColor(kAccent, kCard, kAccentTintAlpha);  ///< カードの上の薄い塗り
// ---- 状態の色（色だけで伝えない。● や文字も付ける） ----
constexpr Color kSuccess = hexColor(0x3fb950);    ///< マイク使用中
constexpr Color kSuccessTint = blendColor(kSuccess, kBg, 0.15);
constexpr Color kDanger = hexColor(0xf85149);     ///< 失敗・録音中
constexpr Color kDangerTint = blendColor(kDanger, kCard, 0.18);  ///< 録音中の停止ボタンの塗り
constexpr Color kQuitFill = blendColor(kDanger, kBg, 0.12);      ///< 終了ボタンの塗り（控えめ）
// ---- ミュート中（見出しのバッジと、ミュートの帯。色だけで伝えない。マイクに斜線の絵と文字も付ける） ----
constexpr Color kMuteFill = hexColor(0x3a1a1c);    ///< 地
constexpr Color kMuteText = hexColor(0xff8a80);    ///< 文字・絵
constexpr Color kMuteBorder = hexColor(0xe5534b);  ///< 枠

/** コントラストの確かめ方の種類。 */
enum class ContrastKind {
    Text,      ///< 文字（4.5:1 以上）
    LargeText, ///< 大きい文字（24px 以上、太字なら 18.7px 以上。3:1 以上）
    Ui,        ///< 部品の枠・選択状態・図（3:1 以上。WCAG 1.4.11）
    Disabled,  ///< 押せないボタンの文字（WCAG では例外。読める程度の 3:1 を目安にする）
};

/** 画面で使っている文字色・部品の色と、その背景の組み合わせ 1 つ。 */
struct ContrastPair {
    const char* what;  ///< どこで使っているか
    Color fg;
    Color bg;
    ContrastKind kind;
};

/**
 * WCAG 2.x の相対輝度。
 * @param c 色
 * @return 0（黒）〜1（白）
 */
double relativeLuminance(Color c);

/**
 * WCAG 2.x のコントラスト比。
 * @param a 色
 * @param b 色
 * @return 1〜21
 */
double contrastRatio(Color a, Color b);

/**
 * 種類ごとの必要な比。
 * @param kind 種類
 * @return 4.5 か 3
 */
double requiredRatio(ContrastKind kind);

/**
 * 画面で使っている組み合わせの一覧（mic_panel.cpp の描画と対応させておく）。
 * @return 一覧
 */
const std::vector<ContrastPair>& contrastPairs();

/**
 * --contrast-report: すべての組み合わせの比と合否を標準出力に書く。
 * @return すべて合格なら 0、1 つでも不合格なら 1
 */
int printContrastReport();
