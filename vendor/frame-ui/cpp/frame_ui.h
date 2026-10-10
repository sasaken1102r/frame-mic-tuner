// SPDX-License-Identifier: MIT — part of frame-ui by sasaken1102r, shipped under the host app's MIT license
// Steam Frame のパネル（frame-mic-tuner・frame-perf-overlay）で共通の見た目の部品。
// frame-ui リポジトリの原稿を各アプリの vendor/frame-ui/ に sync.sh でコピーして使う。コピー側は直さない。
//
// 依存: cairo と、vendor/frame-updater/cpp/update_check.h（更新の帯が UpdateStatus を読む）だけ。
// フォントはアプリが読み込んで cairo_font_face_t* で渡す（FreeType もここでは使わない）。
// 使い方の流れ（くわしくは README.md）:
//   frame_ui::Canvas ui{cr, fonts.regular(), fonts.bold(), pointer};
//   const auto langHits = frame_ui::drawSegmented(ui, rect, {"日本語", "English"}, 0);
//   押されたら: langHits[1].contains(x, y) → 英語に
// すべての部品は「描いて、当たり判定の矩形を返す」だけ。状態（選択中・確認中など）はアプリが持つ。
#pragma once

#include <cairo.h>

#include "update_check.h"  // vendor/frame-updater/cpp

#include <string>
#include <vector>

/** frame-ui の版（sync.sh が UPSTREAM に書く）。 */
#define FRAME_UI_VERSION "0.2.0"

