// 画面の色の定義（ここ 1 か所にまとめる）と、WCAG 2.x のコントラスト比の計算。
// 描画コードはここの色だけを使い、--contrast-report は同じ色の組み合わせを計算して合否を出す。
#pragma once

#include "draw.h"

#include "frame_ui.h"  // vendor/frame-ui/cpp（色は共通の UI 部品と同じものを使う）

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

/**
 * 共通の UI 部品（frame_ui）の色をこのアプリの色にする。
 * @param c frame_ui の色
 * @return 色
 */
constexpr Color uiColor(frame_ui::Rgb c) {
    return {c.r, c.g, c.b};
}

// ---- 背景の段（frame-ui と同じ） ----
constexpr Color kBg = uiColor(frame_ui::kBg);                ///< パネルの地
constexpr Color kCard = uiColor(frame_ui::kCard);            ///< カード・重ねた画面
constexpr Color kInset = hexColor(0x14171c);                 ///< カードの中の一段暗い箱（つながり・一覧の行・更新の箱）
constexpr Color kControl = uiColor(frame_ui::kControl);      ///< ボタン・プリセットの地
constexpr Color kControlHover = uiColor(frame_ui::kControlHover);  ///< ポインターが乗っている
constexpr Color kControlDown = uiColor(frame_ui::kControlDown);    ///< 押している
constexpr Color kDivider = uiColor(frame_ui::kDivider);      ///< 飾りの線・バーの溝（部品の見分けには使わない）
constexpr Color kIconBox = uiColor(frame_ui::kChipIdleFill); ///< 絵の下の角丸の箱・スクロールバーの溝
constexpr Color kRowSelected = hexColor(0x241a2a);           ///< 一覧で「設定を表示中」の行の地
// ---- 文字 ----
constexpr Color kText = uiColor(frame_ui::kText);            ///< 本文
constexpr Color kTextMuted = uiColor(frame_ui::kTextMuted);  ///< 補足（説明・見出し）
constexpr Color kTextSoft = uiColor(frame_ui::kTextSoft);    ///< 少し控えめな本文（外付けのマイクの説明・履歴の 2 行目）
constexpr Color kLabelSoft = uiColor(frame_ui::kLabelSoft);  ///< 一覧の区切りの見出し
// ---- 部品の枠（背景に対して 3:1 以上。WCAG 1.4.11） ----
constexpr Color kBorder = uiColor(frame_ui::kBorder);
// ---- イメージカラー ----
constexpr Color kAccent = uiColor(frame_ui::kAccent);            ///< 選択中・通っている段・再生位置
constexpr Color kAccentPressed = uiColor(frame_ui::kAccentDown); ///< 押している間（少し濃く）
constexpr Color kAccentHover = uiColor(frame_ui::kAccentHover);  ///< 乗っている間（少し明るく）
constexpr Color kOnAccent = uiColor(frame_ui::kOnAccent);        ///< アクセントの塗りの上の文字（白は不可）
constexpr Color kOnAccentSoft = hexColor(0x3a1f44);              ///< アクセントの塗りの上の補足の文字
constexpr Color kAccentText = uiColor(frame_ui::kAccentText);    ///< 薄いピンクの文字（札）
constexpr Color kAccentTint = hexColor(0x3b2445);                ///< アクセントの薄い塗り（通っている段・札）
// ---- 状態の色（色だけで伝えない。● や文字も付ける） ----
constexpr Color kSuccess = uiColor(frame_ui::kChipOkText);       ///< マイク使用中・いま使用中
constexpr Color kSuccessTint = uiColor(frame_ui::kChipOkFill);
constexpr Color kDanger = uiColor(frame_ui::kDanger);            ///< 録音の ●・危ないボタンの枠
constexpr Color kDangerText = uiColor(frame_ui::kDangerText);    ///< 失敗の文字
// ---- ミュート中（見出しの状態ラベル。色だけで伝えない。マイクに斜線の絵と文字も付ける） ----
constexpr Color kMuteFill = uiColor(frame_ui::kChipBadFill);     ///< 地
constexpr Color kMuteText = uiColor(frame_ui::kChipBadText);     ///< 文字・絵
// ---- 未使用・読み込み中の状態ラベル ----
constexpr Color kIdleFill = uiColor(frame_ui::kChipIdleFill);
constexpr Color kIdleText = uiColor(frame_ui::kChipIdleText);
// ---- 重ねた画面の後ろを暗くする色と不透明度 ----
constexpr Color kBackdrop = hexColor(0x06070a);
constexpr double kBackdropAlpha = 0.74;

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
