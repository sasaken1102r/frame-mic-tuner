// 色の定義とコントラスト比の計算の実装。
#include "theme.h"

#include <cmath>
#include <cstdio>

namespace {

/**
 * sRGB の 1 チャンネルを線形の値にする（WCAG 2.x の式）。
 * @param c 0〜1
 * @return 線形の値
 */
double linearChannel(double c) {
    return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}

/**
 * 色を #rrggbb にする（表示用）。
 * @param c 色
 * @return 文字列
 */
std::string hexText(Color c) {
    char text[16];
    std::snprintf(text, sizeof(text), "#%02x%02x%02x", static_cast<int>(std::lround(c.r * 255)),
                  static_cast<int>(std::lround(c.g * 255)), static_cast<int>(std::lround(c.b * 255)));
    return text;
}

/**
 * 種類の名前（表示用）。
 * @param kind 種類
 * @return 名前
 */
const char* kindName(ContrastKind kind) {
    switch (kind) {
        case ContrastKind::Text: return "文字";
        case ContrastKind::LargeText: return "大きい文字";
        case ContrastKind::Ui: return "部品";
        case ContrastKind::Disabled: return "無効（目安）";
    }
    return "";
}

}  // namespace

double relativeLuminance(Color c) {
    return 0.2126 * linearChannel(c.r) + 0.7152 * linearChannel(c.g) + 0.0722 * linearChannel(c.b);
}

double contrastRatio(Color a, Color b) {
    const double la = relativeLuminance(a);
    const double lb = relativeLuminance(b);
    const double light = la > lb ? la : lb;
    const double dark = la > lb ? lb : la;
    return (light + 0.05) / (dark + 0.05);
}

double requiredRatio(ContrastKind kind) {
    return kind == ContrastKind::Text ? 4.5 : 3.0;
}

