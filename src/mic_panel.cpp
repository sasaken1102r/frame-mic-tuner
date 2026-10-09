// マイクのパネルの実装。色は theme.h の定数だけを使う（--contrast-report の組み合わせと対応させる）。
#include "mic_panel.h"

#include "draw.h"
#include "i18n.h"
#include "theme.h"

#include <cairo.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>

namespace {

// 横長の 2 カラム: 左 = 切り替え、右 = 声のチェック、下 = 更新の帯、その下 = 全幅の 1 行（言語・自動起動・終了）と注意書き
constexpr int kWidth = 1200;
constexpr int kHeight = 788;
constexpr double kPad = 28;                  // パネルの外側の余白
constexpr double kCardPad = 20;              // カードの中の余白
constexpr double kLeftX = kPad;              // 左のカラム
constexpr double kLeftRight = 604;
constexpr double kRightX = 628;              // 右のカラム（声のチェックのカード）
constexpr double kRight = kWidth - kPad;     // 右端（ここまでに収める）

constexpr double kTabY = 76;         // 「かんたん」「細かく調整」のタブ
constexpr double kTabH = 48;
// かんたんのタブ
constexpr double kPresetHeadY = 162; // 「どこで音を聞いてる？」の見出し（ベースライン）
constexpr double kModeY = 174;       // イヤホン / スピーカーのカード（プリセット。全幅で縦に 2 枚）
constexpr double kModeH = 110;
constexpr double kModeGap = 10;
constexpr double kCustomRowY = 414;  // カードの下の 1 行（説明、または「細かく調整を見る →」）
// 細かく調整のタブ
constexpr double kToggleY = 146;     // エコー除去の行（次の行は +kToggleStep）
constexpr double kToggleStep = 64;
constexpr double kSegmentX = 172;    // スライド式の左端
constexpr double kSegmentW = 210;
constexpr double kSegmentH = 52;
constexpr double kSliderY = 286;     // ノイズ除去の強さのバー（2 本目は +kSliderStep）
constexpr double kSliderStep = 56;
constexpr double kSliderH = 48;      // 1 本の行の高さ
constexpr double kPipelineY = 458;   // つながりのカード
constexpr double kPipelineH = 116;
constexpr double kVoiceY = 26;       // 声のチェックのカード
constexpr double kVoiceH = 548;
constexpr double kRowStep = 74;      // 履歴の 1 行
constexpr double kUpdateRowY = 594;  // 更新の帯（全幅のカード。今の版・新しい版の確認と更新。ミュート中はミュートの帯）
constexpr double kUpdateRowH = 70;
constexpr double kFooterY = 682;     // 下の 1 行（言語・自動起動・終了）

/**
 * 収まる幅になるまで文字を小さくした大きさを返す。
 * @param pen 描画の道具
 * @param text 文字列
 * @param size 最初の大きさ
 * @param minSize これより小さくしない
 * @param maxWidth 収めたい幅（px）
 * @param bold 太字か
 * @return 文字の大きさ
 */
double fitSize(const Pen& pen, const std::string& text, double size, double minSize, double maxWidth, bool bold) {
    while (size > minSize && pen.measure(text, size, bold) > maxWidth) size -= 1;
    return size;
}

/**
 * 文字を、ある高さの帯の縦の真ん中に置いたときのベースライン。
 * @param top 帯の上
 * @param h 帯の高さ
 * @param size 文字の大きさ
 * @return ベースラインの y
 */
double centerBaseline(double top, double h, double size) {
    return top + h / 2 + size * 0.36;
}

/**
 * 文字を横の真ん中にそろえて描く。
 * @param pen 描画の道具
 * @param cx 真ん中の x
 * @param baseline ベースライン
 * @param text 文字列
 * @param size 大きさ
 * @param c 色
 * @param bold 太字か
 */
void textCentered(const Pen& pen, double cx, double baseline, const std::string& text, double size, Color c,
                  bool bold) {
    pen.text(cx - pen.measure(text, size, bold) / 2, baseline, text, size, c, bold);
}

/**
 * カードを描く: 重ねた影で浮かせ、塗り、枠、内側の 1px のハイライト。
 * @param pen 描画の道具
 * @param x 左
 * @param y 上
 * @param w 幅
 * @param h 高さ
 * @param r 角の半径
 * @param fill 塗り
 * @param border 枠の色
 * @param borderWidth 枠の太さ（0 なら描かない）
 */
void drawCard(const Pen& pen, double x, double y, double w, double h, double r, Color fill, Color border,
              double borderWidth) {
    cairo_t* cr = pen.cr;
    // 影（下にずらした黒を 2 段重ねる）
    cairo_set_source_rgba(cr, 0, 0, 0, 0.22);
    pen.roundedRect(x - 2, y + 6, w + 4, h + 6, r + 2);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.30);
    pen.roundedRect(x, y + 2, w, h + 1, r);
    cairo_fill(cr);
    pen.color(fill);
    pen.roundedRect(x, y, w, h, r);
    cairo_fill(cr);
    if (borderWidth > 0) {
        pen.color(border);
        cairo_set_line_width(cr, borderWidth);
        pen.roundedRect(x + borderWidth / 2, y + borderWidth / 2, w - borderWidth, h - borderWidth, r - borderWidth / 2);
        cairo_stroke(cr);
    }
    // 内側のハイライト（上の縁だけ少し明るく）
    cairo_set_source_rgba(cr, 1, 1, 1, 0.07);
    cairo_set_line_width(cr, 1);
    const double inset = borderWidth + 0.5;
    cairo_move_to(cr, x + r, y + inset);
    cairo_line_to(cr, x + w - r, y + inset);
    cairo_stroke(cr);
}

/**
 * 選択中の部品のまわりに、アクセントの光彩を描く（薄い塗りを外へ広げて重ねる）。
 * @param pen 描画の道具
 * @param x 左
 * @param y 上
 * @param w 幅
 * @param h 高さ
 * @param r 角の半径
 * @param spread 広げる px
 */
void drawGlow(const Pen& pen, double x, double y, double w, double h, double r, double spread) {
    for (int i = 4; i >= 1; --i) {
        const double grow = spread * i / 4;
        pen.color(kAccent, kAccentTintAlpha / 4);
        pen.roundedRect(x - grow, y - grow, w + grow * 2, h + grow * 2, r + grow);
        cairo_fill(pen.cr);
    }
}

/**
 * ✓ を線で描く（フォントに頼らない）。
 * @param cr cairo
 * @param cx 真ん中の x
 * @param cy 真ん中の y
 * @param s 大きさ（px）
 * @param c 色
 */
void drawCheck(cairo_t* cr, double cx, double cy, double s, Color c) {
    cairo_set_source_rgb(cr, c.r, c.g, c.b);
    cairo_set_line_width(cr, s * 0.16);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_move_to(cr, cx - s * 0.36, cy + s * 0.02);
    cairo_line_to(cr, cx - s * 0.10, cy + s * 0.28);
    cairo_line_to(cr, cx + s * 0.38, cy - s * 0.26);
    cairo_stroke(cr);
}

/**
 * イヤホンの絵（左右 2 つのイヤーピースと、合わさるケーブル）。
 * @param cr cairo
 * @param cx 真ん中の x
 * @param cy 真ん中の y
 * @param s 大きさ（px、絵の高さのめやす）
 * @param c 色
 */