namespace frame_ui {

// ============================================================================
// 色と寸法（見本「共通の部品」のとおり。px はパネルの画像の px）
// ============================================================================

/** RGB の色（0〜1）。 */
struct Rgb {
    double r, g, b;
};

/**
 * 16 進の RGB から色を作る。
 * @param rgb 0xRRGGBB
 * @return 色
 */
constexpr Rgb hex(unsigned rgb) {
    return {((rgb >> 16) & 0xFF) / 255.0, ((rgb >> 8) & 0xFF) / 255.0, (rgb & 0xFF) / 255.0};
}

// ---- 地 ----
constexpr Rgb kBg = hex(0x0e1015);    ///< パネルの地（外）
constexpr Rgb kCard = hex(0x1a1d24);  ///< カード・更新の帯
// ---- 文字 ----
constexpr Rgb kText = hex(0xf2f3f5);        ///< 本文・見出し・値・ボタンの文字
constexpr Rgb kTextMuted = hex(0xa3a9b4);   ///< 説明（14px）・カードの右の補足
constexpr Rgb kTextSoft = hex(0xdfe2e7);    ///< 更新の帯のふだんの文
constexpr Rgb kLabelSoft = hex(0xc9cdd4);   ///< 一番下の行の項目名（言語・SteamVR と一緒に起動）
constexpr Rgb kOptionText = hex(0xe6e8ec);  ///< 切り替えの選ばれていない側の文字
// ---- 部品 ----
constexpr Rgb kBorder = hex(0x6e7681);        ///< 切り替え・増減・ボタンの枠（2px。カードの上で 3.67:1、地の上で 4.14:1）
constexpr Rgb kDivider = hex(0x3a3f4a);       ///< 増減の値の左右の区切り線（1px）
constexpr Rgb kControl = hex(0x20242c);       ///< 普通のボタンの地
constexpr Rgb kControlHover = hex(0x2a2f38);  ///< ポインターが乗っている
constexpr Rgb kControlDown = hex(0x343944);   ///< 押している
// ---- イメージカラー（ピンク） ----
constexpr Rgb kAccent = hex(0xe07ef5);        ///< 選択中・強調のボタンの塗り・新しい版の枠
constexpr Rgb kAccentHover = hex(0xe99af8);   ///< 塗りに乗っている
constexpr Rgb kAccentDown = hex(0xc865de);    ///< 塗りを押している
constexpr Rgb kOnAccent = hex(0x1c0f22);      ///< ピンク・赤の塗りの上の文字
constexpr Rgb kAccentText = hex(0xf0c4fa);    ///< 「新しい版があります」の文字
// ---- 危ない操作・失敗 ----
constexpr Rgb kDanger = hex(0xe5534b);         ///< 危ないボタンの枠・失敗の枠・「もう一度押すと終了」の塗り
constexpr Rgb kDangerFill = hex(0x2a1518);     ///< 危ないボタンの地
constexpr Rgb kDangerHover = hex(0x3a1c1f);    ///< 危ないボタンに乗っている
constexpr Rgb kDangerDown = hex(0x4a2024);     ///< 危ないボタンを押している
constexpr Rgb kDangerText = hex(0xff8a80);     ///< 失敗の文字
// ---- 状態ラベル ----
constexpr Rgb kChipOkFill = hex(0x173424);    ///< 緑（動作中）の地
constexpr Rgb kChipOkText = hex(0x4ade80);    ///< 緑の文字と点
constexpr Rgb kChipIdleFill = hex(0x262a33);  ///< 灰（止まっている）の地
constexpr Rgb kChipIdleText = hex(0xc9cdd4);  ///< 灰の文字と輪
constexpr Rgb kChipBadFill = hex(0x3a1a1c);   ///< 赤（問題）の地
constexpr Rgb kChipBadText = hex(0xff8a80);   ///< 赤の文字
constexpr Rgb kChipBadDot = hex(0xff6b61);    ///< 赤の点

// ---- 骨組み（幅 1200 のパネル） ----
constexpr double kPanelPadX = 28;    ///< パネルの左右の余白
constexpr double kPanelPadTop = 24;  ///< パネルの上の余白（下も同じ）
constexpr double kGap = 16;          ///< 段と段・カードとカードの間
constexpr double kHeaderH = 52;      ///< 見出しの行
constexpr double kTitleSize = 32;    ///< アプリ名
constexpr double kUpdateBarH = 72;   ///< 更新の帯
constexpr double kFooterRowH = 60;   ///< 一番下の行（言語・自動起動・終了）
constexpr double kFooterH = 88;      ///< 一番下の行 ＋ 注意書き 1 行
// ---- カード ----
constexpr double kCardRadius = 18;
constexpr double kCardPadX = 22;
constexpr double kCardPadY = 20;
constexpr double kCardTitleH = 32;  ///< カードの見出しの行の高さ（drawCard はこの下の y を返す）
// ---- 文字の大きさ ----
constexpr double kCardTitleSize = 22;  ///< カードの見出し（太字）
constexpr double kLabelSize = 20;      ///< 項目名（太字）
constexpr double kValueSize = 22;      ///< 増減の値（太字）
constexpr double kControlSize = 19;    ///< 切り替え・ボタン・状態ラベル・更新の帯の文字
constexpr double kHintSize = 14;       ///< 説明・注意書き
constexpr double kFooterLabelSize = 18;
// ---- 部品 ----
constexpr double kControlH = 56;        ///< 切り替え・増減・ボタンの高さ（当たり判定の最小）
constexpr double kSegRadius = 30;       ///< 切り替えの外枠の角丸（高さ 56 なので実質カプセル）
constexpr double kSegInset = 5;         ///< 切り替えの外枠と選択中の塗りの間
constexpr double kStepperButtonW = 60;  ///< 増減の［−］［＋］の幅
constexpr double kButtonRadius = 16;
constexpr double kButtonPadX = 24;  ///< ボタンの文字の左右の余白（buttonWidth）
constexpr double kChipH = 44;
constexpr double kDisabledAlpha = 0.45;  ///< 使えない部品の不透明度

// ============================================================================
// 描く道具
// ============================================================================

/** 矩形（当たり判定もこれ。w か h が 0 なら何にも当たらない）。 */
struct Rect {
    double x = 0, y = 0, w = 0, h = 0;