const std::vector<ContrastPair>& contrastPairs() {
    // 描画で使っている組み合わせをすべて並べる（文字の大きさによらず、文字はすべて 4.5:1 で確かめる）
    static const std::vector<ContrastPair> pairs = {
        // 地・カードの上の文字
        {"見出し・本文（パネルの地）", kText, kBg, ContrastKind::Text},
        {"本文（カード）", kText, kCard, ContrastKind::Text},
        {"補足の文字（パネルの地）", kTextMuted, kBg, ContrastKind::Text},
        {"補足の文字（カード）", kTextMuted, kCard, ContrastKind::Text},
        // ボタン・ピル
        {"ボタンの文字", kText, kControl, ContrastKind::Text},
        {"ボタンの補足の文字", kTextMuted, kControl, ContrastKind::Text},
        {"ボタンの文字（ポインターが乗っている・押している）", kText, kControlHover, ContrastKind::Text},
        {"選択中の文字（アクセントの塗り）", kOnAccent, kAccent, ContrastKind::Text},
        {"選択中の文字（押している間）", kOnAccent, kAccentPressed, ContrastKind::Text},
        {"押せないボタンの文字", kTextDisabled, kControl, ContrastKind::Disabled},
        // アクセント色の文字
        {"アクセントの文字（パネルの地）", kAccent, kBg, ContrastKind::Text},
        {"アクセントの文字（カード）", kAccent, kCard, ContrastKind::Text},
        {"通っている段の文字（アクセントの薄い塗り）", kText, kAccentTint, ContrastKind::Text},
        // 状態
        {"「使用中」のバッジ", kSuccess, kSuccessTint, ContrastKind::Text},
        {"「未使用」のバッジ", kTextMuted, kControl, ContrastKind::Text},
        {"失敗・「録音中」の文字（カード）", kDanger, kCard, ContrastKind::Text},
        {"失敗の文字（パネルの地）", kDanger, kBg, ContrastKind::Text},
        {"録音中の停止ボタンの文字", kText, kDangerTint, ContrastKind::Text},
        {"終了ボタンの文字", kText, kQuitFill, ContrastKind::Text},
        {"終了の確認中の文字（赤い塗り）", kOnAccent, kDanger, ContrastKind::Text},
        // 部品の見分け（WCAG 1.4.11）
        {"ボタン・ピルの枠（カード）", kBorder, kCard, ContrastKind::Ui},
        {"ボタン・ピルの枠（パネルの地）", kBorder, kBg, ContrastKind::Ui},
        {"選択中の塗り（カード）", kAccent, kCard, ContrastKind::Ui},
        {"選択中の塗り（ピルの地）", kAccent, kControl, ContrastKind::Ui},
        {"選択中の塗り（パネルの地）", kAccent, kBg, ContrastKind::Ui},
        {"録音の ● と停止ボタンの枠", kDanger, kControl, ContrastKind::Ui},
        {"録音中の停止ボタンの枠（カード）", kDanger, kCard, ContrastKind::Ui},
        {"「使用中」の ●（バッジの塗り）", kSuccess, kSuccessTint, ContrastKind::Ui},
        {"「未使用」の ○（バッジの塗り）", kTextMuted, kControl, ContrastKind::Ui},
        {"終了ボタンの枠（パネルの地）", kDanger, kBg, ContrastKind::Ui},
        {"選ばれていないカードの枠（パネルの地）", kBorder, kBg, ContrastKind::Ui},
        {"選ばれていないカードを押している間の枠（パネルの地）", kAccent, kBg, ContrastKind::Ui},
        {"録音のボタンの枠（カード）", kDanger, kCard, ContrastKind::Ui},
        {"つながりの線（通っている）", kAccent, kCard, ContrastKind::Ui},
        {"つながりの線・段の枠（通っていない、点線）", kBorder, kCard, ContrastKind::Ui},
        {"音量メーターの塗り（メーターの地）", kAccent, kBg, ContrastKind::Ui},
        {"波形（カード）", kTextMuted, kCard, ContrastKind::Ui},
        {"波形の再生済み・再生位置の線（カード）", kAccent, kCard, ContrastKind::Ui},
        {"再生ボタンの ▶（ボタンの地）", kText, kControl, ContrastKind::Ui},
        // イヤホン / スピーカーのカード（プリセット）のチップ: 地の色のピルに文字
        {"チップの文字（選ばれていないカード）", kText, kBg, ContrastKind::Text},
        {"チップの文字（選択中のカードの上）", kAccent, kBg, ContrastKind::Text},
        {"チップの枠（選ばれていないカード）", kBorder, kCard, ContrastKind::Ui},
        {"チップの枠（選ばれていないカードに乗っている間）", kBorder, kControl, ContrastKind::Ui},
        {"チップの地（選択中のカードの上）", kBg, kAccent, ContrastKind::Ui},
        {"プリセットの見出し・細かく調整の見出し", kTextMuted, kBg, ContrastKind::Text},
        // 左の列のタブ（かんたん / 細かく調整。ピル型の切り替え）と、かんたんのタブの「細かく調整を見る →」
        {"タブの文字（選ばれていない）", kText, kControl, ContrastKind::Text},
        {"タブの文字（選択中、アクセントの塗り）", kOnAccent, kAccent, ContrastKind::Text},
        {"タブの枠（パネルの地）", kBorder, kBg, ContrastKind::Ui},
        {"タブの選択中の塗り（タブの地）", kAccent, kControl, ContrastKind::Ui},
        {"「細かく調整を見る →」の文字", kText, kControl, ContrastKind::Text},
        {"「細かく調整を見る →」の文字（乗っている間）", kText, kControlHover, ContrastKind::Text},
        {"「標準は 23%・500ms」の説明", kTextMuted, kBg, ContrastKind::Text},
        // どちらのプリセットとも一致しないときのグレーのカード（押せるまま。非活性の例外にせず、3:1 以上で読めるように）
        {"グレーのカードの文字・絵・チップの文字（地の塗り）", kTextDisabled, kBg, ContrastKind::Disabled},
        {"グレーのカードの文字（乗っている間）", kTextDisabled, kControl, ContrastKind::Disabled},
        {"グレーのカードの点線の枠・チップの枠", kBorder, kBg, ContrastKind::Ui},
        {"グレーのカードのチップの枠（乗っている間）", kBorder, kControl, ContrastKind::Ui},
        {"「今は細かく調整した設定です」", kText, kBg, ContrastKind::Text},
        // ノイズ除去の強さのバー（パネルの地の上）
        {"バーの値の文字", kText, kBg, ContrastKind::Text},
        {"バーの塗り（現在値まで）", kAccent, kBg, ContrastKind::Ui},
        {"バーの溝の枠", kBorder, kBg, ContrastKind::Ui},
        {"バーのつまみ", kText, kBg, ContrastKind::Ui},
        {"バーのつまみの縁（塗りの上で、つまみを見分ける）", kBg, kAccent, ContrastKind::Ui},
        {"ノイズ除去オフのときのバーの塗り・つまみ・値", kTextDisabled, kBg, ContrastKind::Disabled},
        {"− / ＋ / 標準に戻すの文字", kText, kControl, ContrastKind::Text},
        {"再生中の ■（アクセントの塗り）", kOnAccent, kAccent, ContrastKind::Ui},
        // 更新の帯（カード。枠は外がパネルの地、内がカード）
        {"更新の帯の文（最新・確認中・更新中・確認）", kText, kCard, ContrastKind::Text},
        {"更新の帯の補足（版だけ・2 行目）", kTextMuted, kCard, ContrastKind::Text},
        {"更新の帯の新しい版・入れ終わりの文", kAccent, kCard, ContrastKind::Text},
        {"更新の帯の失敗の文", kDanger, kCard, ContrastKind::Text},
        {"更新の帯のアクセントの枠（パネルの地）", kAccent, kBg, ContrastKind::Ui},
        {"更新の帯のアクセントの枠（カード）", kAccent, kCard, ContrastKind::Ui},
        {"更新の帯の赤い枠（パネルの地）", kDanger, kBg, ContrastKind::Ui},
        {"更新の帯の赤い枠（カード）", kDanger, kCard, ContrastKind::Ui},
        {"更新の帯のボタンの文字", kText, kControl, ContrastKind::Text},
        {"更新の帯のボタンの文字（乗っている・押している）", kText, kControlHover, ContrastKind::Text},
        {"更新の帯のボタンの枠（カード）", kBorder, kCard, ContrastKind::Ui},
        {"更新の帯の「更新する」の文字（アクセントの塗り）", kOnAccent, kAccent, ContrastKind::Text},
        {"更新の帯の「更新する」の文字（押している間）", kOnAccent, kAccentPressed, ContrastKind::Text},
        {"更新の帯の「更新する」の塗り（カード）", kAccent, kCard, ContrastKind::Ui},
        // ミュート中: 見出しのバッジ（パネルの地の上）と、更新の帯の場所に出すミュートの帯
        {"「ミュート中」のバッジの文字", kMuteText, kMuteFill, ContrastKind::Text},
        {"「ミュート中」のバッジの絵（マイクに斜線）", kMuteText, kMuteFill, ContrastKind::Ui},
        {"「ミュート中」のバッジの枠（パネルの地）", kMuteBorder, kBg, ContrastKind::Ui},
        {"「ミュート中」のバッジの枠（バッジの地）", kMuteBorder, kMuteFill, ContrastKind::Ui},
        {"ミュートの帯の文", kMuteText, kMuteFill, ContrastKind::Text},
        {"ミュートの帯の赤い枠（パネルの地）", kMuteBorder, kBg, ContrastKind::Ui},
        {"ミュートの帯の赤い枠（帯の地）", kMuteBorder, kMuteFill, ContrastKind::Ui},
        {"「ミュートを解除」の文字", kText, kControl, ContrastKind::Text},
        {"「ミュートを解除」の文字（乗っている・押している）", kText, kControlHover, ContrastKind::Text},
        {"「ミュートを解除」の枠（帯の地）", kBorder, kMuteFill, ContrastKind::Ui},
    };
    return pairs;
}