void drawEarphoneIcon(cairo_t* cr, double cx, double cy, double s, Color c) {
    cairo_set_source_rgb(cr, c.r, c.g, c.b);
    const double u = s / 72.0;
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    for (int side = -1; side <= 1; side += 2) {
        const double bx = cx + side * 17 * u;
        // イヤーピース（丸）と軸
        cairo_arc(cr, bx, cy - 18 * u, 13 * u, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_set_line_width(cr, 8 * u);
        cairo_move_to(cr, bx, cy - 10 * u);
        cairo_line_to(cr, bx, cy + 8 * u);
        cairo_stroke(cr);
        // ケーブル（下で 1 本に合わさる）
        cairo_set_line_width(cr, 4 * u);
        cairo_move_to(cr, bx, cy + 8 * u);
        cairo_curve_to(cr, bx, cy + 20 * u, cx, cy + 16 * u, cx, cy + 26 * u);
        cairo_stroke(cr);
    }
    cairo_set_line_width(cr, 4 * u);
    cairo_move_to(cr, cx, cy + 26 * u);
    cairo_line_to(cr, cx, cy + 34 * u);
    cairo_stroke(cr);
}

/**
 * スピーカーの絵（箱・コーンと、音の波 2 本）。
 * @param cr cairo
 * @param cx 真ん中の x
 * @param cy 真ん中の y
 * @param s 大きさ（px）
 * @param c 色
 */
void drawSpeakerIcon(cairo_t* cr, double cx, double cy, double s, Color c) {
    cairo_set_source_rgb(cr, c.r, c.g, c.b);
    const double u = s / 72.0;
    const double left = cx - 30 * u;
    cairo_move_to(cr, left, cy - 11 * u);
    cairo_line_to(cr, left + 14 * u, cy - 11 * u);
    cairo_line_to(cr, left + 32 * u, cy - 27 * u);
    cairo_line_to(cr, left + 32 * u, cy + 27 * u);
    cairo_line_to(cr, left + 14 * u, cy + 11 * u);
    cairo_line_to(cr, left, cy + 11 * u);
    cairo_close_path(cr);
    cairo_fill(cr);
    cairo_set_line_width(cr, 5 * u);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    for (int i = 1; i <= 2; ++i) {
        cairo_new_sub_path(cr);
        cairo_arc(cr, left + 34 * u, cy, (10 + i * 11) * u, -M_PI / 4, M_PI / 4);
        cairo_stroke(cr);
    }
}

/**
 * ▶（再生）を描く。
 * @param cr cairo
 * @param cx 真ん中の x
 * @param cy 真ん中の y
 * @param s 大きさ（px）
 * @param c 色
 */
void drawPlayIcon(cairo_t* cr, double cx, double cy, double s, Color c) {
    cairo_set_source_rgb(cr, c.r, c.g, c.b);
    cairo_move_to(cr, cx - s * 0.32, cy - s * 0.42);
    cairo_line_to(cr, cx + s * 0.44, cy);
    cairo_line_to(cr, cx - s * 0.32, cy + s * 0.42);
    cairo_close_path(cr);
    cairo_fill(cr);
}

/**
 * ■（停止）を描く。
 * @param pen 描画の道具
 * @param cx 真ん中の x
 * @param cy 真ん中の y
 * @param s 大きさ（px）
 * @param c 色
 */
void drawStopIcon(const Pen& pen, double cx, double cy, double s, Color c) {
    pen.color(c);
    pen.roundedRect(cx - s / 2, cy - s / 2, s, s, s * 0.15);
    cairo_fill(pen.cr);
}

/**
 * ●（録音）を描く。
 * @param cr cairo
 * @param cx 真ん中の x
 * @param cy 真ん中の y
 * @param r 半径
 * @param c 色
 */
void drawDot(cairo_t* cr, double cx, double cy, double r, Color c) {
    cairo_set_source_rgb(cr, c.r, c.g, c.b);
    cairo_new_sub_path(cr);
    cairo_arc(cr, cx, cy, r, 0, 2 * M_PI);
    cairo_fill(cr);
}

/**
 * 枠の線を描く（角丸）。
 * @param pen 描画の道具
 * @param x 左
 * @param y 上
 * @param w 幅
 * @param h 高さ
 * @param r 角の半径
 * @param c 色
 * @param width 線の太さ
 * @param dashed 点線にするか
 */
void strokeRounded(const Pen& pen, double x, double y, double w, double h, double r, Color c, double width,
                   bool dashed = false) {
    pen.color(c);
    cairo_set_line_width(pen.cr, width);
    if (dashed) {
        const double dash[] = {7, 5};
        cairo_set_dash(pen.cr, dash, 2, 0);
    }
    pen.roundedRect(x + width / 2, y + width / 2, w - width, h - width, std::max(0.0, r - width / 2));
    cairo_stroke(pen.cr);
    cairo_set_dash(pen.cr, nullptr, 0, 0);
}

/**
 * 斜線の入ったマイクの絵（ミュート中の印。色だけで伝えないため）。
 * @param cr cairo
 * @param cx 真ん中の x
 * @param cy 真ん中の y
 * @param s 大きさ（px、絵の高さのめやす）
 * @param c 色
 * @param bg 下の色（斜線とマイクの間に隙間を空けるのに使う）
 */
void drawMicMutedIcon(cairo_t* cr, double cx, double cy, double s, Color c, Color bg) {
    const double u = s / 24.0;
    cairo_save(cr);
    cairo_set_source_rgb(cr, c.r, c.g, c.b);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    // 頭（縦長のカプセル）
    const double headW = 7 * u;
    const double headTop = cy - 10 * u;
    const double headH = 12 * u;
    cairo_new_sub_path(cr);
    cairo_arc(cr, cx, headTop + headW / 2, headW / 2, M_PI, 0);
    cairo_arc(cr, cx, headTop + headH - headW / 2, headW / 2, 0, M_PI);
    cairo_close_path(cr);
    cairo_fill(cr);
    // 受け（U 字）・柄・台
    cairo_set_line_width(cr, 2 * u);
    cairo_new_sub_path(cr);
    cairo_arc(cr, cx, cy - 1 * u, 6.5 * u, 0, M_PI);
    cairo_stroke(cr);
    cairo_move_to(cr, cx, cy + 5.5 * u);
    cairo_line_to(cr, cx, cy + 9 * u);
    cairo_move_to(cr, cx - 4 * u, cy + 10 * u);
    cairo_line_to(cr, cx + 4 * u, cy + 10 * u);
    cairo_stroke(cr);
    // 斜線（下の色で太めに抜いてから、色で引く）
    cairo_set_source_rgb(cr, bg.r, bg.g, bg.b);
    cairo_set_line_width(cr, 5 * u);
    cairo_move_to(cr, cx - 9 * u, cy - 10 * u);
    cairo_line_to(cr, cx + 9 * u, cy + 10 * u);
    cairo_stroke(cr);
    cairo_set_source_rgb(cr, c.r, c.g, c.b);
    cairo_set_line_width(cr, 2.2 * u);
    cairo_move_to(cr, cx - 9 * u, cy - 10 * u);
    cairo_line_to(cr, cx + 9 * u, cy + 10 * u);
    cairo_stroke(cr);
    cairo_restore(cr);
}

/**
 * ピル型のボタンを描く（更新の帯・ミュートの帯のボタン。当たり判定の登録は呼び出し側）。
 * primary はアクセントの塗り、ほかは下の行のボタンと同じ地・枠。
 * @param pen 描画の道具
 * @param x 左
 * @param y 上
 * @param w 幅
 * @param h 高さ
 * @param label 文言
 * @param size 文字の大きさ
 * @param primary アクセントの塗りにするか
 * @param pointer 0 = ふつう、1 = 乗っている、2 = 押している
 */
void drawPillButton(const Pen& pen, double x, double y, double w, double h, const std::string& label, double size,
                    bool primary, int pointer) {
    cairo_t* cr = pen.cr;
    if (primary) {
        pen.color(pointer == 2 ? kAccentPressed : kAccent);
        pen.roundedRect(x, y, w, h, h / 2);
        cairo_fill(cr);
    } else {
        pen.color(pointer > 0 ? kControlHover : kControl);
        pen.roundedRect(x, y, w, h, h / 2);
        cairo_fill(cr);
        strokeRounded(pen, x, y, w, h, h / 2, kBorder, 2);
    }
    textCentered(pen, x + w / 2, centerBaseline(y, h, size), label, size, primary ? kOnAccent : kText, true);
}

/**
 * 録った時刻を「00:41」の形にする。
 * @param t 時刻
 * @return 文字列
 */
std::string clockText(std::time_t t) {
    std::tm tm {};
    localtime_r(&t, &tm);
    char text[16];
    std::strftime(text, sizeof(text), "%H:%M", &tm);
    return text;
}

}  // namespace

std::string errorText(MicError error, const UiText& text) {
    switch (error) {
        case MicError::None: break;
        case MicError::ReadSettings: return text.errReadSettings;
        case MicError::NotInstalled: return text.errNotInstalled;
        case MicError::ReadLinks: return text.errReadLinks;
        case MicError::WriteSettings: return text.errWriteSettings;
        case MicError::WriteAutostart: return text.errWriteAutostart;
        case MicError::WriteNsParams: return text.errWriteNsParams;
        case MicError::WriteEcho: return text.errWriteEcho;
        case MicError::WriteNs: return text.errWriteNs;
        case MicError::WriteMute: return text.errWriteMute;
    }
    return "";
}

std::string updateErrorText(const std::string& error, const UiText& t) {
    if (error == "network") return t.updateErrNetwork;
    if (error == "rate-limited") return t.updateErrRateLimited;
    if (error == "not-found") return t.updateErrNotFound;
    if (error == "bad-response") return t.updateErrBadResponse;
    if (error == "bad-version") return t.updateErrBadVersion;
    if (error == "bad-url") return t.updateErrBadUrl;
    if (error == "missing-tool") return t.updateErrMissingTool;
    if (error == "no-checksums") return t.updateErrNoChecksums;
    if (error == "no-asset") return t.updateErrNoAsset;
    if (error == "checksum-mismatch") return t.updateErrChecksumMismatch;
    if (error == "unsafe-archive") return t.updateErrUnsafeArchive;
    if (error == "no-installer") return t.updateErrNoInstaller;
    if (error == "install-failed") return t.updateErrInstallFailed;
    if (error == "bad-args") return t.updateErrBadArgs;
    if (error == "busy") return t.updateErrBusy;
    if (error == "not-newer") return t.updateErrNotNewer;
    if (error == "detach-failed") return t.updateErrDetachFailed;
    if (error == "interrupted") return t.updateErrInterrupted;
    if (error == "io") return t.updateErrIo;
    return t.updateErrOther;  // 知らない理由・usage・script-failed・spawn-failed
}

MicPanel::MicPanel(const FontSet& fonts) : fonts_(fonts) {
    surface_ = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, kWidth, kHeight);
    cr_ = cairo_create(surface_);
    cairo_font_options_t* options = cairo_font_options_create();
    cairo_font_options_set_antialias(options, CAIRO_ANTIALIAS_GRAY);
    cairo_font_options_set_hint_style(options, CAIRO_HINT_STYLE_SLIGHT);
    cairo_set_font_options(cr_, options);
    cairo_font_options_destroy(options);
}