    /**
     * 点が中にあるか。
     * @param px x
     * @param py y
     * @return 中なら true（w・h が 0 なら常に false）
     */
    bool contains(double px, double py) const { return w > 0 && h > 0 && px >= x && px < x + w && py >= y && py < y + h; }
    /** @return 右端 */
    double right() const { return x + w; }
    /** @return 下端 */
    double bottom() const { return y + h; }
};

/** レーザーポインターの今（乗っている・押している見た目に使う）。 */
struct Pointer {
    double x = -1, y = -1;  ///< パネルの画像の px
    bool inside = false;    ///< パネルに乗っているか
    bool down = false;      ///< トリガーを押しているか
};

/**
 * 描く先（cairo）とフォントとポインター。部品の関数はすべてこれを受け取る。
 * フォントはアプリの FontSet のもの（regular() / bold()）をそのまま渡す。持ち主はアプリ。
 */
struct Canvas {
    cairo_t* cr;
    cairo_font_face_t* regular;
    cairo_font_face_t* bold;
    Pointer pointer {};

    /**
     * 文字列の幅を測る。
     * @param text UTF-8
     * @param size 大きさ（px）
     * @param isBold 太字か
     * @return 幅（px）
     */
    double measure(const std::string& text, double size, bool isBold = false) const;

    /**
     * 文字を描く。
     * @param x 左端（alignRight なら右端）
     * @param baseline ベースライン
     * @param text UTF-8
     * @param size 大きさ（px）
     * @param c 色
     * @param isBold 太字か
     * @param alignRight 右揃えか
     * @return 描いた幅（px）
     */
    double text(double x, double baseline, const std::string& text, double size, Rgb c, bool isBold = false,
                bool alignRight = false) const;

    /**
     * 幅に収まるまで文字を小さくした大きさ。
     * @param text UTF-8
     * @param size 最初の大きさ
     * @param minSize これより小さくしない
     * @param maxWidth 収めたい幅
     * @param isBold 太字か
     * @return 大きさ
     */
    double fit(const std::string& text, double size, double minSize, double maxWidth, bool isBold = false) const;