int printContrastReport() {
    int failures = 0;
    double lowest = 100.0;
    const ContrastPair* lowestPair = nullptr;
    std::printf("%-6s %-7s %-7s %6s %5s  %s\n", "結果", "文字色", "背景", "比", "必要", "どこで使っているか");
    for (const ContrastPair& pair : contrastPairs()) {
        const double ratio = contrastRatio(pair.fg, pair.bg);
        const double need = requiredRatio(pair.kind);
        const bool ok = ratio >= need;
        if (!ok) ++failures;
        if (ratio < lowest) {
            lowest = ratio;
            lowestPair = &pair;
        }
        std::printf("%-6s %s %s %6.2f %5.1f  %s（%s）\n", ok ? "合格" : "不合格", hexText(pair.fg).c_str(),
                    hexText(pair.bg).c_str(), ratio, need, pair.what, kindName(pair.kind));
    }
    // 共通の UI 部品（vendor/frame-ui）が使う組み合わせも、同じ式で確かめる
    for (const frame_ui::ContrastPair& pair : frame_ui::contrastPairs()) {
        const Color fg = uiColor(pair.fg);
        const Color bg = uiColor(pair.bg);
        const double ratio = contrastRatio(fg, bg);
        const bool ok = ratio >= pair.need;
        if (!ok) ++failures;
        std::printf("%-6s %s %s %6.2f %5.1f  frame-ui: %s\n", ok ? "合格" : "不合格", hexText(fg).c_str(),
                    hexText(bg).c_str(), ratio, pair.need, pair.what);
    }
    if (lowestPair != nullptr) {
        std::printf("いちばん低い比: %.2f（%s: %s / %s）\n", lowest, lowestPair->what, hexText(lowestPair->fg).c_str(),
                    hexText(lowestPair->bg).c_str());
    }
    std::printf("%zu 組中 %d 組が不合格\n", contrastPairs().size() + frame_ui::contrastPairs().size(), failures);
    return failures == 0 ? 0 : 1;
}