MicPanel::~MicPanel() {
    cairo_destroy(cr_);
    cairo_surface_destroy(surface_);
}

void MicPanel::addButton(PanelAction action, int index, double x, double y, double w, double h, bool usable) {
    buttons_.push_back({{action, index}, x, y, w, h, usable});
}

int MicPanel::pointerState(PanelAction action, int index) const {
    const PanelHit hit {action, index};
    if (pressed_ == hit) return 2;
    if (hover_ == hit) return 1;
    return 0;
}

PanelHit MicPanel::hitTest(double x, double y) const {
    for (const auto& b : buttons_) {
        if (!b.usable) continue;
        if (x >= b.x && x <= b.x + b.w && y >= b.y && y <= b.y + b.h) return b.hit;
    }
    return {};
}

double MicPanel::trackValue(const Track& track, double x) {
    const double fraction = std::clamp((x - track.x0) / std::max(1.0, track.x1 - track.x0), 0.0, 1.0);
    const double value = track.min + (track.max - track.min) * fraction;
    return track.action == PanelAction::NsVadSlider ? clampNsVad(value) : clampNsGrace(value);
}

bool MicPanel::pointerMove(double x, double y) {
    if (dragAction_ != PanelAction::None) {
        // ドラッグ中は、ポインターが溝の上下に外れても横の位置で値を決める
        for (const Track& track : tracks_) {
            if (track.action != dragAction_) continue;
            const double value = trackValue(track, x);
            if (value == dragValue_) return false;
            dragValue_ = value;
            return true;
        }
        return false;
    }
    const PanelHit now = hitTest(x, y);
    if (now == hover_) return false;
    hover_ = now;
    return true;
}

PanelHit MicPanel::pointerDown(double x, double y, double now) {
    hover_ = hitTest(x, y);
    pressed_ = hover_;
    if (pressed_.action == PanelAction::NsVadSlider || pressed_.action == PanelAction::NsGraceSlider) {
        // バーは押したところへ飛び、そのままドラッグ
        for (const Track& track : tracks_) {
            if (track.action != pressed_.action) continue;
            dragAction_ = track.action;
            dragValue_ = trackValue(track, x);
        }
    }
    if (pressed_.action == PanelAction::Quit) {
        // 誤って押しても終わらないよう、1 回目は確認の表示にするだけ
        if (quitArmed_ && now <= quitArmedUntil_) return pressed_;
        quitArmed_ = true;
        quitArmedUntil_ = now + kQuitConfirmSec;
        return {};
    }
    if (pressed_.action == PanelAction::UpdateInstall) {
        // 終了と同じく、1 回目は確認の表示にするだけ（帯が updateConfirmFormat と「やめる」「更新する」に変わる）
        if (updateArmed_ && now <= updateArmedUntil_) return pressed_;
        updateArmed_ = true;
        updateArmedUntil_ = now + kQuitConfirmSec;
        return {};
    }
    quitArmed_ = false;    // 別のボタン（更新の「やめる」も）を押したら確認は取り消す
    updateArmed_ = false;
    return pressed_;
}

bool MicPanel::pointerUp() {
    dragAction_ = PanelAction::None;
    if (pressed_.action == PanelAction::None) return false;
    pressed_ = {};
    return true;
}

bool MicPanel::pointerLeave() {
    const bool changed = hover_.action != PanelAction::None || pressed_.action != PanelAction::None;
    hover_ = {};
    pressed_ = {};
    dragAction_ = PanelAction::None;
    return changed;
}

bool MicPanel::tick(double now) {
    const bool holdExpired = holdUntil_ >= 0 && now_ <= holdUntil_ && now > holdUntil_;
    now_ = now;
    bool changed = holdExpired;  // 書いた値の表示をやめて、読んだ値に戻す
    if (quitArmed_ && now > quitArmedUntil_) {
        quitArmed_ = false;
        changed = true;
    }
    if (updateArmed_ && now > updateArmedUntil_) {
        updateArmed_ = false;
        changed = true;
    }
    return changed;
}

void MicPanel::holdPreset(PanelAction action, double until) {
    presetHold_ = action;
    presetHoldUntil_ = until;
}

void MicPanel::holdNsValues(double vad, double grace, double until) {
    holdVad_ = clampNsVad(vad);
    holdGrace_ = clampNsGrace(grace);
    holdUntil_ = until;
}

void MicPanel::displayedNsValues(const MicState& state, double& vad, double& grace) const {
    const NsParams& params = state.nsParams;
    const bool held = now_ <= holdUntil_;
    vad = held ? holdVad_ : (params.vadKnown ? params.vad : kNsVadDefault);
    grace = held ? holdGrace_ : (params.graceKnown ? params.grace : kNsGraceDefault);
    if (dragAction_ == PanelAction::NsVadSlider) vad = dragValue_;
    if (dragAction_ == PanelAction::NsGraceSlider) grace = dragValue_;
}

void MicPanel::setDragForPreview(PanelAction action, double value) {
    dragAction_ = action;
    dragValue_ = action == PanelAction::NsVadSlider ? clampNsVad(value) : clampNsGrace(value);
    pressed_ = {action, 0};
    hover_ = pressed_;
}

void MicPanel::armQuitForPreview() {
    quitArmed_ = true;
    quitArmedUntil_ = 1e300;
}

void MicPanel::armUpdateForPreview() {
    updateArmed_ = true;
    updateArmedUntil_ = 1e300;
}

void MicPanel::setPointerForPreview(PanelHit hover, PanelHit pressed) {
    hover_ = hover;
    pressed_ = pressed;
}

void MicPanel::drawSegmented(const Pen& pen, double x, double y, double w, double h, const std::string* labels,
                             const PanelAction* actions, int count, int selected, double size, bool usable) {
    if (count <= 0) return;
    cairo_t* cr = pen.cr;
    const double r = h / 2;
    // 地のピル。押せるときは枠（3:1 以上）で部品の形を見せる。押せないときは枠なし
    pen.color(kControl);
    pen.roundedRect(x, y, w, h, r);
    cairo_fill(cr);
    if (usable) strokeRounded(pen, x, y, w, h, r, kBorder, 2);

    const double inset = 5;
    const double segment = (w - inset * 2) / count;
    for (int i = 0; i < count; ++i) {
        const double sx = x + inset + segment * i;
        const double sy = y + inset;
        const double sh = h - inset * 2;
        const int pointer = usable ? pointerState(actions[i]) : 0;
        const bool isSelected = usable && selected == i;
        if (isSelected) {
            // 選択中の側: アクセントの塗り（押している間は少し濃く）＋ ✓ ＋ 太字
            pen.color(pointer == 2 ? kAccentPressed : kAccent);
            pen.roundedRect(sx, sy, segment, sh, sh / 2);
            cairo_fill(cr);
        } else if (pointer > 0) {
            pen.color(kControlHover);
            pen.roundedRect(sx, sy, segment, sh, sh / 2);
            cairo_fill(cr);
        }
        const Color textColor = !usable ? kTextDisabled : (isSelected ? kOnAccent : kText);
        const double checkW = isSelected ? size * 0.9 : 0;
        const double labelSize = fitSize(pen, labels[i], size, size * 0.7, segment - 24 - checkW, isSelected);
        const double textW = pen.measure(labels[i], labelSize, isSelected) + checkW;
        const double tx = sx + (segment - textW) / 2;
        if (isSelected) drawCheck(cr, tx + checkW * 0.4, sy + sh / 2, size * 0.72, kOnAccent);
        pen.text(tx + checkW, centerBaseline(sy, sh, labelSize), labels[i], labelSize, textColor, isSelected);
        addButton(actions[i], 0, sx, y, segment, h, usable);
    }
}

void MicPanel::drawHeader(const Pen& pen, const UiText& t, const MicState& state) {
    pen.text(kLeftX, 54, t.title, 34, kText, true);
    // 左のカラムの右上のバッジ: 使用中は緑の塗り＋●＋文字、未使用は灰色の塗り＋○＋文字（色だけで伝えない）
    const double h = 40;
    const double y = 22;
    if (state.muteKnown && state.muted) {
        // ミュート中は使用中 / 未使用より優先して、赤い地と枠＋斜線の入ったマイクの絵＋「ミュート中」
        const double size = 20;
        const double w = pen.measure(t.micMuted, size, true) + 62;
        const double x = kLeftRight - w;
        pen.color(kMuteFill);
        pen.roundedRect(x, y, w, h, h / 2);
        cairo_fill(pen.cr);
        strokeRounded(pen, x, y, w, h, h / 2, kMuteBorder, 1.5);
        drawMicMutedIcon(pen.cr, x + 25, y + h / 2, 22, kMuteText, kMuteFill);
        pen.text(x + 44, centerBaseline(y, h, size), t.micMuted, size, kMuteText, true);
        return;
    }
    if (!state.loaded || !state.linksKnown) {
        pen.text(kLeftRight, centerBaseline(y, h, 18), state.loaded ? t.chainUnknown : t.loading, 18, kTextMuted,
                 false, true);
        return;
    }
    const std::string label = state.inUse ? t.micInUse : t.micIdle;
    const double size = 20;
    const double w = pen.measure(label, size, true) + 58;
    const double x = kLeftRight - w;
    pen.color(state.inUse ? kSuccessTint : kControl);
    pen.roundedRect(x, y, w, h, h / 2);
    cairo_fill(pen.cr);
    const Color c = state.inUse ? kSuccess : kTextMuted;
    if (state.inUse) {
        drawDot(pen.cr, x + 24, y + h / 2, 6.5, c);
    } else {
        pen.color(c);
        cairo_set_line_width(pen.cr, 2.5);
        cairo_new_sub_path(pen.cr);
        cairo_arc(pen.cr, x + 24, y + h / 2, 5.5, 0, 2 * M_PI);
        cairo_stroke(pen.cr);
    }
    pen.text(x + 40, centerBaseline(y, h, size), label, size, c, true);
}