    /**
     * 幅に収まらなければ末尾を「…」にした文字列（fit で小さくしても入らないとき用）。
     * @param text UTF-8
     * @param size 大きさ
     * @param maxWidth 幅
     * @param isBold 太字か
     * @return 収まる文字列
     */
    std::string ellipsize(const std::string& text, double size, double maxWidth, bool isBold = false) const;
};

/**
 * 文字を高さ h の帯の縦の真ん中に置くときのベースライン（Noto Sans CJK 向け。2 アプリの centerBaseline と同じ式）。
 * @param top 帯の上
 * @param h 帯の高さ
 * @param size 文字の大きさ
 * @return ベースラインの y
 */
double centerBaseline(double top, double h, double size);

/**
 * 角の丸い四角の道筋を作る（塗る・線を引くのは呼ぶ側）。
 * @param cr cairo
 * @param r 矩形
 * @param radius 角の半径（高さの半分を超えたら半分にする）
 */
void roundedRect(cairo_t* cr, Rect r, double radius);

/**
 * ✓ を線で描く（フォントに頼らない）。
 * @param cr cairo
 * @param cx 真ん中の x
 * @param cy 真ん中の y
 * @param s 大きさ（px）
 * @param c 色
 */
void drawCheck(cairo_t* cr, double cx, double cy, double s, Rgb c);

/**
 * パネル全体の地を塗る（角丸 24）。
 * @param ui 描く先
 * @param w パネルの幅
 * @param h パネルの高さ
 */
void drawPanelBackground(const Canvas& ui, double w, double h);

// ============================================================================
// 部品（描いて、当たり判定を返す。使えない部分の当たり判定は w = h = 0）
// ============================================================================

/**
 * 切り替え（2 つ以上の選択肢を 1 本のカプセルに並べる。選択中はピンク塗り ＋ ✓）。
 * @param ui 描く先
 * @param r 外枠（高さ kControlH）
 * @param labels 選択肢の文言（左から）
 * @param selected 選択中の番号（-1 ならどれも選ばない）
 * @param enabled 使えるか（false なら全体を薄くし、当たり判定は空）
 * @return 選択肢ごとの当たり判定（外枠を等分した、高さいっぱいの矩形）
 */
std::vector<Rect> drawSegmented(const Canvas& ui, Rect r, const std::vector<std::string>& labels, int selected,
                                bool enabled = true);

/** 数値の増減の当たり判定。 */
struct StepperHits {
    Rect minus;  ///< ［−］
    Rect plus;   ///< ［＋］
};

/**
 * 数値の増減（カプセルに「− 値 ＋」）。
 * @param ui 描く先
 * @param r 外枠（高さ kControlH、幅は 60 + 値 + 60）
 * @param value 値の文言（「18 cm」「23%」）
 * @param enabled 使えるか（false なら全体を薄く・当たり判定は空）
 * @param canMinus まだ下げられるか（false なら − だけ薄く・当たり判定は空）
 * @param canPlus まだ上げられるか
 * @return ［−］［＋］の当たり判定
 */
StepperHits drawStepper(const Canvas& ui, Rect r, const std::string& value, bool enabled = true, bool canMinus = true,
                        bool canPlus = true);

/** ボタンの種類。 */
enum class ButtonKind {
    Normal,       ///< 地 #20242c・枠 #6e7681
    Primary,      ///< ピンクの塗り（「更新する」）
    Selected,     ///< ピンクの塗り ＋ ✓（今その状態になっている選択肢。perf の位置・向き）
    Danger,       ///< 赤い枠（「終了」「録音」）
    DangerArmed,  ///< 赤い塗り（「もう一度押すと終了」の確認中）
};

/**
 * 文言が入るボタンの幅（文字 ＋ 左右 kButtonPadX）。
 * @param ui 描く先（文字を測るだけ）
 * @param label 文言
 * @param minWidth これより狭くしない
 * @return 幅
 */
double buttonWidth(const Canvas& ui, const std::string& label, double minWidth = 0);

/**
 * ボタン（角丸 16、文字は 19px 太字。入りきらなければ小さくする）。
 * @param ui 描く先
 * @param r 矩形（高さ kControlH 以上）
 * @param label 文言
 * @param kind 種類
 * @param enabled 使えるか（false なら薄く・当たり判定は空）
 * @return 当たり判定
 */
Rect drawButton(const Canvas& ui, Rect r, const std::string& label, ButtonKind kind = ButtonKind::Normal,
                bool enabled = true);

/** カードの枠の色。 */
enum class Edge {
    None,  ///< 枠なし
    Pink,  ///< ピンク 2px（新しい版がある・確認中）
    Red,   ///< 赤 2px（失敗）
};

/**
 * カード（地 #1a1d24・角丸 18）。見出しを渡せば左上に 22px 太字、補足を右に 14px 灰で描く。
 * @param ui 描く先
 * @param r 矩形
 * @param title 見出し（空なら描かない）
 * @param sub 右の補足（空なら描かない。見出しと重なるなら小さくする）
 * @param edge 枠
 * @return 中身を置き始める y（見出しがあれば見出しの行の下、無ければ上の余白の下）。左は r.x + kCardPadX
 */
double drawCard(const Canvas& ui, Rect r, const std::string& title = "", const std::string& sub = "",
                Edge edge = Edge::None);

/** 状態ラベルの色。 */
enum class Tone {
    Ok,    ///< 緑（動作中）: ● ＋ 文字
    Idle,  ///< 灰（止まっている）: ○ ＋ 文字
    Bad,   ///< 赤（問題）: ● ＋ 文字
};

/**
 * 状態ラベルの幅。
 * @param ui 描く先
 * @param label 文言
 * @return 幅
 */
double chipWidth(const Canvas& ui, const std::string& label);

/**
 * 状態ラベル（高さ 44・角丸 22・点 12px）。押せない。
 * @param ui 描く先
 * @param right 右端の x
 * @param centerY 縦の真ん中
 * @param label 文言
 * @param tone 色
 * @return 描いた矩形
 */
Rect drawChip(const Canvas& ui, double right, double centerY, const std::string& label, Tone tone);

/**
 * 見出しの行（左にアプリ名 32px 太字、右に状態ラベル）。
 * @param ui 描く先
 * @param r 行（高さ kHeaderH。左右はパネルの余白の内側）
 * @param title アプリ名
 * @param chipLabel 状態ラベルの文言（空なら出さない）
 * @param tone 状態ラベルの色
 */
void drawHeader(const Canvas& ui, Rect r, const std::string& title, const std::string& chipLabel, Tone tone);

// ============================================================================
// 文言（日本語・英語・簡体字中国語）
// ============================================================================

/** 表示の言語。 */
enum class Lang {
    Ja,  ///< 日本語
    En,  ///< 英語
    Sc,  ///< 簡体字中国語
};

/** 2 アプリで共通の文言（更新の帯は frame-updater の strings.md のとおり）。 */
struct Strings {
    // ---- 一番下の行・共通のボタン ----
    const char* language;          ///< 言語 / Language
    const char* japanese;          ///< 日本語（どの言語でもこの字）
    const char* english;           ///< English（どの言語でもこの字）
    const char* simplifiedChinese; ///< 简体中文（どの言語でもこの字）
    const char* startWithSteamVr;  ///< SteamVR と一緒に起動 / Start with SteamVR
    const char* on;                ///< オン / On
    const char* off;               ///< オフ / Off
    const char* quit;              ///< 終了 / Quit
    const char* quitConfirm;       ///< もう一度押すと終了 / Press again to quit
    const char* reset;             ///< 既定に戻す / Reset
    // ---- 更新の帯（strings.md） ----
    const char* updateUpToDateFormat;    ///< 最新版です（%s）
    const char* updateChecking;          ///< 新しい版を確かめています…
    const char* updateAvailableFormat;   ///< 新しい版 %s があります
    const char* updateButton;            ///< 更新する
    const char* updateManual;            ///< ここからは入れられない版です。GitHub から手で更新してね
    const char* updateConfirmFormat;     ///< %s に更新しますか？
    const char* updateConfirmYes;        ///< 更新する
    const char* updateConfirmNo;         ///< やめる
    const char* updateInstallingFormat;  ///< 更新中: %s
    const char* updateInstallingHint;    ///< この画面が閉じて開き直すことがあります（updateConfirmHint を帯の幅に縮めたもの）
    const char* updateInstalledFormat;   ///< %s を入れました。開き直すと新しい版になります
    const char* updateInstallFailed;     ///< 更新できませんでした（今の版のままです）:
    const char* updateCheckFailed;       ///< 新しい版を確かめられませんでした:
    const char* updateCheckNow;          ///< 今すぐ確かめる
    const char* updateRetry;             ///< もう一度
    const char* updateDismiss;           ///< 閉じる
    const char* updateCurrentFormat;     ///< 今の版: %s（確認がオフでまだ何も分からないとき。strings.md に無い frame-ui の追加）
};

/**
 * 言語の文言の表。
 * @param lang 言語
 * @return 表（プログラムの終わりまで有効）
 */
const Strings& strings(Lang lang);

/**
 * 更新の手順（UpdateStatus::step）の文言。知らない手順はそのまま返す。
 * @param lang 言語
 * @param step "start" / "download" / "verify" / "extract" / "install"
 * @return 文言
 */
std::string updateStepText(Lang lang, const std::string& step);

/**
 * 更新の失敗の理由（UpdateStatus::error）の文言。知らないコードは "other" の文言。
 * @param lang 言語
 * @param error strings.md の理由のコード
 * @return 文言
 */
std::string updateErrorText(Lang lang, const std::string& error);

// ============================================================================
// 更新の帯と一番下の行
// ============================================================================

/** 更新の帯のボタンが表す操作。 */
enum class UpdateAction {
    None,
    CheckNow,    ///< 今すぐ確かめる → updater.checkNow()
    Install,     ///< 更新する（1 回目）→ アプリが confirming = true にする
    ConfirmYes,  ///< 確認の「更新する」→ updater.install()、confirming = false
    ConfirmNo,   ///< 確認の「やめる」→ confirming = false
    Retry,       ///< 失敗のあとの「もう一度」→ updater.install()
    Dismiss,     ///< 「閉じる」→ updater.dismiss()
};

/** 更新の帯のボタン 1 つの当たり判定。 */
struct UpdateHit {
    UpdateAction action;
    Rect rect;
};

/**
 * 更新の帯（カード、高さ 72）。左に状態の文、右にボタン。状態ごとの見た目は README の表のとおり。
 * @param ui 描く先
 * @param r 帯（高さ kUpdateBarH）
 * @param status updater.status()
 * @param confirming 「更新する」を 1 回押して確認を出している間（アプリが持つ。Available かつ installable のときだけ効く）
 * @param lang 言語
 * @return ボタンの当たり判定（押せるものだけ）
 */
std::vector<UpdateHit> drawUpdateBar(const Canvas& ui, Rect r, const frame_updater::UpdateStatus& status,
                                     bool confirming, Lang lang);

/** 一番下の行に出すもの。 */
struct FooterView {
    Lang lang = Lang::Ja;           ///< 今の言語（切り替えの選択中と、文言の言語）
    bool offerSimplifiedChinese = true;  ///< 言語の切り替えに「简体中文」も出すか（false なら日本語・English の 2 つ）
    int autostart = -1;             ///< SteamVR と一緒に起動: 1 = オン、0 = オフ、-1 = 分からない（どちらも選ばない）
    bool autostartEnabled = true;   ///< 自動起動の切り替えを使えるか（ユニットが無い・切り替え中なら false）
    bool quitArmed = false;         ///< 「終了」を 1 回押して確認中か（アプリが持つ。数秒で戻す）
    std::string note;               ///< 下の注意書き 1 行（幅に入らなければ小さく・末尾を「…」）
    bool noteIsError = false;       ///< 注意書きを失敗の色（赤・太字）にする
};

/** 一番下の行の当たり判定。 */
struct FooterHits {
    Rect langJa;
    Rect langEn;
    Rect langSc;  ///< offerSimplifiedChinese が false なら空
    Rect autostartOn;
    Rect autostartOff;
    Rect quit;  ///< 1 回目で quitArmed = true、確認中の 2 回目で終了
};

/**
 * 一番下の行（「言語 [日本語|English|简体中文]」「SteamVR と一緒に起動 [オン|オフ]」「［終了］」）と注意書き 1 行。
 * @param ui 描く先
 * @param r 行（高さ kFooterH。上の kFooterRowH が部品、その下に注意書き）
 * @param view 出すもの
 * @return 当たり判定
 */
FooterHits drawFooter(const Canvas& ui, Rect r, const FooterView& view);

// ============================================================================
// コントラスト（各アプリの --contrast-report に足す用）
// ============================================================================

/** 部品で使っている色の組み合わせ 1 つ。 */
struct ContrastPair {
    const char* what;  ///< どこで使っているか
    Rgb fg;
    Rgb bg;
    double need;  ///< 必要な比（文字 4.5、部品の見分け 3.0）
};

/**
 * WCAG 2.x のコントラスト比。
 * @param a 色
 * @param b 色
 * @return 1〜21
 */
double contrastRatio(Rgb a, Rgb b);

/**
 * frame-ui の部品が使う組み合わせの一覧。
 * @return 一覧
 */
const std::vector<ContrastPair>& contrastPairs();

}  // namespace frame_ui
