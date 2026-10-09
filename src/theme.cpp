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

/** 重ねた画面の後ろ（パネルの地を暗くしたもの）。 */
constexpr Color kDimmedBg = blendColor(kBackdrop, kBg, kBackdropAlpha);

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
    // このアプリが自分で描く部分の組み合わせ（文字の大きさによらず、文字はすべて 4.5:1 で確かめる）。
    // 切り替え・ボタン・一番下の行は frame-ui の部品なので、frame-ui の組み合わせ（下）で確かめる
    static const std::vector<ContrastPair> pairs = {
        // 地・カード・一段暗い箱の上の文字
        {"見出し・本文（パネルの地）", kText, kBg, ContrastKind::Text},
        {"本文（カード・重ねた画面）", kText, kCard, ContrastKind::Text},
        {"本文（一段暗い箱・一覧の行）", kText, kInset, ContrastKind::Text},
        {"補足（カード）", kTextMuted, kCard, ContrastKind::Text},
        {"補足（一段暗い箱・一覧の行）", kTextMuted, kInset, ContrastKind::Text},
        {"控えめな本文（カード: 外付けのマイクの説明・リポジトリ）", kTextSoft, kCard, ContrastKind::Text},
        {"控えめな本文（一段暗い箱: 更新の文）", kTextSoft, kInset, ContrastKind::Text},
        {"一覧の区切りの見出し（重ねた画面）", kLabelSoft, kCard, ContrastKind::Text},
        // 見出しの状態ラベル（幅を決めてあり、ミュートしても動かない）
        {"状態ラベル「使用中」", kSuccess, kSuccessTint, ContrastKind::Text},
        {"状態ラベル「使用中」の ●", kSuccess, kSuccessTint, ContrastKind::Ui},
        {"状態ラベル「未使用」「読み込み中」", kIdleText, kIdleFill, ContrastKind::Text},
        {"状態ラベル「ミュート中」", kMuteText, kMuteFill, ContrastKind::Text},
        {"状態ラベル「ミュート中」のマイクに斜線の絵", kMuteText, kMuteFill, ContrastKind::Ui},
        // 音の出口・マイクの欄（パネルの地の上の、カードの色のボタン）
        {"欄の文字", kText, kCard, ContrastKind::Text},
        {"欄の文字（乗っている）", kText, kControlHover, ContrastKind::Text},
        {"欄の文字（押している）", kText, kControlDown, ContrastKind::Text},
        {"欄の小さな見出し・数", kTextMuted, kCard, ContrastKind::Text},
        {"欄の枠（パネルの地）", kBorder, kBg, ContrastKind::Ui},
        {"出口・マイクの絵（角丸の箱の上）", kText, kIconBox, ContrastKind::Ui},
        // プリセット（かんたん）: 選ばれていないものはボタンの地、選択中はピンクの塗り
        {"プリセットの名前（選ばれていない）", kText, kControl, ContrastKind::Text},
        {"プリセットの効果（選ばれていない）", kTextMuted, kControl, ContrastKind::Text},
        {"プリセットの名前（乗っている）", kText, kControlHover, ContrastKind::Text},
        {"プリセットの枠（カード）", kBorder, kCard, ContrastKind::Ui},
        {"プリセットの札の文字（選ばれていない）", kText, kBg, ContrastKind::Text},
        {"プリセットの札の枠（選ばれていない）", kBorder, kControl, ContrastKind::Ui},
        {"プリセットの名前・絵（選択中）", kOnAccent, kAccent, ContrastKind::Text},
        {"プリセットの名前（選択中に乗っている）", kOnAccent, kAccentHover, ContrastKind::Text},
        {"プリセットの名前（選択中を押している）", kOnAccent, kAccentPressed, ContrastKind::Text},
        {"プリセットの効果（選択中）", kOnAccentSoft, kAccent, ContrastKind::Text},
        {"プリセットの札の文字（選択中）", kAccentText, kOnAccent, ContrastKind::Text},
        {"選択中のプリセットの塗り（カード）", kAccent, kCard, ContrastKind::Ui},
        // 自動で切り替えた知らせ（ピンクの枠）
        {"知らせの文字（カード）", kText, kCard, ContrastKind::Text},
        {"知らせの枠・入れ替えの絵（カード）", kAccent, kCard, ContrastKind::Ui},
        // ノイズ除去の強さのバー（カードの上）
        {"バーの値（オン）", kText, kCard, ContrastKind::Text},
        {"バーの値（ノイズ除去がオフの間）", kTextMuted, kCard, ContrastKind::Text},
        {"バーの塗り（現在値まで）", kAccent, kCard, ContrastKind::Ui},
        {"バーのつまみ", kText, kCard, ContrastKind::Ui},
        {"バーの塗り・つまみ（ノイズ除去がオフの間）", kTextMuted, kCard, ContrastKind::Ui},
        // つながり（一段暗い箱）
        {"通っている段の文字（ピンクの薄い塗り）", kText, kAccentTint, ContrastKind::Text},
        {"通っている段の枠・線（一段暗い箱）", kAccent, kInset, ContrastKind::Ui},
        {"通っていない段の文字（一段暗い箱）", kTextMuted, kInset, ContrastKind::Text},
        {"通っていない段の点線の枠・線（一段暗い箱）", kBorder, kInset, ContrastKind::Ui},
        // 声のチェック（カード）
        {"録音のボタンの ●・枠（カード）", kDanger, kCard, ContrastKind::Ui},
        {"「録音中」の文字（カード）", kDangerText, kCard, ContrastKind::Text},
        {"録音中の「停止」（赤い塗り）", kOnAccent, kDanger, ContrastKind::Text},
        {"音量メーターの塗り（パネルの地の溝）", kAccent, kBg, ContrastKind::Ui},
        {"再生のボタンの枠（カード）", kBorder, kCard, ContrastKind::Ui},
        {"再生のボタンの ▶（カード）", kText, kCard, ContrastKind::Ui},
        {"再生中の ■（ピンクの塗り）", kOnAccent, kAccent, ContrastKind::Ui},
        {"履歴の 2 行目（カード）", kTextSoft, kCard, ContrastKind::Text},
        {"波形（カード）", kTextMuted, kCard, ContrastKind::Ui},
        {"波形の再生済みの部分（カード）", kAccent, kCard, ContrastKind::Ui},
        // 失敗
        {"失敗の文字（パネルの地。一番下の行）", kDangerText, kBg, ContrastKind::Text},
        {"失敗の文字（カード）", kDangerText, kCard, ContrastKind::Text},
        {"失敗の文字（一段暗い箱）", kDangerText, kInset, ContrastKind::Text},
        // 重ねた画面（音の出口を選ぶ・使うマイク）
        {"重ねた画面の枠（後ろの暗い地）", kBorder, kDimmedBg, ContrastKind::Ui},
        {"重ねた画面の枠（重ねた画面の地）", kBorder, kCard, ContrastKind::Ui},
        {"一覧の行の枠（設定を表示中、ピンク）", kAccent, kCard, ContrastKind::Ui},
        {"一覧の行の文字（設定を表示中）", kText, kRowSelected, ContrastKind::Text},
        {"一覧の行の補足（設定を表示中）", kTextMuted, kRowSelected, ContrastKind::Text},
        {"札「設定を表示中」「このアプリで使う」", kAccentText, kAccentTint, ContrastKind::Text},
        {"行の状態「いま使用中」の文字と ●", kSuccess, kInset, ContrastKind::Text},
        {"行の状態「いま使用中」の文字と ●（設定を表示中の行）", kSuccess, kRowSelected, ContrastKind::Text},
        {"スクロールバーのつまみ（溝の上）", kBorder, kIconBox, ContrastKind::Ui},
        // アプリと更新（一段暗い箱の更新の表示）
        {"新しい版があります（一段暗い箱）", kAccentText, kInset, ContrastKind::Text},
        {"更新の箱のピンクの枠（カード）", kAccent, kCard, ContrastKind::Ui},
        {"更新の箱の赤い枠（カード）", kDanger, kCard, ContrastKind::Ui},
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
        std::printf("いちばん低い比（このアプリの分）: %.2f（%s: %s / %s）\n", lowest, lowestPair->what,
                    hexText(lowestPair->fg).c_str(), hexText(lowestPair->bg).c_str());
    }
    std::printf("%zu 組中 %d 組が不合格\n", contrastPairs().size() + frame_ui::contrastPairs().size(), failures);
    return failures == 0 ? 0 : 1;
}