void MicPanel::drawModeCards(const Pen& pen, const UiText& t, const MicState& state, double y) {
    // プリセットの中身（ユーザーと決めたもの）: イヤホン = エコー除去オフ・ノイズ除去オフ、
    // スピーカー = エコー除去オン・ノイズ除去オフ。一致はこの 2 つだけで見る（バーの値はノイズ除去がオフなら効かない）
    const bool known = state.echoKnown && state.nsKnown;
    bool matches[2] = {known && !state.echo && !state.ns, known && state.echo && !state.ns};
    // プリセットを書き込んでいる間は、押したカードを選択中の見た目で保つ（途中の値でグレーにしない）
    const bool holding = presetHold_ != PanelAction::None && now_ <= presetHoldUntil_;
    if (holding) {
        matches[0] = presetHold_ == PanelAction::Earphone;
        matches[1] = presetHold_ == PanelAction::Speaker;
    }
    // どちらとも一致しない（細かく調整した設定）ときは、両方のカードをグレーにする（押せるまま）。読み込み中はグレーにしない
    const bool custom = known && !matches[0] && !matches[1];

    // 見出し: 2 枚のカードは、設定をまとめて切り替えるプリセットだと分かるように
    pen.text(kLeftX, kPresetHeadY, t.presetTitle, fitSize(pen, t.presetTitle, 19, 14, kLeftRight - kLeftX, true), kText,
             true);

    // カードは全幅で縦に 2 枚（タブにしたぶん、名前とチップを大きくできる）
    const double w = kLeftRight - kLeftX;
    for (int i = 0; i < 2; ++i) {
        const PanelAction action = i == 0 ? PanelAction::Earphone : PanelAction::Speaker;
        const double x = kLeftX;
        const double cardY = y + i * (kModeH + kModeGap);
        const bool selected = matches[i];
        const int pointer = pointerState(action);
        const double r = 24;
        if (selected) {
            drawGlow(pen, x, cardY, w, kModeH, r, 8);
            drawCard(pen, x, cardY, w, kModeH, r, pointer == 2 ? kAccentPressed : kAccent, kAccent, 0);
        } else if (custom) {
            // グレー: 地の色の塗りに点線の枠。押している間は枠をアクセントに（押すとこのプリセットに戻る）
            drawCard(pen, x, cardY, w, kModeH, r, pointer > 0 ? kControl : kBg, kBorder, 0);
            if (pointer == 2) {
                strokeRounded(pen, x, cardY, w, kModeH, r, kAccent, 3);
            } else {
                strokeRounded(pen, x, cardY, w, kModeH, r, kBorder, 2, true);
            }
        } else {
            // 選ばれていないカードは枠（3:1 以上）で押せる形を見せる。乗っている間は少し明るく、押している間は枠をアクセントに
            drawCard(pen, x, cardY, w, kModeH, r, pointer > 0 ? kControl : kCard, pointer == 2 ? kAccent : kBorder,
                     pointer == 2 ? 3 : 2);
        }
        // グレーのときも、文字は読める明るさ（地の上で 4:1 以上）にする
        const Color fg = selected ? kOnAccent : (custom ? kTextDisabled : kText);
        const Color sub = selected ? kOnAccent : (custom ? kTextDisabled : kTextMuted);
        const double innerRight = x + w - 18;

        // 左に絵
        if (i == 0) {
            drawEarphoneIcon(pen.cr, x + 50, cardY + kModeH / 2 - 2, 56, fg);
        } else {
            drawSpeakerIcon(pen.cr, x + 52, cardY + kModeH / 2, 56, fg);
        }

        // 右に「押すと何になるか」のチップ 2 つ（縦に並べる。14px）
        const std::string chips[2] = {i == 0 ? t.chipEchoOff : t.chipEchoOn, t.chipNsOff};
        const double chipH = 30;
        const double chipSize = 14;
        double chipW = 0;
        for (const std::string& chip : chips) chipW = std::max(chipW, pen.measure(chip, chipSize, true) + 24);
        const double chipX = innerRight - chipW;
        for (int c = 0; c < 2; ++c) {
            const double chipY = cardY + (kModeH - chipH * 2 - 8) / 2 + c * (chipH + 8);
            // 選択中（アクセントの塗り）の上では濃い地にアクセントの文字、ほかは地の色に枠と文字
            pen.color(kBg);
            pen.roundedRect(chipX, chipY, chipW, chipH, chipH / 2);
            cairo_fill(pen.cr);
            if (!selected) strokeRounded(pen, chipX, chipY, chipW, chipH, chipH / 2, kBorder, 1);
            const Color chipText = selected ? kAccent : (custom ? kTextDisabled : kText);
            pen.text(chipX + 12, centerBaseline(chipY, chipH, chipSize), chips[c], chipSize, chipText, true);
        }

        // 真ん中: 名前（28px 目安。選択中は前に ✓ の丸）と、何のためか
        double tx = x + 96;
        const double textRight = chipX - 14;
        if (selected) {
            drawDot(pen.cr, tx + 13, cardY + 42, 13, kOnAccent);
            drawCheck(pen.cr, tx + 13, cardY + 42, 17, kAccent);
            tx += 34;
        }
        const std::string title = i == 0 ? t.earphoneCard : t.speakerCard;
        pen.text(tx, cardY + 52, title, fitSize(pen, title, 28, 18, textRight - tx, true), fg, true);
        const std::string effect = i == 0 ? t.earphoneHint : t.speakerHint;
        pen.text(x + 96, cardY + 84, effect, fitSize(pen, effect, 17, 12, textRight - x - 96, false), sub, false);
        addButton(action, 0, x, cardY, w, kModeH);
    }

    // カードの下の 1 行: ふだんは説明、どちらとも一致しないときは「今は細かく調整した設定です」と、細かく調整のタブへ移るボタン
    const double rowH = 34;
    if (custom) {
        const std::string label = t.goFine;
        const double size = 15;
        const double buttonW = pen.measure(label, size, true) + 32;
        const double buttonX = kLeftRight - buttonW;
        const int pointer = pointerState(PanelAction::TabFine, 1);  // タブ（番号 0）とは別のボタン
        pen.color(pointer > 0 ? kControlHover : kControl);
        pen.roundedRect(buttonX, kCustomRowY, buttonW, rowH, rowH / 2);
        cairo_fill(pen.cr);
        strokeRounded(pen, buttonX, kCustomRowY, buttonW, rowH, rowH / 2, kBorder, 1.5);
        pen.text(buttonX + 16, centerBaseline(kCustomRowY, rowH, size), label, size, kText, true);
        addButton(PanelAction::TabFine, 1, buttonX, kCustomRowY, buttonW, rowH);
        pen.text(kLeftX, centerBaseline(kCustomRowY, rowH, 16), t.presetCustom,
                 fitSize(pen, t.presetCustom, 16, 12, buttonX - kLeftX - 12, true), kText, true);
    } else {
        pen.text(kLeftX, centerBaseline(kCustomRowY, rowH, 15), t.presetHint,
                 fitSize(pen, t.presetHint, 15, 12, kLeftRight - kLeftX, false), kTextMuted);
    }
}

void MicPanel::drawTabs(const Pen& pen, const UiText& t, PanelTab tab) {
    const std::string labels[2] = {t.tabQuick, t.fineTune};
    const PanelAction actions[2] = {PanelAction::TabQuick, PanelAction::TabFine};
    drawSegmented(pen, kLeftX, kTabY, kLeftRight - kLeftX, kTabH, labels, actions, 2,
                  tab == PanelTab::Quick ? 0 : 1, 20);
}

void MicPanel::drawToggleRows(const Pen& pen, const UiText& t, const MicState& state, double y) {
    for (int row = 0; row < 2; ++row) {
        const double top = y + row * kToggleStep;
        const bool isEcho = row == 0;
        const std::string title = isEcho ? t.rowEcho : t.rowNs;
        pen.text(kLeftX, centerBaseline(top, kSegmentH, 22), title,
                 fitSize(pen, title, 22, 16, kSegmentX - kLeftX - 10, true), kText, true);
        const std::string labels[2] = {t.on, t.off};
        const PanelAction actions[2] = {isEcho ? PanelAction::EchoOn : PanelAction::NsOn,
                                        isEcho ? PanelAction::EchoOff : PanelAction::NsOff};
        const bool known = isEcho ? state.echoKnown : state.nsKnown;
        const bool value = isEcho ? state.echo : state.ns;
        drawSegmented(pen, kSegmentX, top, kSegmentW, kSegmentH, labels, actions, 2,
                      known ? (value ? 0 : 1) : -1, 22);
        const std::string hint = isEcho ? t.echoHint : t.nsHint;
        const double hintX = kSegmentX + kSegmentW + 16;
        pen.text(hintX, centerBaseline(top, kSegmentH, 16), hint,
                 fitSize(pen, hint, 16, 12, kLeftRight - hintX, false), kTextMuted);
    }
}

void MicPanel::drawNsSliders(const Pen& pen, const UiText& t, const MicState& state, double y) {
    cairo_t* cr = pen.cr;
    const NsParams& params = state.nsParams;
    // 値が読めていれば操作できる。ノイズ除去がオフの間は効かないので、グレーにして説明を出す（押せるまま）
    const bool usable = params.nodeKnown && params.vadKnown && params.graceKnown;
    const bool active = usable && state.nsKnown && state.ns;
    double vad = 0.0;
    double grace = 0.0;
    displayedNsValues(state, vad, grace);

    const double labelW = 176;
    const double buttonW = 42;
    const double buttonH = 42;
    const double minusX = kLeftX + labelW + 8;
    const double trackX0 = minusX + buttonW + 14;
    const double trackX1 = trackX0 + 196;
    const double plusX = trackX1 + 14;
    const double valueRight = kLeftRight;
    const double resetW = 140;
    const double resetX = kLeftRight - resetW;

    /**
     * − / ＋ / 標準に戻すの小さなボタン（ピル）を描いて登録する。
     */
    const auto smallButton = [&](PanelAction action, double x, double top, double w, const std::string& label,
                                 double size) {
        const int pointer = usable ? pointerState(action) : 0;
        pen.color(pointer > 0 ? kControlHover : kControl);
        pen.roundedRect(x, top, w, buttonH, buttonH / 2);
        cairo_fill(cr);
        if (usable) strokeRounded(pen, x, top, w, buttonH, buttonH / 2, kBorder, 1.5);
        const double s = fitSize(pen, label, size, 11, w - 12, true);
        textCentered(pen, x + w / 2, centerBaseline(top, buttonH, s), label, s, usable ? kText : kTextDisabled, true);
        addButton(action, 0, x, top, w, buttonH, usable);
    };

    for (int row = 0; row < 2; ++row) {
        const bool isVad = row == 0;
        const double top = y + row * kSliderStep;
        const double cy = top + kSliderH / 2;
        const PanelAction sliderAction = isVad ? PanelAction::NsVadSlider : PanelAction::NsGraceSlider;
        const double min = isVad ? kNsVadMin : kNsGraceMin;
        const double max = isVad ? kNsVadMax : kNsGraceMax;
        const double value = isVad ? vad : grace;

        // 見出しと短い説明（オフの間は 1 本目の説明を「オフの間は効きません」に）
        const std::string title = isVad ? t.nsVad : t.nsGrace;
        pen.text(kLeftX, top + 20, title, fitSize(pen, title, 17, 13, labelW - 6, true), active ? kText : kTextMuted,
                 true);
        const std::string hint = (!active && isVad) ? t.nsInactive : (isVad ? t.nsVadHint : t.nsGraceHint);
        pen.text(kLeftX, top + 41, hint, fitSize(pen, hint, 14, 11, labelW - 6, false), kTextMuted);

        smallButton(isVad ? PanelAction::NsVadMinus : PanelAction::NsGraceMinus, minusX, cy - buttonH / 2, buttonW,
                    "−", 22);
        smallButton(isVad ? PanelAction::NsVadPlus : PanelAction::NsGracePlus, plusX, cy - buttonH / 2, buttonW,
                    "＋", 22);

        // 溝・現在値までの塗り・つまみ。オフの間と、値が読めない間はグレー
        const double grooveH = 8;
        pen.color(kControl);
        pen.roundedRect(trackX0, cy - grooveH / 2, trackX1 - trackX0, grooveH, grooveH / 2);
        cairo_fill(cr);
        strokeRounded(pen, trackX0, cy - grooveH / 2, trackX1 - trackX0, grooveH, grooveH / 2, kBorder, 1.5);
        const double fraction = usable ? std::clamp((value - min) / (max - min), 0.0, 1.0) : 0.0;
        const double thumbX = trackX0 + (trackX1 - trackX0) * fraction;
        const Color fill = active ? kAccent : kTextDisabled;
        if (usable && thumbX > trackX0 + 1) {
            pen.color(fill);
            pen.roundedRect(trackX0, cy - grooveH / 2, thumbX - trackX0, grooveH, grooveH / 2);
            cairo_fill(cr);
        }
        const bool dragged = dragAction_ == sliderAction;
        const int pointer = usable ? pointerState(sliderAction) : 0;
        if (usable) {
            const double r = dragged ? 12 : (pointer > 0 ? 11 : 10);
            if (dragged && active) {
                pen.color(kAccent, kAccentTintAlpha * 1.5);
                cairo_new_sub_path(cr);
                cairo_arc(cr, thumbX, cy, r + 7, 0, 2 * M_PI);
                cairo_fill(cr);
            }
            // つまみ: 白（オフの間はグレー）に、塗りと見分けるための濃い縁
            drawDot(cr, thumbX, cy, r + 2, kBg);
            drawDot(cr, thumbX, cy, r, active ? kText : kTextDisabled);
        }
        tracks_.push_back({sliderAction, trackX0, trackX1, min, max, cy});
        // 溝の当たり判定は、つまみの半径ぶん左右に広げ、行の高さいっぱいにする
        addButton(sliderAction, 0, trackX0 - 10, top, trackX1 - trackX0 + 20, kSliderH, usable);

        // 今の値
        char text[32];
        if (!usable) {
            std::snprintf(text, sizeof(text), "--");
        } else if (isVad) {
            std::snprintf(text, sizeof(text), "%.0f%%", value);
        } else {
            std::snprintf(text, sizeof(text), "%.0fms", value);
        }
        pen.text(valueRight, centerBaseline(top, kSliderH, 19), text, 19, active ? kText : kTextDisabled, true, true);
    }

    // 標準に戻す（SteamOS の既定 23% / 500ms）。バーの下の行に、標準の値の説明と一緒に置く
    const double resetTop = y + kSliderStep * 2;
    pen.text(kLeftX, centerBaseline(resetTop, buttonH, 14), t.nsDefaultNote,
             fitSize(pen, t.nsDefaultNote, 14, 11, resetX - kLeftX - 12, false), kTextMuted);
    smallButton(PanelAction::NsReset, resetX, resetTop, resetW, t.nsReset, 16);
}

void MicPanel::drawPipeline(const Pen& pen, const UiText& t, const MicState& state, double y) {
    cairo_t* cr = pen.cr;
    const double x = kLeftX;
    const double w = kLeftRight - kLeftX;
    drawCard(pen, x, y, w, kPipelineH, 20, kCard, kDivider, 1);
    pen.text(x + kCardPad, y + 30, t.rowChain, 16, kTextMuted, true);
    std::string note;
    if (!state.loaded) {
        note = t.loading;
    } else if (!state.linksKnown) {
        note = t.chainUnknown;
    } else if (!state.inUse) {
        note = t.chainIdle;
    }
    if (!note.empty()) {
        const double noteMax = w - kCardPad * 2 - pen.measure(t.rowChain, 16, true) - 16;
        pen.text(x + w - kCardPad, y + 30, note, fitSize(pen, note, 15, 12, noteMax, false), kTextMuted, false, true);
    }

    // 5 つの段: マイク ─ 音質補正 ─ エコー除去 ─ ノイズ除去 ─ アプリへ。通っている段だけ光らせる
    const std::string labels[5] = {t.stageMic, t.stageEq, t.stageEcho, t.stageNs, t.stageOut};
    bool active[5] = {state.linksKnown, false, false, false, state.linksKnown};
    for (const ChainStage& stage : state.chain) {
        if (stage.kind == ChainStage::Kind::Eq) active[1] = true;
        if (stage.kind == ChainStage::Kind::EchoCancel) active[2] = true;
        if (stage.kind == ChainStage::Kind::NoiseSuppression) active[3] = true;
    }
    const double nodeH = 42;
    const double nodePad = 11;  // 札の中の左右の余白
    const double cy = y + 66;
    const double inner = w - kCardPad * 2;
    double size = 19;
    double widths[5];
    double total = 0;
    for (; size >= 13; size -= 1) {
        total = 0;
        for (int i = 0; i < 5; ++i) {
            widths[i] = pen.measure(labels[i], size, true) + nodePad * 2;
            total += widths[i];
        }
        if (total + 4 * 16 <= inner) break;  // 札の間は 16px 以上空ける
    }
    const double gap = (inner - total) / 4;
    double left[5];
    double cursor = x + kCardPad;
    for (int i = 0; i < 5; ++i) {
        left[i] = cursor;
        cursor += widths[i] + gap;
    }

    // 下地: となり合う段を点線でつなぐ（通っていないつながり）
    const double dash[] = {5, 4};
    pen.color(kBorder);
    cairo_set_line_width(cr, 2);
    cairo_set_dash(cr, dash, 2, 0);
    for (int i = 0; i < 4; ++i) {
        cairo_move_to(cr, left[i] + widths[i], cy);
        cairo_line_to(cr, left[i + 1], cy);
    }
    cairo_stroke(cr);
    cairo_set_dash(cr, nullptr, 0, 0);

    // 実際の通り道（アクセント）。通っていない段は下を回って飛ばす
    if (state.linksKnown) {
        pen.color(kAccent);
        cairo_set_line_width(cr, 3.5);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        int previous = 0;
        for (int i = 1; i < 5; ++i) {
            if (!active[i]) continue;
            const double x0 = left[previous] + widths[previous];
            const double x1 = left[i];
            if (i == previous + 1) {
                cairo_move_to(cr, x0, cy);
                cairo_line_to(cr, x1, cy);
            } else {
                const double from = left[previous] + widths[previous] / 2;
                const double to = left[i] + widths[i] / 2;
                cairo_move_to(cr, from, cy + nodeH / 2);
                cairo_curve_to(cr, from, cy + 40, to, cy + 40, to, cy + nodeH / 2);
            }
            previous = i;
        }
        cairo_stroke(cr);
    }

    for (int i = 0; i < 5; ++i) {
        const double nx = left[i];
        const double ny = cy - nodeH / 2;
        if (active[i]) {
            // 通っている段: アクセントの薄い塗り＋実線の枠＋太字
            pen.color(kAccentTint);
            pen.roundedRect(nx, ny, widths[i], nodeH, nodeH / 2);
            cairo_fill(cr);
            strokeRounded(pen, nx, ny, widths[i], nodeH, nodeH / 2, kAccent, 2);
            pen.text(nx + nodePad, centerBaseline(ny, nodeH, size), labels[i], size, kText, true);
        } else {
            // 通っていない段: 塗りなし＋点線の枠＋細字
            pen.color(kCard);
            pen.roundedRect(nx, ny, widths[i], nodeH, nodeH / 2);
            cairo_fill(cr);
            strokeRounded(pen, nx, ny, widths[i], nodeH, nodeH / 2, kBorder, 2, true);
            pen.text(nx + nodePad + (pen.measure(labels[i], size, true) - pen.measure(labels[i], size, false)) / 2,
                     centerBaseline(ny, nodeH, size), labels[i], size, kTextMuted, false);
        }
    }
}

void MicPanel::drawVoice(const Pen& pen, const UiText& t, const VoiceView& voice, double y) {
    cairo_t* cr = pen.cr;
    const double x = kRightX;
    const double w = kRight - kRightX;
    drawCard(pen, x, y, w, kVoiceH, 20, kCard, kDivider, 1);
    const double inner = x + kCardPad;
    const double innerRight = x + w - kCardPad;
    pen.text(inner, y + 40, t.voiceTitle, 24, kText, true);
    const double buttonW = 150;
    const double buttonH = 52;
    const double buttonX = innerRight - buttonW;
    const double buttonY = y + 14;
    pen.text(inner, y + 64, t.voiceHint, fitSize(pen, t.voiceHint, 15, 12, buttonX - inner - 12, false), kTextMuted);

    // 録音のボタン: 待機中は「● 録音」、録音中は赤い塗りの「■ 停止」
    const int pointer = pointerState(PanelAction::Record);
    pen.color(voice.recording ? kDangerTint : (pointer > 0 ? kControlHover : kControl));
    pen.roundedRect(buttonX, buttonY, buttonW, buttonH, buttonH / 2);
    cairo_fill(cr);
    strokeRounded(pen, buttonX, buttonY, buttonW, buttonH, buttonH / 2, kDanger, 2);
    {
        const std::string label = voice.recording ? t.stop : t.record;
        const double size = fitSize(pen, label, 22, 15, buttonW - 60, true);
        const double iconW = 28;
        const double contentW = iconW + pen.measure(label, size, true);
        const double cx = buttonX + (buttonW - contentW) / 2;
        if (voice.recording) {
            drawStopIcon(pen, cx + 9, buttonY + buttonH / 2, 16, kText);
        } else {
            drawDot(cr, cx + 9, buttonY + buttonH / 2, 9, kDanger);
        }
        pen.text(cx + iconW, centerBaseline(buttonY, buttonH, size), label, size, kText, true);
    }
    addButton(PanelAction::Record, 0, buttonX, buttonY, buttonW, buttonH);

    // メーターの行: 録音中は「● 録音中」・経過と残り・ピークのメーター（dBFS）
    const double meterTop = y + 78;
    const double meterH = 40;
    if (voice.recording) {
        drawDot(cr, inner + 7, meterTop + meterH / 2, 7, kDanger);
        const double labelEnd = inner + 20 + pen.text(inner + 20, centerBaseline(meterTop, meterH, 17), t.recording,
                                                      17, kDanger, true);
        char elapsed[64];
        std::snprintf(elapsed, sizeof(elapsed), t.elapsedFormat, voice.recordSec,
                      std::max(0.0, kVoiceMaxSec - voice.recordSec));
        const double elapsedEnd =
            labelEnd + 12 + pen.text(labelEnd + 12, centerBaseline(meterTop, meterH, 16), elapsed, 16, kText, false);
        char level[32];
        std::snprintf(level, sizeof(level), "%.1f dBFS", std::max(-60.0f, voice.levelDb));
        const double levelW = pen.measure("-60.0 dBFS", 16, false);
        pen.text(innerRight, centerBaseline(meterTop, meterH, 16), level, 16, kText, false, true);
        const double barX = elapsedEnd + 14;
        const double barW = innerRight - levelW - 12 - barX;
        const double barH = 16;
        const double barY = meterTop + (meterH - barH) / 2;
        if (barW > 30) {
            pen.color(kBg);
            pen.roundedRect(barX, barY, barW, barH, barH / 2);
            cairo_fill(cr);
            const double fraction = std::clamp((voice.levelDb + 60.0) / 60.0, 0.0, 1.0);
            if (fraction > 0) {
                pen.color(kAccent);
                pen.roundedRect(barX, barY, std::max(barH, barW * fraction), barH, barH / 2);
                cairo_fill(cr);
            }
            strokeRounded(pen, barX, barY, barW, barH, barH / 2, kBorder, 1.5);
        }
    } else if (voice.error != VoiceError::None) {
        const std::string message = voice.error == VoiceError::Record ? t.errRecord : t.errPlay;
        pen.text(inner, centerBaseline(meterTop, meterH, 17), message,
                 fitSize(pen, message, 17, 12, innerRight - inner, true), kDanger, true);
    } else {
        pen.text(inner, centerBaseline(meterTop, meterH, 16), t.voiceIdle,
                 fitSize(pen, t.voiceIdle, 16, 12, innerRight - inner, false), kTextMuted);
    }

    // 履歴（新しい順に 5 件まで）
    pen.color(kDivider);
    cairo_set_line_width(cr, 1);
    cairo_move_to(cr, inner, y + 128.5);
    cairo_line_to(cr, innerRight, y + 128.5);
    cairo_stroke(cr);
    const double listTop = y + 138;
    if (voice.clips.empty()) {
        pen.text(inner, listTop + 32, t.noClips, 16, kTextMuted);
    }
    for (size_t i = 0; i < voice.clips.size() && i < kVoiceHistory; ++i) {
        const VoiceClip& clip = *voice.clips[i];
        const double rowTop = listTop + i * kRowStep;
        const double rowH = 56;
        const double cy = rowTop + rowH / 2;
        const bool playing = voice.playing && voice.playingId == clip.id;
        const int index = static_cast<int>(i);
        if (playing) {
            // 再生中の行: 左にアクセントの縦線
            pen.color(kAccent);
            pen.roundedRect(x + 6, rowTop + 8, 4, rowH - 16, 2);
            cairo_fill(cr);
        }
        // ▶ / ■ のボタン（丸）
        const double d = 48;
        const double bx = inner;
        const double by = cy - d / 2;
        const int state = pointerState(PanelAction::Play, index);
        if (playing) {
            pen.color(state == 2 ? kAccentPressed : kAccent);
            cairo_new_sub_path(cr);
            cairo_arc(cr, bx + d / 2, cy, d / 2, 0, 2 * M_PI);
            cairo_fill(cr);
            drawStopIcon(pen, bx + d / 2, cy, 15, kOnAccent);
        } else {
            pen.color(state > 0 ? kControlHover : kControl);
            cairo_new_sub_path(cr);
            cairo_arc(cr, bx + d / 2, cy, d / 2, 0, 2 * M_PI);
            cairo_fill(cr);
            pen.color(kBorder);
            cairo_set_line_width(cr, 2);
            cairo_new_sub_path(cr);
            cairo_arc(cr, bx + d / 2, cy, d / 2 - 1, 0, 2 * M_PI);
            cairo_stroke(cr);
            drawPlayIcon(cr, bx + d / 2 + 2, cy, 18, kText);
        }
        addButton(PanelAction::Play, index, bx, by, d, d);

        // 時刻・長さ（1 行目）と、録ったときの設定（2 行目）
        const double tx = bx + d + 14;
        const double waveX = inner + 256;
        const double timeEnd = tx + pen.text(tx, rowTop + 24, clockText(clip.recordedAt), 19, kText, true);
        char length[32];
        std::snprintf(length, sizeof(length), "%.1f %s", clip.seconds(), t.seconds);
        const double lengthEnd = timeEnd + 10 + pen.text(timeEnd + 10, rowTop + 24, length, 15, kTextMuted);
        const std::string setting = clipSettingLabel(clip, t.earphone, t.speaker, t.withNs, t.unknownSetting);
        const double settingMax = waveX - tx - 10;
        // ノイズ除去がオンで録ったものは、そのときの強さも短く（例: 23%/500ms）。
        // 2 行目に収まればその後ろに、収まらなければ 1 行目の長さの後ろに、そこにも収まらなければ省く
        std::string numbers;
        if (clip.nsKnown && clip.ns && clip.nsParamsKnown) {
            char text[32];
            std::snprintf(text, sizeof(text), "%.0f%%/%.0fms", clip.nsVad, clip.nsGrace);
            numbers = text;
        }
        const std::string withNumbers = setting + " " + numbers;
        if (!numbers.empty() && pen.measure(withNumbers, 13) <= settingMax) {
            pen.text(tx, rowTop + 46, withNumbers, fitSize(pen, withNumbers, 15, 13, settingMax, false), kText);
        } else {
            pen.text(tx, rowTop + 46, setting, fitSize(pen, setting, 15, 11, settingMax, false), kText);
            if (!numbers.empty() && lengthEnd + 8 + pen.measure(numbers, 13) <= waveX - 8) {
                pen.text(lengthEnd + 8, rowTop + 24, numbers, 13, kTextMuted);
            }
        }

        // 波形（横に長く）。細くなりすぎないよう、3px ごとの棒にまとめる。再生中は再生済みの部分と位置の線をアクセントに
        const double waveW = innerRight - waveX;
        const double waveH = 38;
        const int bars = std::max(1, static_cast<int>(waveW / 3));
        const double barW = waveW / bars;
        const double playedX = playing ? waveX + waveW * std::clamp(voice.playSec / std::max(0.01, clip.seconds()), 0.0, 1.0)
                                        : waveX;
        const int bins = static_cast<int>(clip.wave.size());
        for (int b = 0; b < bars && bins > 0; ++b) {
            float peak = 0.0f;
            for (int k = b * bins / bars; k < std::max(b * bins / bars + 1, (b + 1) * bins / bars) && k < bins; ++k) {
                peak = std::max(peak, clip.wave[k]);
            }
            const double bx0 = waveX + b * barW;
            const double amplitude = std::sqrt(std::clamp(static_cast<double>(peak), 0.0, 1.0));  // 小さい音も見えるように
            const double bh = std::max(2.0, amplitude * waveH);
            pen.color(playing && bx0 < playedX ? kAccent : kTextMuted);
            cairo_rectangle(cr, bx0, cy - bh / 2, std::max(1.0, barW - 1), bh);
            cairo_fill(cr);
        }
        if (playing) {
            pen.color(kAccent);
            cairo_rectangle(cr, playedX - 1, cy - waveH / 2 - 4, 3, waveH + 8);
            cairo_fill(cr);
        }
    }
}

void MicPanel::drawUpdateRow(const Pen& pen, const UiText& t, const frame_updater::UpdateStatus& update, double y) {
    using frame_updater::UpdateState;
    const double x = kPad;
    const double w = kRight - kPad;
    const double h = kUpdateRowH;
    const double buttonH = 50;  // 下の行のボタンと同じ高さ
    const double buttonY = y + (h - buttonH) / 2;
    const double buttonSize = 19;
    const double textX = x + kCardPad;

    // 帯の文: checking 中はほかの状態より優先して「確かめています…」を出す（前の答えは裏でそのまま残る）。
    // 1 行目（message）と、あれば 2 行目の補足（hint）。枠は普段はほかのカードと同じ飾りの線、
    // 新しい版・確認・入れ終わりはアクセント、更新の失敗は赤で囲む（色だけでなく文でも伝える）
    const bool confirming = !update.checking && update.state == UpdateState::Available && update.installable &&
                            updateArmed_;
    std::string message;
    std::string hint;
    Color color = kText;
    bool bold = false;
    Color border = kDivider;
    double borderWidth = 1;
    char buf[256];
    if (update.checking) {
        message = t.updateChecking;
    } else {
        switch (update.state) {
            case UpdateState::Unknown:
                message = "v" + update.current;
                color = kTextMuted;
                break;
            case UpdateState::UpToDate:
                std::snprintf(buf, sizeof(buf), t.updateUpToDateFormat, update.current.c_str());
                message = buf;
                break;
            case UpdateState::Available:
                std::snprintf(buf, sizeof(buf), confirming ? t.updateConfirmFormat : t.updateAvailableFormat,
                              update.latest.c_str());
                message = buf;
                color = confirming ? kText : kAccent;
                bold = true;
                border = kAccent;
                borderWidth = 2;
                if (confirming) hint = t.updateConfirmHint;
                if (!update.installable) {
                    // 手で更新: 2 行目に理由と、入るならリリースページの URL
                    hint = t.updateManual;
                    const std::string withUrl = hint + "  " + t.updateReleasePage + update.url;
                    if (!update.url.empty() && pen.measure(withUrl, 13) <= w - kCardPad * 2) hint = withUrl;
                }
                break;
            case UpdateState::Installing: {
                const char* step = t.updateStepStart;
                if (update.step == "download") step = t.updateStepDownload;
                else if (update.step == "verify") step = t.updateStepVerify;
                else if (update.step == "extract") step = t.updateStepExtract;
                else if (update.step == "install") step = t.updateStepInstall;
                std::snprintf(buf, sizeof(buf), t.updateInstallingFormat, step);
                message = buf;
                hint = t.updateConfirmHint;  // 確認のときと同じ補足（途中で閉じて開き直すことがある）
                break;
            }
            case UpdateState::Installed:
                std::snprintf(buf, sizeof(buf), t.updateInstalledFormat, update.version.c_str());
                message = buf;
                hint = t.updateInstalledHint;
                color = kAccent;
                bold = true;
                border = kAccent;
                borderWidth = 2;
                break;
            case UpdateState::CheckFailed:
                // 確かめられなかっただけで今の版はそのまま動くので、枠は普段のまま（文だけ赤）
                message = std::string(t.updateCheckFailed) + " " + updateErrorText(update.error, t);
                color = kDanger;
                break;
            case UpdateState::InstallFailed:
                message = std::string(t.updateInstallFailed) + " " + updateErrorText(update.error, t);
                color = kDanger;
                bold = true;
                border = kDanger;
                borderWidth = 2;
                break;
        }
    }
    drawCard(pen, x, y, w, h, 20, kCard, border, borderWidth);

    // ボタン 1 個を右端から積む（押している・乗っている見た目とボタン自身の当たり判定はここでまとめて描く）。
    // primary = 「更新する」: アクセントの塗り。ほかは下の行のボタンと同じ地・枠
    double buttonsLeft = x + w - (h - buttonH) / 2;
    const auto button = [&](PanelAction action, const std::string& label, bool primary) {
        const double bw = std::max(120.0, pen.measure(label, buttonSize, true) + 56);
        const double bx = buttonsLeft - bw;
        buttonsLeft = bx - 10;
        drawPillButton(pen, bx, buttonY, bw, buttonH, label, buttonSize, primary, pointerState(action));
        addButton(action, 0, bx, buttonY, bw, buttonH);
    };

    // ボタン: 確かめている間は出さない（連打を避ける）。ほかは状態ごとに 0〜2 個。2 個のときは右が進む側
    if (!update.checking) {
        switch (update.state) {
            case UpdateState::Unknown:
            case UpdateState::UpToDate:
            case UpdateState::CheckFailed:
                button(PanelAction::UpdateCheckNow, t.updateCheckNow, false);
                break;
            case UpdateState::Available:
                if (confirming) {
                    button(PanelAction::UpdateInstall, t.updateConfirmYes, true);
                    button(PanelAction::UpdateCancel, t.updateConfirmNo, false);
                } else if (update.installable) {
                    button(PanelAction::UpdateInstall, t.updateButton, true);
                }
                break;
            case UpdateState::Installed:
                button(PanelAction::UpdateDismiss, t.updateDismiss, false);
                break;
            case UpdateState::InstallFailed:
                button(PanelAction::UpdateDismiss, t.updateDismiss, false);
                button(PanelAction::UpdateRetry, t.updateRetry, false);
                break;
            case UpdateState::Installing: break;  // 更新中はボタンなし
        }
    }

    const double textMax = buttonsLeft - 6 - textX;
    const double size = fitSize(pen, message, 19, 14, textMax, bold);
    if (hint.empty()) {
        pen.text(textX, centerBaseline(y, h, size), message, size, color, bold);
    } else {
        pen.text(textX, y + 30, message, size, color, bold);
        pen.text(textX, y + 54, hint, fitSize(pen, hint, 15, 12, textMax, false), kTextMuted);
    }
}

void MicPanel::drawMuteRow(const Pen& pen, const UiText& t, double y) {
    // 更新の帯と同じ大きさ・同じボタンの部品で、赤い地と枠にする（色だけでなく、絵と文でも伝える）
    const double x = kPad;
    const double w = kRight - kPad;
    const double h = kUpdateRowH;
    const double buttonH = 50;
    const double buttonY = y + (h - buttonH) / 2;
    const double buttonSize = 19;
    drawCard(pen, x, y, w, h, 20, kMuteFill, kMuteBorder, 2);

    const double bw = std::max(120.0, pen.measure(t.unmute, buttonSize, true) + 56);
    const double bx = x + w - (h - buttonH) / 2 - bw;
    drawPillButton(pen, bx, buttonY, bw, buttonH, t.unmute, buttonSize, false, pointerState(PanelAction::Unmute));
    addButton(PanelAction::Unmute, 0, bx, buttonY, bw, buttonH);

    const double iconX = x + kCardPad + 14;
    drawMicMutedIcon(pen.cr, iconX, y + h / 2, 30, kMuteText, kMuteFill);
    const double textX = iconX + 26;
    const double size = fitSize(pen, t.mutedBanner, 21, 14, bx - 16 - textX, true);
    pen.text(textX, centerBaseline(y, h, size), t.mutedBanner, size, kMuteText, true);
}

void MicPanel::drawFooter(const Pen& pen, const UiText& t, const MicState& state, Language language, double y) {
    const double h = 50;
    const double size = 19;
    double x = kPad;
    // 言語
    x += pen.text(x, centerBaseline(y, h, size), t.rowLanguage, size, kTextMuted) + 12;
    {
        const std::string labels[3] = {"日本語", "English", "简体中文"};  // 言語の名前はその言語自身の書き方
        const PanelAction actions[3] = {PanelAction::LanguageJa, PanelAction::LanguageEn, PanelAction::LanguageSc};
        const int selected = language == Language::Ja ? 0 : (language == Language::En ? 1 : 2);
        drawSegmented(pen, x, y, 320, h, labels, actions, 3, selected, size);
        x += 320 + 36;
    }
    // SteamVR と一緒に起動
    const bool usable = state.autostart == Autostart::Enabled || state.autostart == Autostart::Disabled;
    const double quitW = 160;
    const double autostartW = 160;
    x += pen.text(x, centerBaseline(y, h, size), t.rowAutostart, size, kTextMuted) + 12;
    {
        const std::string labels[2] = {t.on, t.off};
        const PanelAction actions[2] = {PanelAction::AutostartOn, PanelAction::AutostartOff};
        const int selected = state.autostart == Autostart::Enabled ? 0 : (state.autostart == Autostart::Disabled ? 1 : -1);
        drawSegmented(pen, x, y, autostartW, h, labels, actions, 2, selected, size, usable);
    }
    // 終了（小さく控えめに。確認中は赤い塗り）
    {
        const double qx = kRight - quitW;
        const int pointer = pointerState(PanelAction::Quit);
        const std::string label = quitArmed_ ? t.quitConfirm : t.quit;
        pen.color(quitArmed_ ? kDanger : (pointer > 0 ? kControlHover : kQuitFill));
        pen.roundedRect(qx, y, quitW, h, h / 2);
        cairo_fill(pen.cr);
        strokeRounded(pen, qx, y, quitW, h, h / 2, kDanger, 2);
        const double qs = fitSize(pen, label, size, 12, quitW - 20, true);
        textCentered(pen, qx + quitW / 2, centerBaseline(y, h, qs), label, qs, quitArmed_ ? kOnAccent : kText, true);
        addButton(PanelAction::Quit, 0, qx, y, quitW, h);
    }

    // 最下行（1 行だけ）: 失敗（赤）があればそれ、無ければ自動起動が使えない理由、どちらも無ければ説明
    const double lineY = y + h + 34;
    const MicError error = state.writeError != MicError::None ? state.writeError : state.readError;
    if (error != MicError::None) {
        const std::string message = errorText(error, t);
        pen.text(kPad, lineY, message, fitSize(pen, message, 18, 12, kRight - kPad, true), kDanger, true);
    } else if (state.loaded && !usable) {
        const std::string note = state.autostart == Autostart::Missing ? t.autostartMissing : t.autostartUnknown;
        pen.text(kPad, lineY, note, fitSize(pen, note, 16, 12, kRight - kPad, false), kTextMuted);
    } else {
        pen.text(kPad, lineY, t.footer, fitSize(pen, t.footer, 16, 12, kRight - kPad, false), kTextMuted);
    }
}

void MicPanel::render(const Config& config, const MicState& state, const VoiceView& voice,
                      const frame_updater::UpdateStatus& update) {
    const Pen pen {cr_, &fonts_};
    const UiText& t = uiText(config.language);
    buttons_.clear();
    tracks_.clear();

    // 地（不透明。コントラスト比は不透明な地で計算している）
    cairo_save(cr_);
    cairo_set_operator(cr_, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr_);
    cairo_restore(cr_);
    pen.color(kBg);
    pen.roundedRect(0, 0, kWidth, kHeight, 24);
    cairo_fill(cr_);

    // 左のカラム = 切り替え、右のカラム = 声のチェック、下 = 全幅の 1 行
    drawHeader(pen, t, state);
    // 左の列: タブで「かんたん」（プリセット）と「細かく調整」（1 つずつの設定）を切り替える。つながりは両方で下に出す
    drawTabs(pen, t, config.tab);
    if (config.tab == PanelTab::Quick) {
        drawModeCards(pen, t, state, kModeY);
    } else {
        drawToggleRows(pen, t, state, kToggleY);
        drawNsSliders(pen, t, state, kSliderY);
    }
    drawPipeline(pen, t, state, kPipelineY);
    drawVoice(pen, t, voice, kVoiceY);
    // ミュート中は、更新の帯の場所（両方のタブで見える全幅の帯）にミュートの帯を出す。解除されたら更新の帯に戻る
    if (state.muteKnown && state.muted) {
        drawMuteRow(pen, t, kUpdateRowY);
    } else {
        drawUpdateRow(pen, t, update, kUpdateRowY);
    }
    drawFooter(pen, t, state, config.language, kFooterY);

    // 押せなくなったボタンに乗っていた印は外す
    bool hoverFound = false;
    for (const auto& b : buttons_) hoverFound |= b.usable && b.hit == hover_;
    if (!hoverFound) hover_ = {};
    cairo_surface_flush(surface_);
}

const std::vector<uint8_t>& MicPanel::toRgba() {
    surfaceToRgba(surface_, rgba_);
    return rgba_;
}

bool MicPanel::writePng(const std::string& path) const {
    return cairo_surface_write_to_png(surface_, path.c_str()) == CAIRO_STATUS_SUCCESS;
}

bool MicPanel::trackCenter(PanelAction action, double& x, double& y) const {
    for (const Track& track : tracks_) {
        if (track.action != action) continue;
        x = (track.x0 + track.x1) / 2;
        y = track.cy;
        return true;
    }
    return false;
}

bool MicPanel::buttonCenter(PanelAction action, double& x, double& y) const {
    for (const auto& b : buttons_) {
        if (b.hit.action != action || b.hit.index != 0) continue;
        x = b.x + b.w / 2;
        y = b.y + b.h / 2;
        return true;
    }
    return false;
}

int MicPanel::width() const {
    return kWidth;
}

int MicPanel::height() const {
    return kHeight;
}

void renderThumbnail(const FontSet& fonts, int size, std::vector<uint8_t>& rgba, const std::string& pngPath) {
    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size);
    cairo_t* cr = cairo_create(surface);
    const Pen pen {cr, &fonts};
    const double s = size / 256.0;

    pen.color(kBg);
    pen.roundedRect(8 * s, 8 * s, 240 * s, 240 * s, 48 * s);
    cairo_fill(cr);
    // 内側の縁（カードと同じ明るい 1px）
    cairo_set_source_rgba(cr, 1, 1, 1, 0.08);
    cairo_set_line_width(cr, 2 * s);
    pen.roundedRect(9 * s, 9 * s, 238 * s, 238 * s, 47 * s);
    cairo_stroke(cr);

    // マイクの頭（縦長のカプセル、イメージカラー）と、その光彩
    for (int i = 3; i >= 1; --i) {
        pen.color(kAccent, 0.07);
        pen.roundedRect((100 - i * 5) * s, (30 - i * 5) * s, (56 + i * 10) * s, (96 + i * 10) * s, (28 + i * 5) * s);
        cairo_fill(cr);
    }
    pen.color(kAccent);
    pen.roundedRect(100 * s, 30 * s, 56 * s, 96 * s, 28 * s);
    cairo_fill(cr);
    // 頭の網目（横線 3 本）
    pen.color(kBg, 0.55);
    cairo_set_line_width(cr, 5 * s);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    for (int i = 0; i < 3; ++i) {
        const double y = (62 + i * 16) * s;
        cairo_move_to(cr, 114 * s, y);
        cairo_line_to(cr, 142 * s, y);
    }
    cairo_stroke(cr);
    // 受け（U 字）・柄・台
    pen.color(kText);
    cairo_set_line_width(cr, 10 * s);
    cairo_arc(cr, 128 * s, 94 * s, 48 * s, 0, M_PI);
    cairo_stroke(cr);
    cairo_move_to(cr, 128 * s, 142 * s);
    cairo_line_to(cr, 128 * s, 170 * s);
    cairo_move_to(cr, 100 * s, 172 * s);
    cairo_line_to(cr, 156 * s, 172 * s);
    cairo_stroke(cr);

    const double w = pen.measure("Mic", 56 * s, true);
    pen.text((size - w) / 2, 230 * s, "Mic", 56 * s, kText, true);

    cairo_surface_flush(surface);
    surfaceToRgba(surface, rgba);
    if (!pngPath.empty()) cairo_surface_write_to_png(surface, pngPath.c_str());
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}
