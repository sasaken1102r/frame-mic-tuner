// マイクのパネルの実装。配置は見本（1200×788）のとおり。部品は frame-ui、色は theme.h（frame-ui と同じ）だけを使う。
#include "mic_panel.h"

#include "draw.h"
#include "i18n.h"
#include "outputs.h"
#include "theme.h"

#include "frame_apps_cairo.h"  // vendor/frame-apps/cpp

#include <cairo.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <initializer_list>

namespace {

using frame_ui::Rect;

constexpr int kWidth = 1200;
constexpr int kHeight = 788;

// ---- 見出し（状態ラベルとミュートのボタンは幅を決めてあるので、ミュートしても動かない） ----
constexpr Rect kChipRect {140, 28, 168, 44};
constexpr Rect kMuteRect {324, 22, 220, 56};
constexpr Rect kAppsRect {964, 22, 208, 56};
// ---- マイクの設定 ----
constexpr Rect kOutputSelect {28, 92, 380, 60};   ///< 「音の出口」の欄
constexpr Rect kMicSelect {420, 92, 184, 60};     ///< 「マイク」の欄
constexpr Rect kSettingsCard {28, 168, 576, 492};
constexpr Rect kTabsRect {50, 188, 532, 56};
constexpr double kInnerX = 50;                    ///< 左のカードの中身の左端
constexpr double kInnerRight = 582;               ///< 左のカードの中身の右端
constexpr double kInnerW = kInnerRight - kInnerX;
constexpr Rect kPipelineRect {50, 568, 532, 74};
constexpr Rect kVoiceCard {620, 92, 552, 568};
// ---- アプリと更新 ----
constexpr Rect kAppCard {28, 92, 560, 568};
constexpr Rect kUpdateBox {50, 244, 516, 112};
constexpr Rect kRelatedCard {604, 92, 568, 568};
constexpr Rect kAppsModal {56, 56, 1088, 676};
constexpr Rect kAppsList {84, 148, 1032, 392};
constexpr Rect kConfirmCard {260, 150, 680, 476};
constexpr double kAppRowH = 84;    ///< 一覧のアプリの行の高さ
constexpr double kAppRowGap = 14;  ///< 一覧のアプリの行の間
// ---- 一番下の行（frame-ui の drawFooter。部品の行は y 676〜732、注意書きはその下） ----
constexpr Rect kFooterRect {28, 674, 1144, 88};
// ---- 重ねた画面 ----
constexpr Rect kOutputModal {56, 56, 1088, 676};
constexpr Rect kOutputList {84, 148, 1032, 484};
constexpr Rect kMicModal {160, 140, 880, 440};
constexpr Rect kMicList {188, 240, 824, 312};
constexpr double kRowH = 76;        ///< 一覧の行の高さ
constexpr double kRowGap = 8;       ///< 一覧の行の間
constexpr double kRowButtonW = 196; ///< 行の右のボタン・札の幅

/**
 * 色を frame-ui の色にする（Canvas::text に渡す）。
 * @param c 色
 * @return frame-ui の色
 */
frame_ui::Rgb rgb(Color c) {
    return {c.r, c.g, c.b};
}

/**
 * 色を設定する。
 * @param cr cairo
 * @param c 色
 * @param alpha 不透明度
 */
void setColor(cairo_t* cr, Color c, double alpha = 1.0) {
    cairo_set_source_rgba(cr, c.r, c.g, c.b, alpha);
}

/**
 * 角丸の四角を塗る。
 * @param cr cairo
 * @param r 矩形
 * @param radius 角の半径
 * @param c 色
 */
void fillRounded(cairo_t* cr, Rect r, double radius, Color c) {
    setColor(cr, c);
    frame_ui::roundedRect(cr, r, radius);
    cairo_fill(cr);
}

/**
 * 内側に収まる角丸の枠線を引く。
 * @param cr cairo
 * @param r 矩形
 * @param radius 角の半径
 * @param c 色
 * @param width 線の太さ
 * @param dashed 点線にするか
 */
void strokeRounded(cairo_t* cr, Rect r, double radius, Color c, double width, bool dashed = false) {
    setColor(cr, c);
    cairo_set_line_width(cr, width);
    if (dashed) {
        const double dash[] = {6, 4};
        cairo_set_dash(cr, dash, 2, 0);
    }
    frame_ui::roundedRect(cr, {r.x + width / 2, r.y + width / 2, r.w - width, r.h - width},
                          std::max(0.0, radius - width / 2));
    cairo_stroke(cr);
    cairo_set_dash(cr, nullptr, 0, 0);
}

/**
 * 塗りの丸。
 * @param cr cairo
 * @param cx 真ん中の x
 * @param cy 真ん中の y
 * @param r 半径
 * @param c 色
 */
void fillCircle(cairo_t* cr, double cx, double cy, double r, Color c) {
    setColor(cr, c);
    cairo_new_sub_path(cr);
    cairo_arc(cr, cx, cy, r, 0, 2 * M_PI);
    cairo_fill(cr);
}

/**
 * 描いたものを薄くする（使えない部品・前に使った出口）。alpha が 1 ならそのまま描く。
 * @param cr cairo
 * @param alpha 不透明度
 * @param draw 描く処理
 */
void withAlpha(cairo_t* cr, double alpha, const std::function<void()>& draw) {
    if (alpha >= 1.0) {
        draw();
        return;
    }
    cairo_push_group(cr);
    draw();
    cairo_pop_group_to_source(cr);
    cairo_paint_with_alpha(cr, alpha);
}

/**
 * 2 つの矩形の重なり（一覧の見えている枠で、行のボタンの当たり判定を切る）。
 * @param a 矩形
 * @param b 矩形
 * @return 重なり（無ければ空）
 */
Rect intersect(Rect a, Rect b) {
    const double x0 = std::max(a.x, b.x);
    const double y0 = std::max(a.y, b.y);
    const double x1 = std::min(a.right(), b.right());
    const double y1 = std::min(a.bottom(), b.bottom());
    if (x1 - x0 < 8 || y1 - y0 < 8) return {};
    return {x0, y0, x1 - x0, y1 - y0};
}

// ---- 絵（見本の SVG と同じ線。viewBox の座標で描く） ----

/**
 * 絵を描く準備: (x, y) に一辺 size で置き、viewBox の 1 を size / box にする。線の太さは viewBox の単位。
 * @param cr cairo
 * @param x 左
 * @param y 上
 * @param size 一辺（px）
 * @param box viewBox の一辺
 * @param c 色
 * @param strokeWidth 線の太さ（viewBox の単位）
 */
void beginIcon(cairo_t* cr, double x, double y, double size, double box, Color c, double strokeWidth) {
    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_scale(cr, size / box, size / box);
    setColor(cr, c);
    cairo_set_line_width(cr, strokeWidth);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_new_path(cr);
}

/**
 * スピーカーの絵（箱・コーンと音の波 2 本。viewBox 40）。
 * @param cr cairo
 * @param x 左
 * @param y 上
 * @param size 一辺（px）
 * @param c 色
 * @param strokeWidth 線の太さ（viewBox の単位）
 */
void drawSpeakerIcon(cairo_t* cr, double x, double y, double size, Color c, double strokeWidth = 3.4) {
    beginIcon(cr, x, y, size, 40, c, strokeWidth);
    cairo_move_to(cr, 6, 15);
    cairo_line_to(cr, 13, 15);
    cairo_line_to(cr, 22, 8);
    cairo_line_to(cr, 22, 32);
    cairo_line_to(cr, 13, 25);
    cairo_line_to(cr, 6, 25);
    cairo_close_path(cr);
    cairo_stroke(cr);
    cairo_move_to(cr, 28, 14);
    cairo_curve_to(cr, 30, 16, 31, 18, 31, 20);
    cairo_curve_to(cr, 31, 22, 30, 24, 28, 26);
    cairo_stroke(cr);
    cairo_move_to(cr, 32, 9);
    cairo_curve_to(cr, 35.5, 12, 37, 16, 37, 20);
    cairo_curve_to(cr, 37, 24, 35.5, 28, 32, 31);
    cairo_stroke(cr);
    cairo_restore(cr);
}

/**
 * イヤホンの絵（左右のイヤーピースと、合わさるケーブル。viewBox 40）。
 * @param cr cairo
 * @param x 左
 * @param y 上
 * @param size 一辺（px）
 * @param c 色
 * @param strokeWidth 線の太さ（viewBox の単位）
 */
void drawEarphonesIcon(cairo_t* cr, double x, double y, double size, Color c, double strokeWidth = 3.4) {
    beginIcon(cr, x, y, size, 40, c, strokeWidth);
    cairo_new_sub_path(cr);
    cairo_arc(cr, 11, 12, 6, 0, 2 * M_PI);
    cairo_new_sub_path(cr);
    cairo_arc(cr, 29, 12, 6, 0, 2 * M_PI);
    cairo_stroke(cr);
    cairo_move_to(cr, 11, 18);
    cairo_line_to(cr, 11, 24);
    cairo_curve_to(cr, 11, 28, 15, 30, 20, 30);
    cairo_curve_to(cr, 25, 30, 29, 28, 29, 24);
    cairo_line_to(cr, 29, 18);
    cairo_move_to(cr, 20, 30);
    cairo_line_to(cr, 20, 36);
    cairo_stroke(cr);
    cairo_restore(cr);
}

/**
 * 出口の種類の絵（スピーカーかイヤホン）。
 * @param cr cairo
 * @param speaker Frame のスピーカーなら true
 * @param x 左
 * @param y 上
 * @param size 一辺（px）
 * @param c 色
 * @param strokeWidth 線の太さ（viewBox の単位）
 */
void drawOutputIcon(cairo_t* cr, bool speaker, double x, double y, double size, Color c, double strokeWidth = 3.4) {
    if (speaker) {
        drawSpeakerIcon(cr, x, y, size, c, strokeWidth);
    } else {
        drawEarphonesIcon(cr, x, y, size, c, strokeWidth);
    }
}

/**
 * マイクの絵（viewBox 24）。muted なら斜線を入れる（ミュート中の印。色だけで伝えない）。
 * @param cr cairo
 * @param x 左
 * @param y 上
 * @param size 一辺（px）
 * @param c 色
 * @param muted 斜線を入れるか
 */
void drawMicIcon(cairo_t* cr, double x, double y, double size, Color c, bool muted) {
    beginIcon(cr, x, y, size, 24, c, muted ? 2.2 : 2.0);
    const double top = muted ? 3 : 2;
    frame_ui::roundedRect(cr, {9, top, 6, 11}, 3);
    cairo_stroke(cr);
    cairo_new_sub_path(cr);
    cairo_arc_negative(cr, 12, top + 8, 7, M_PI, 0);
    cairo_stroke(cr);
    cairo_move_to(cr, 12, top + 15);
    cairo_line_to(cr, 12, muted ? 21 : 22);
    cairo_stroke(cr);
    if (muted) {
        cairo_move_to(cr, 4, 4);
        cairo_line_to(cr, 20, 20);
        cairo_stroke(cr);
    }
    cairo_restore(cr);
}

/**
 * 4 つの四角の絵（「アプリと更新」。viewBox 20）。
 * @param cr cairo
 * @param x 左
 * @param y 上
 * @param size 一辺（px）
 * @param c 色
 */
void drawGridIcon(cairo_t* cr, double x, double y, double size, Color c) {
    beginIcon(cr, x, y, size, 20, c, 1);
    for (const double gx : {2.0, 11.0}) {
        for (const double gy : {2.0, 11.0}) frame_ui::roundedRect(cr, {gx, gy, 7, 7}, 2);
    }
    cairo_fill(cr);
    cairo_restore(cr);
}

/**
 * 線の折れ線（山かっこ・✕・入れ替えの矢印など）を描く。
 * @param cr cairo
 * @param x 左
 * @param y 上
 * @param size 一辺（px）
 * @param box viewBox の一辺
 * @param c 色
 * @param strokeWidth 線の太さ（viewBox の単位）
 * @param points 点（x, y の並び）。NAN の組で線を切る
 */
void drawPolyline(cairo_t* cr, double x, double y, double size, double box, Color c, double strokeWidth,
                  std::initializer_list<double> points) {
    beginIcon(cr, x, y, size, box, c, strokeWidth);
    bool start = true;
    for (auto it = points.begin(); it != points.end() && it + 1 != points.end(); it += 2) {
        if (std::isnan(*it)) {
            start = true;
            continue;
        }
        if (start) {
            cairo_move_to(cr, *it, *(it + 1));
        } else {
            cairo_line_to(cr, *it, *(it + 1));
        }
        start = false;
    }
    cairo_stroke(cr);
    cairo_restore(cr);
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
    setColor(cr, c);
    cairo_move_to(cr, cx - s * 0.32, cy - s * 0.42);
    cairo_line_to(cr, cx + s * 0.44, cy);
    cairo_line_to(cr, cx - s * 0.32, cy + s * 0.42);
    cairo_close_path(cr);
    cairo_fill(cr);
}

/**
 * ■（停止）を描く。
 * @param cr cairo
 * @param cx 真ん中の x
 * @param cy 真ん中の y
 * @param s 一辺（px）
 * @param c 色
 */
void drawStopIcon(cairo_t* cr, double cx, double cy, double s, Color c) {
    fillRounded(cr, {cx - s / 2, cy - s / 2, s, s}, s * 0.15, c);
}

// ---- 文字 ----

/**
 * 文言の %s を 1 つ埋める（printf の書式。表の文言は自分の表のものだけ）。
 * @param format 書式
 * @param arg 入れる文字列
 * @return 文字列
 */
std::string format1(const char* format, const std::string& arg) {
    char text[640];
    std::snprintf(text, sizeof(text), format, arg.c_str());
    return text;
}

/**
 * 文言の %1$s・%2$s を埋める（言語で順番が違ってよい）。
 * @param format 書式
 * @param a 1 つ目
 * @param b 2 つ目
 * @return 文字列
 */
std::string format2(const char* format, const std::string& a, const std::string& b) {
    char text[640];
    std::snprintf(text, sizeof(text), format, a.c_str(), b.c_str());
    return text;
}

/**
 * 文言の %d を埋める。
 * @param format 書式
 * @param n 数
 * @return 文字列
 */
std::string formatInt(const char* format, int n) {
    char text[160];
    std::snprintf(text, sizeof(text), format, n);
    return text;
}

/**
 * 行の頭に来てはいけない記号か（禁則。前の行に残す）。
 * @param token 1 文字ぶん
 * @return そうなら true
 */
bool closesLine(const std::string& token) {
    static const char* const kClosing[] = {"、", "。", "，", "．", "）", "」", "』", "】", "・", "…", "：", "；", "！", "？",
                                          ",", ".", ")", ":", ";", "!", "?"};
    for (const char* c : kClosing) {
        if (token.rfind(c, 0) == 0) return true;
    }
    return false;
}

/**
 * 幅に収まるよう行に分ける（英語は語の切れ目、日本語・中国語は文字ごと。句読点は行頭に来ないようにする）。
 * 入りきらない分は最後の行の末尾を「…」にする。
 * @param ui 描く先（文字を測る）
 * @param text 文字列
 * @param size 大きさ
 * @param bold 太字か
 * @param width 幅
 * @param maxLines 行の上限
 * @return 行
 */
std::vector<std::string> wrapText(const frame_ui::Canvas& ui, const std::string& text, double size, bool bold,
                                  double width, int maxLines) {
    // 区切り: 英数字の続き（後ろの空白ごと）か、それ以外の 1 文字
    std::vector<std::string> tokens;
    for (size_t i = 0; i < text.size();) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        size_t end = i + 1;
        if (c < 0x80 && c != ' ') {
            while (end < text.size() && static_cast<unsigned char>(text[end]) < 0x80 && text[end] != ' ') ++end;
        } else if (c >= 0x80) {
            while (end < text.size() && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80) ++end;
        }
        while (end < text.size() && text[end] == ' ') ++end;  // 後ろの空白はその語に付ける
        tokens.push_back(text.substr(i, end - i));
        i = end;
    }
    const auto trimRight = [](std::string s) {
        while (!s.empty() && s.back() == ' ') s.pop_back();
        return s;
    };
    std::vector<std::string> lines;
    std::string line;
    for (const std::string& token : tokens) {
        if (line.empty()) {
            line = token;
        } else if (ui.measure(trimRight(line + token), size, bold) <= width || closesLine(token)) {
            line += token;
        } else {
            lines.push_back(trimRight(line));
            line = token;
        }
    }
    if (!line.empty()) lines.push_back(trimRight(line));
    if (static_cast<int>(lines.size()) > maxLines) {
        std::string rest;
        for (size_t i = maxLines - 1; i < lines.size(); ++i) rest += lines[i] + (i + 1 < lines.size() ? " " : "");
        lines.resize(maxLines);
        lines.back() = ui.ellipsize(rest, size, width, bold);
    }
    for (std::string& l : lines) l = ui.ellipsize(l, size, width, bold);
    return lines;
}

/**
 * 行に分けて描く。
 * @param ui 描く先
 * @param x 左
 * @param firstBaseline 1 行目のベースライン
 * @param lineH 行の高さ
 * @param lines 行
 * @param size 大きさ
 * @param c 色
 * @param bold 太字か
 */
void drawLines(const frame_ui::Canvas& ui, double x, double firstBaseline, double lineH,
               const std::vector<std::string>& lines, double size, Color c, bool bold = false) {
    for (size_t i = 0; i < lines.size(); ++i) ui.text(x, firstBaseline + lineH * i, lines[i], size, rgb(c), bold);
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

/**
 * 「2026-10-08」を「10/8」にする。
 * @param date YYYY-MM-DD
 * @return M/D（読めなければ空）
 */
std::string monthDay(const std::string& date) {
    int year = 0;
    int month = 0;
    int day = 0;
    if (std::sscanf(date.c_str(), "%d-%d-%d", &year, &month, &day) != 3 || month < 1 || day < 1) return "";
    return std::to_string(month) + "/" + std::to_string(day);
}

/**
 * 設定のまとめ（「エコー除去オン・ノイズ除去オン」）。
 * @param echo エコー除去
 * @param ns ノイズ除去
 * @param t 言語の表
 * @return 文字列
 */
std::string summaryText(bool echo, bool ns, const UiText& t) {
    return std::string(echo ? t.sumEchoOn : t.sumEchoOff) + (ns ? t.sumNsOn : "");
}

/** 音の出口の一覧の 1 行（つながっている出口と、覚えているがつながっていない出口）。 */
struct OutputEntry {
    std::string key;
    std::string name;        ///< 表示名
    bool connected = false;
    bool active = false;     ///< 今の出口
    OutputProfile profile;   ///< 覚えている設定（無ければ初めての設定）
};

/**
 * 音の出口の一覧を作る（つながっているもの → 前に使ったもの（最後に使った日の新しい順））。
 * @param config 設定（出口ごとの設定）
 * @param devices 今の一覧
 * @param activeKey 今の出口
 * @param t 言語の表
 * @return 一覧
 */
std::vector<OutputEntry> outputEntries(const Config& config, const AudioDevices& devices, const std::string& activeKey,
                                       const UiText& t) {
    std::vector<OutputEntry> entries;
    const auto profileOf = [&](const std::string& key, const std::string& name) {
        const auto it = config.outputs.find(key);
        return it != config.outputs.end() ? it->second : firstProfile(key, name);
    };
    for (const AudioEndpoint& e : devices.outputs) {
        entries.push_back({e.key, outputDisplayName(e.key, e.name, t), true, e.key == activeKey, profileOf(e.key, e.name)});
    }
    if (!activeKey.empty() && devices.findOutput(activeKey) == nullptr) {
        // 一覧をまだ読めていないとき: 今の出口だけは、つながっている出口として出す
        const OutputProfile profile = profileOf(activeKey, "");
        entries.insert(entries.begin(), {activeKey, outputDisplayName(activeKey, profile.name, t), true, true, profile});
    }
    std::vector<OutputEntry> past;
    for (const auto& entry : config.outputs) {
        const bool listed = std::any_of(entries.begin(), entries.end(), [&](const OutputEntry& e) { return e.key == entry.first; });
        if (listed) continue;
        past.push_back({entry.first, outputDisplayName(entry.first, entry.second.name, t), false, false, entry.second});
    }
    std::stable_sort(past.begin(), past.end(),
                     [](const OutputEntry& a, const OutputEntry& b) { return a.profile.lastUsed > b.profile.lastUsed; });
    entries.insert(entries.end(), past.begin(), past.end());
    return entries;
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
        case MicError::WriteMuteOn: return text.errWriteMuteOn;
        case MicError::WriteOutput: return text.errWriteOutput;
        case MicError::WriteInput: return text.errWriteInput;
    }
    return "";
}

frame_ui::Lang uiLang(Language language) {
    if (language == Language::En) return frame_ui::Lang::En;
    if (language == Language::Sc) return frame_ui::Lang::Sc;
    return frame_ui::Lang::Ja;
}

std::string outputDisplayName(const std::string& key, const std::string& name, const UiText& t) {
    if (isBuiltinSpeaker(key)) return t.speakerName;
    return name.empty() ? key : name;
}

namespace {

/**
 * サムネイル（マイクの絵と「Mic」）を描く（renderThumbnail と、アプリと更新の画面のアイコンで共通）。
 * @param cr cairo（一辺 size の画像）
 * @param fonts フォント
 * @param size 一辺
 */
void drawThumbnail(cairo_t* cr, const FontSet& fonts, int size) {
    const Pen pen {cr, &fonts};
    const double s = size / 256.0;

    pen.color(kBg);
    pen.roundedRect(8 * s, 8 * s, 240 * s, 240 * s, 48 * s);
    cairo_fill(cr);
    // 内側の縁（明るい 1px）
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
}

}  // namespace

MicPanel::MicPanel(const FontSet& fonts) : fonts_(fonts) {
    surface_ = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, kWidth, kHeight);
    cr_ = cairo_create(surface_);
    cairo_font_options_t* options = cairo_font_options_create();
    cairo_font_options_set_antialias(options, CAIRO_ANTIALIAS_GRAY);
    cairo_font_options_set_hint_style(options, CAIRO_HINT_STYLE_SLIGHT);
    cairo_set_font_options(cr_, options);
    cairo_font_options_destroy(options);
    // アプリと更新の画面のアイコン（サムネイルと同じ絵を 1 回だけ描いておく）
    appIcon_ = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 256, 256);
    cairo_t* iconCr = cairo_create(appIcon_);
    drawThumbnail(iconCr, fonts_, 256);
    cairo_destroy(iconCr);
}

MicPanel::~MicPanel() {
    cairo_surface_destroy(appIcon_);
    cairo_destroy(cr_);
    cairo_surface_destroy(surface_);
}

void MicPanel::addButton(const PanelHit& hit, Rect rect, bool inList) {
    if (rect.w <= 0 || rect.h <= 0) return;
    buttons_.push_back({hit, rect, inList});
}

int MicPanel::look(Rect rect) const {
    // 重ねた画面を出している間は、後ろを描くあいだ pointer_.inside を外しておく（render）ので、後ろは乗っている見た目にならない
    if (!pointer_.inside || listDragging_ || !rect.contains(pointer_.x, pointer_.y)) return 0;
    return pointer_.down ? 2 : 1;
}

PanelHit MicPanel::hitTest(double x, double y, bool* inList) const {
    for (auto it = buttons_.rbegin(); it != buttons_.rend(); ++it) {
        if (!it->rect.contains(x, y)) continue;
        if (inList != nullptr) *inList = it->inList;
        return it->hit;
    }
    if (inList != nullptr) *inList = false;
    return {};
}

double MicPanel::trackValue(const Track& track, double x) {
    const double fraction = std::clamp((x - track.x0) / std::max(1.0, track.x1 - track.x0), 0.0, 1.0);
    const double value = track.min + (track.max - track.min) * fraction;
    return track.action == PanelAction::NsVadSlider ? clampNsVad(value) : clampNsGrace(value);
}

void MicPanel::applyLocal(const PanelHit& hit) {
    switch (hit.action) {
        case PanelAction::ShowApps:
            view_ = PanelView::Apps;
            overlay_ = PanelOverlay::None;
            break;
        case PanelAction::ShowSettings: view_ = PanelView::Settings; break;
        case PanelAction::OpenOutputPicker:
            overlay_ = PanelOverlay::OutputPicker;
            listScroll_ = 0;
            break;
        case PanelAction::OpenMicPicker:
            overlay_ = PanelOverlay::MicPicker;
            listScroll_ = 0;
            break;
        case PanelAction::CloseOverlay: overlay_ = PanelOverlay::None; break;
        case PanelAction::OutputView:
            viewKey_ = hit.key;
            overlay_ = PanelOverlay::None;
            break;
        case PanelAction::OutputUse: viewKey_ = hit.key; break;  // 一覧は開いたまま（「いま使用中」に変わるのを見せる）
        case PanelAction::OutputForget:
            if (viewKey_ == hit.key) viewKey_.clear();
            break;
        case PanelAction::MicUse: overlay_ = PanelOverlay::None; break;
        case PanelAction::AppsOpenList:
            overlay_ = PanelOverlay::AppsList;
            listScroll_ = 0;
            launchResult_ = frame_apps::LaunchResult::Started;
            break;
        case PanelAction::AppInstall:
        case PanelAction::AppsOpenMenu:
            // まず確認（実行するコマンドを見せる）。開くのは確認の「Konsole で開く」
            confirmBack_ = overlay_ == PanelOverlay::AppsList ? PanelOverlay::AppsList : PanelOverlay::None;
            confirmKey_ = hit.action == PanelAction::AppInstall ? hit.key : std::string();
            overlay_ = PanelOverlay::AppsConfirm;
            launchResult_ = frame_apps::LaunchResult::Started;
            break;
        case PanelAction::AppsConfirmCancel:
            overlay_ = confirmBack_;
            launchResult_ = frame_apps::LaunchResult::Started;
            break;
        default: break;
    }
}

void MicPanel::setLaunchResult(frame_apps::LaunchResult result) {
    launchResult_ = result;
    if (result == frame_apps::LaunchResult::Started && overlay_ == PanelOverlay::AppsConfirm) overlay_ = confirmBack_;
}

void MicPanel::openConfirmForPreview(const std::string& key, bool fromList) {
    view_ = PanelView::Apps;
    confirmKey_ = key;
    confirmBack_ = fromList ? PanelOverlay::AppsList : PanelOverlay::None;
    overlay_ = PanelOverlay::AppsConfirm;
}

bool MicPanel::pointerMove(double x, double y) {
    pointer_.x = x;
    pointer_.y = y;
    pointer_.inside = true;
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
    if (listTracking_) {
        // 一覧を押したまま 8px 以上動かしたら、押した行は取り消してスクロールにする
        if (!listDragging_ && std::fabs(y - listStartY_) > 8) {
            listDragging_ = true;
            listPress_ = {};
        }
        if (!listDragging_) return false;
        const double next = std::clamp(listStartScroll_ + (listStartY_ - y), 0.0, listMaxScroll_);
        if (next == listScroll_) return false;
        listScroll_ = next;
        return true;
    }
    const PanelHit now = hitTest(x, y);
    if (now == hover_) return false;
    hover_ = now;
    return true;
}

PanelHit MicPanel::pointerDown(double x, double y, double now) {
    pointer_ = {x, y, true, true};
    bool inList = false;
    PanelHit hit = hitTest(x, y, &inList);
    hover_ = hit;
    if (inList || (overlay_ != PanelOverlay::None && listRect_.contains(x, y) && hit.action == PanelAction::None)) {
        // 一覧の中: 離したときに返す（そのまま動かせばスクロール）
        listTracking_ = true;
        listDragging_ = false;
        listPress_ = hit;
        listStartY_ = y;
        listStartScroll_ = listScroll_;
        return {};
    }
    if (hit.action == PanelAction::NsVadSlider || hit.action == PanelAction::NsGraceSlider) {
        // バーは押したところへ飛び、そのままドラッグ
        for (const Track& track : tracks_) {
            if (track.action != hit.action) continue;
            dragAction_ = track.action;
            dragValue_ = trackValue(track, x);
        }
    }
    if (hit.action == PanelAction::Quit) {
        // 誤って押しても終わらないよう、1 回目は確認の表示にするだけ
        updateArmed_ = false;
        if (quitArmed_ && now <= quitArmedUntil_) return hit;
        quitArmed_ = true;
        quitArmedUntil_ = now + kQuitConfirmSec;
        return {};
    }
    quitArmed_ = false;  // 別のボタンを押したら確認は取り消す
    if (hit.action == PanelAction::UpdateInstall) {
        // 1 回目は確認の表示（やめる・更新する）にするだけ
        updateArmed_ = true;
        updateArmedUntil_ = now + kQuitConfirmSec;
        return {};
    }
    const bool confirmed = hit.action == PanelAction::UpdateConfirmYes && updateArmed_;
    updateArmed_ = false;
    if (hit.action == PanelAction::UpdateConfirmNo) return {};
    if (hit.action == PanelAction::UpdateConfirmYes && !confirmed) return {};
    applyLocal(hit);
    return hit;
}

PanelHit MicPanel::pointerUp() {
    pointer_.down = false;
    dragAction_ = PanelAction::None;
    PanelHit result;
    if (listTracking_) {
        if (!listDragging_ && listPress_.action != PanelAction::None && hitTest(pointer_.x, pointer_.y) == listPress_) {
            result = listPress_;
        }
        listTracking_ = false;
        listDragging_ = false;
        listPress_ = {};
        applyLocal(result);
    }
    return result;
}

bool MicPanel::pointerLeave() {
    const bool changed = pointer_.inside || hover_.action != PanelAction::None || listTracking_;
    pointer_.inside = false;
    pointer_.down = false;
    hover_ = {};
    dragAction_ = PanelAction::None;
    listTracking_ = false;
    listDragging_ = false;
    listPress_ = {};
    return changed;
}

bool MicPanel::scroll(double dy) {
    if (dy == 0.0 || overlay_ == PanelOverlay::None || overlay_ == PanelOverlay::AppsConfirm) return false;
    const double next = std::clamp(listScroll_ - dy * kScrollPxPerUnit, 0.0, listMaxScroll_);
    if (next == listScroll_) return false;
    listScroll_ = next;
    return true;
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

void MicPanel::resetView() {
    view_ = PanelView::Settings;
    overlay_ = PanelOverlay::None;
    viewKey_.clear();
    listScroll_ = 0;
    listTracking_ = false;
    listDragging_ = false;
    listPress_ = {};
    updateArmed_ = false;
    quitArmed_ = false;
    launchResult_ = frame_apps::LaunchResult::Started;
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

void MicPanel::displayedNsValues(double& vad, double& grace) const {
    const bool held = now_ <= holdUntil_;
    vad = held ? holdVad_ : baseVad_;
    grace = held ? holdGrace_ : baseGrace_;
    if (dragAction_ == PanelAction::NsVadSlider) vad = dragValue_;
    if (dragAction_ == PanelAction::NsGraceSlider) grace = dragValue_;
}

bool MicPanel::endDrag() {
    if (dragAction_ == PanelAction::None) return false;
    dragAction_ = PanelAction::None;
    pointer_.down = false;
    return true;
}

void MicPanel::setDragForPreview(PanelAction action, double value) {
    dragAction_ = action;
    dragValue_ = action == PanelAction::NsVadSlider ? clampNsVad(value) : clampNsGrace(value);
}

void MicPanel::armQuitForPreview() {
    quitArmed_ = true;
    quitArmedUntil_ = 1e300;
}

void MicPanel::armUpdateForPreview() {
    updateArmed_ = true;
    updateArmedUntil_ = 1e300;
}

void MicPanel::setPointerForPreview(double x, double y, bool down) {
    pointer_ = {x, y, true, down};
    hover_ = hitTest(x, y);
}

void MicPanel::showForPreview(PanelView view, PanelOverlay overlay) {
    view_ = view;
    overlay_ = overlay;
}

// ============================================================================
// 見出し
// ============================================================================

void MicPanel::drawHeader(const frame_ui::Canvas& ui, const UiText& t, const PanelModel& model) {
    cairo_t* cr = ui.cr;
    const MicState& state = model.state;
    const double titleSize = ui.fit(t.title, frame_ui::kTitleSize, 22, kChipRect.x - frame_ui::kPanelPadX - 12, true);
    ui.text(frame_ui::kPanelPadX, frame_ui::centerBaseline(24, frame_ui::kHeaderH, titleSize), t.title, titleSize,
            rgb(kText), true);

    // 状態ラベル（幅 168 で固定。ミュート中は赤い地にマイクに斜線の絵、使用中は緑の ●、未使用・読み込み中は灰の ○）
    const bool muted = state.muteKnown && state.muted;
    std::string label;
    Color fill = kIdleFill;
    Color fg = kIdleText;
    int mark = 0;  // 0 = ○、1 = ●、2 = マイクに斜線、3 = なし
    if (muted) {
        label = t.micMuted;
        fill = kMuteFill;
        fg = kMuteText;
        mark = 2;
    } else if (!state.loaded) {
        label = t.loading;
        mark = 3;
    } else if (!state.linksKnown) {
        label = t.chipUnknown;
    } else if (state.inUse) {
        label = t.micInUse;
        fill = kSuccessTint;
        fg = kSuccess;
        mark = 1;
    } else {
        label = t.micIdle;
    }
    fillRounded(cr, kChipRect, kChipRect.h / 2, fill);
    const double markW = mark == 2 ? 20 : (mark == 3 ? 0 : 12);
    const double gap = markW > 0 ? 8 : 0;
    const double size = ui.fit(label, frame_ui::kControlSize, 13, kChipRect.w - 32 - markW - gap, true);
    const double textW = ui.measure(label, size, true);
    const double left = kChipRect.x + (kChipRect.w - markW - gap - textW) / 2;
    const double cy = kChipRect.y + kChipRect.h / 2;
    if (mark == 1) {
        fillCircle(cr, left + 6, cy, 6, fg);
    } else if (mark == 0) {
        setColor(cr, fg);
        cairo_set_line_width(cr, 2);
        cairo_new_sub_path(cr);
        cairo_arc(cr, left + 6, cy, 5, 0, 2 * M_PI);
        cairo_stroke(cr);
    } else if (mark == 2) {
        drawMicIcon(cr, left, cy - 10, 20, fg, true);
    }
    ui.text(left + markW + gap, frame_ui::centerBaseline(kChipRect.y, kChipRect.h, size), label, size, rgb(fg), true);

    // ミュートする / ミュートを解除（幅 220 で固定。読めていない間は押せない）
    const bool canMute = state.muteKnown;
    const Rect muteHit = frame_ui::drawButton(ui, kMuteRect, "", frame_ui::ButtonKind::Normal, canMute);
    withAlpha(cr, canMute ? 1.0 : frame_ui::kDisabledAlpha, [&] {
        const std::string text = muted ? t.unmute : t.mute;
        const double iconW = muted ? 0 : 20;
        const double iconGap = muted ? 0 : 8;
        const double textSize = ui.fit(text, frame_ui::kControlSize, 13, kMuteRect.w - 28 - iconW - iconGap, true);
        const double w = iconW + iconGap + ui.measure(text, textSize, true);
        const double x = kMuteRect.x + (kMuteRect.w - w) / 2;
        if (!muted) drawMicIcon(cr, x, kMuteRect.y + kMuteRect.h / 2 - 10, 20, kText, true);
        ui.text(x + iconW + iconGap, frame_ui::centerBaseline(kMuteRect.y, kMuteRect.h, textSize), text, textSize,
                rgb(kText), true);
    });
    addButton({PanelAction::MuteToggle}, muteHit);

    // 右: 「アプリと更新」（新しい版があるときはピンクの点）/ アプリと更新の画面では「← マイクの設定へ」
    const bool apps = view_ == PanelView::Apps;
    const Rect appsHit = frame_ui::drawButton(ui, kAppsRect, "", frame_ui::ButtonKind::Normal);
    {
        using frame_updater::UpdateState;
        // ピンクの点: 新しい版がある（入れ終わり・失敗も）か、このアプリで使うアプリが入っていない
        const UpdateState s = model.update.state;
        bool relatedMissing = false;
        for (const frame_apps::Entry& e : model.apps) relatedMissing |= e.related && e.state == frame_apps::AppState::Missing;
        const bool attention = !apps && (s == UpdateState::Available || s == UpdateState::Installed ||
                                         s == UpdateState::InstallFailed || relatedMissing);
        const std::string text = apps ? t.backToSettings : t.appsButton;
        const double iconW = apps ? 18 : 20;
        const double dotW = attention ? 10 + 8 : 0;
        const double textSize = ui.fit(text, frame_ui::kControlSize, 13, kAppsRect.w - 32 - iconW - 8 - dotW, true);
        const double w = iconW + 8 + ui.measure(text, textSize, true) + dotW;
        const double x = kAppsRect.x + (kAppsRect.w - w) / 2;
        const double acy = kAppsRect.y + kAppsRect.h / 2;
        if (apps) {
            drawPolyline(cr, x, acy - 9, 18, 18, kText, 2.4, {11, 3, 5, 9, 11, 15});
        } else {
            drawGridIcon(cr, x, acy - 10, 20, kText);
        }
        const double textEnd = x + iconW + 8 + ui.text(x + iconW + 8, frame_ui::centerBaseline(kAppsRect.y, kAppsRect.h, textSize),
                                                       text, textSize, rgb(kText), true);
        if (attention) fillCircle(cr, textEnd + 8 + 5, acy, 5, kAccent);
    }
    addButton({apps ? PanelAction::ShowSettings : PanelAction::ShowApps}, appsHit);
}

// ============================================================================
// マイクの設定
// ============================================================================

void MicPanel::drawSelectors(const frame_ui::Canvas& ui, const UiText& t, const Config& config, const PanelModel& model) {
    cairo_t* cr = ui.cr;
    const AudioDevices& devices = model.state.devices;
    /**
     * 欄の地・枠・絵の箱・▾ を描き、文字を置ける幅を返す。
     */
    const auto selectBase = [&](Rect r, PanelAction action) {
        const int l = look(r);
        fillRounded(cr, r, 18, l == 2 ? kControlDown : (l == 1 ? kControlHover : kCard));
        strokeRounded(cr, r, 18, kBorder, 2);
        fillRounded(cr, {r.x + 10, r.y + 10, 40, 40}, 11, kIconBox);
        drawPolyline(cr, r.right() - 14 - 18, r.y + r.h / 2 - 9, 18, 18, kText, 2.4, {4, 7, 9, 12, 14, 7});
        addButton({action}, r);
    };
    /**
     * 欄の 2 行（小さな見出しと名前）を描く。
     */
    const auto selectText = [&](Rect r, double right, const std::string& label, const std::string& name) {
        const double x = r.x + 62;
        const double w = right - x;
        ui.text(x, r.y + 22, ui.ellipsize(label, 12, w), 12, rgb(kTextMuted));
        const double size = ui.fit(name, 18, 15, w, true);
        ui.text(x, r.y + 45, ui.ellipsize(name, size, w, true), size, rgb(kText), true);
    };

    // 音の出口（表示している出口。何個つながっても 1 つの欄）
    const std::string activeKey = model.activeOutputKey.empty() ? devices.defaultOutputKey : model.activeOutputKey;
    const std::vector<OutputEntry> entries = outputEntries(config, devices, activeKey, t);
    selectBase(kOutputSelect, PanelAction::OpenOutputPicker);
    drawOutputIcon(cr, isBuiltinSpeaker(viewedKey_), kOutputSelect.x + 10 + 9, kOutputSelect.y + 10 + 9, 22, kText);
    const std::string count = entries.empty() ? std::string() : formatInt(t.countFormat, static_cast<int>(entries.size()));
    const double countRight = kOutputSelect.right() - 14 - 18 - 12;
    const double countW = count.empty() ? 0 : ui.text(countRight, kOutputSelect.y + 36, count, 14, rgb(kTextMuted), false, true);
    std::string viewedName = t.loading;
    for (const OutputEntry& e : entries) {
        if (e.key == viewedKey_) viewedName = e.name;
    }
    selectText(kOutputSelect, countRight - countW - 12, t.outputLabel, viewedName);

    // 使うマイク（短い名前）
    selectBase(kMicSelect, PanelAction::OpenMicPicker);
    drawMicIcon(cr, kMicSelect.x + 10 + 9, kMicSelect.y + 10 + 9, 22, kText, false);
    std::string micName = "—";
    if (!devices.defaultInputKey.empty()) {
        const AudioEndpoint* input = devices.findInput(devices.defaultInputKey);
        micName = isBuiltinMic(devices.defaultInputKey) ? t.builtinMicShort
                                                         : shortDeviceName(input != nullptr ? input->name : devices.defaultInputKey);
    }
    selectText(kMicSelect, kMicSelect.right() - 14 - 18 - 8, t.micLabel, micName);
}

void MicPanel::drawSettings(const frame_ui::Canvas& ui, const UiText& t, const Config& config, const PanelModel& model) {
    const MicState& state = model.state;
    const AudioDevices& devices = state.devices;

    // 設定を表示する出口: 選んだ出口（まだあれば）、無ければ今の出口
    const std::string activeKey = model.activeOutputKey.empty() ? devices.defaultOutputKey : model.activeOutputKey;
    const bool viewKnown = !viewKey_.empty() && (devices.findOutput(viewKey_) != nullptr || config.outputs.count(viewKey_) != 0);
    viewedKey_ = viewKnown ? viewKey_ : activeKey;
    viewingActive_ = viewedKey_ == activeKey;
    const AudioEndpoint* endpoint = devices.findOutput(viewedKey_);
    const auto saved = config.outputs.find(viewedKey_);
    const OutputProfile profile =
        saved != config.outputs.end() ? saved->second : firstProfile(viewedKey_, endpoint != nullptr ? endpoint->name : "");
    const std::string outputName =
        viewedKey_.empty() ? std::string(t.loading)
                           : outputDisplayName(viewedKey_, endpoint != nullptr ? endpoint->name : profile.name, t);
    // 今の出口は今かかっている値、ほかの出口は覚えている値を出す
    bool echo = profile.echo;
    bool ns = profile.ns;
    bool known = true;
    bool nsParamsKnown = true;
    baseVad_ = profile.nsVad;
    baseGrace_ = profile.nsGrace;
    if (viewingActive_) {
        known = state.echoKnown && state.nsKnown;
        echo = state.echo;
        ns = state.ns;
        const NsParams& p = state.nsParams;
        nsParamsKnown = p.nodeKnown && p.vadKnown && p.graceKnown;
        if (nsParamsKnown) {
            baseVad_ = p.vad;
            baseGrace_ = p.grace;
        }
    }

    drawSelectors(ui, t, config, model);
    frame_ui::drawCard(ui, kSettingsCard);
    const bool external = !devices.defaultInputKey.empty() && !isBuiltinMic(devices.defaultInputKey);
    const std::vector<Rect> tabs = frame_ui::drawSegmented(ui, kTabsRect, {t.tabQuick, t.fineTune},
                                                           config.tab == PanelTab::Quick ? 0 : 1, !external);
    addButton({PanelAction::TabQuick}, tabs[0]);
    addButton({PanelAction::TabFine}, tabs[1]);
    if (external) {
        drawExternal(ui, t, model, echo, ns, outputName);
    } else if (config.tab == PanelTab::Quick) {
        drawQuick(ui, t, config, model, echo, ns, known, outputName);
    } else {
        drawFine(ui, t, model, echo, ns, known, nsParamsKnown);
    }
    drawPipeline(ui, t, state, external);
    drawVoice(ui, t, model.voice);
}

void MicPanel::drawQuick(const frame_ui::Canvas& ui, const UiText& t, const Config& /*config*/, const PanelModel& model,
                         bool echo, bool ns, bool known, const std::string& outputName) {
    cairo_t* cr = ui.cr;
    const std::string heading = format1(t.listeningFormat, outputName);
    ui.text(kInnerX, frame_ui::centerBaseline(262, 26, 18), ui.ellipsize(heading, 18, kInnerW, true), 18, rgb(kText), true);

    // プリセット（ユーザーと決めたもの）: イヤホン = エコー除去オフ・ノイズ除去オフ、スピーカー = エコー除去オン・ノイズ除去オフ。
    // 一致はこの 2 つだけで見る（バーの値はノイズ除去がオフなら効かない）
    bool matches[2] = {known && !echo && !ns, known && echo && !ns};
    // プリセットを書き込んでいる間は、押したカードを選択中の見た目で保つ（途中の値で選択が外れない）
    if (viewingActive_ && presetHold_ != PanelAction::None && now_ <= presetHoldUntil_) {
        matches[0] = presetHold_ == PanelAction::Earphone;
        matches[1] = presetHold_ == PanelAction::Speaker;
    }
    for (int i = 0; i < 2; ++i) {
        const Rect r {kInnerX, 296.0 + i * 106, kInnerW, 96};
        const bool selected = matches[i];
        const int l = look(r);
        const Color fill = selected ? (l == 2 ? kAccentPressed : (l == 1 ? kAccentHover : kAccent))
                                    : (l == 2 ? kControlDown : (l == 1 ? kControlHover : kControl));
        fillRounded(cr, r, 16, fill);
        strokeRounded(cr, r, 16, selected ? fill : (l == 2 ? kAccent : kBorder), 2);
        const Color fg = selected ? kOnAccent : kText;
        const double cy = r.y + r.h / 2;
        drawOutputIcon(cr, i == 1, r.x + 20, cy - 22, 44, fg, 3);

        // 右に「押すと何になるか」の札 2 つ（縦に並べる）
        const std::string tags[2] = {i == 0 ? t.chipEchoOff : t.chipEchoOn, t.chipNsOff};
        double tagW = 0;
        for (const std::string& tag : tags) tagW = std::max(tagW, ui.measure(tag, 13, true) + 22);
        const double tagX = r.right() - 16 - tagW;
        for (int c = 0; c < 2; ++c) {
            const Rect tag {tagX, cy - 27 + c * 30, tagW, 24};
            fillRounded(cr, tag, 12, selected ? kOnAccent : kBg);
            if (!selected) strokeRounded(cr, tag, 12, kBorder, 2);
            ui.text(tag.x + 11, frame_ui::centerBaseline(tag.y, tag.h, 13), tags[c], 13,
                    rgb(selected ? kAccentText : kText), true);
        }

        // 真ん中: 名前（選択中は前に ✓ の丸）と、何のためか
        double tx = r.x + 80;
        const double textRight = tagX - 14;
        if (selected) {
            fillCircle(cr, tx + 11, cy - 13, 11, kOnAccent);
            frame_ui::drawCheck(cr, tx + 11, cy - 13, 14, frame_ui::kAccent);
            tx += 30;
        }
        const std::string title = i == 0 ? t.earphoneCard : t.speakerCard;
        const double titleSize = ui.fit(title, 22, 16, textRight - tx, true);
        ui.text(tx, cy - 6, ui.ellipsize(title, titleSize, textRight - tx, true), titleSize, rgb(fg), true);
        const std::string effect = i == 0 ? t.earphoneHint : t.speakerHint;
        const double effectSize = ui.fit(effect, 15, 12, textRight - r.x - 80);
        ui.text(r.x + 80, cy + 23, ui.ellipsize(effect, effectSize, textRight - r.x - 80), effectSize,
                rgb(selected ? kOnAccentSoft : kTextMuted));
        addButton({i == 0 ? PanelAction::Earphone : PanelAction::Speaker}, r);
    }

    // カードの下: 自動で切り替えた直後は知らせ（元に戻す）、ふだんは説明
    if (model.switchNotice && model.switchKey == viewedKey_ && viewingActive_) {
        const Rect box {kInnerX, 502, kInnerW, 56};
        strokeRounded(cr, box, 14, kAccent, 2);
        drawPolyline(cr, box.x + 14, box.y + 18, 20, 24, kAccent, 2.2, {4, 8, 17, 8, 14, 5, NAN, NAN, 20, 16, 7, 16, 10, 19});
        const double buttonW = std::max(96.0, ui.measure(t.undo, frame_ui::kControlSize, true) + 32);
        const Rect undo {box.right() - 6 - buttonW, box.y + 6, buttonW, 44};
        addButton({PanelAction::UndoSwitch}, frame_ui::drawButton(ui, undo, t.undo));
        const double textX = box.x + 14 + 20 + 10;
        const std::vector<std::string> lines =
            wrapText(ui, format1(t.switchedFormat, outputName), 14, false, undo.x - 10 - textX, 2);
        drawLines(ui, textX, box.y + 28 - (static_cast<double>(lines.size()) - 1) * 9.5 + 5, 19, lines, 14, kText);
    } else {
        const std::string note = (matches[0] || matches[1] || !known) ? format1(t.quickNoteFormat, outputName) : t.quickCustom;
        drawLines(ui, kInnerX, 510 + 15, 20, wrapText(ui, note, 14, false, kInnerW, 2), 14, kTextMuted);
    }
}

void MicPanel::drawFine(const frame_ui::Canvas& ui, const UiText& t, const PanelModel& /*model*/, bool echo, bool ns,
                        bool known, bool nsParamsKnown) {
    cairo_t* cr = ui.cr;
    // エコー除去・ノイズ除去のオン / オフ（右に 2 行までの説明）
    for (int row = 0; row < 2; ++row) {
        const double top = 262 + row * 68;
        const bool isEcho = row == 0;
        const std::string title = isEcho ? t.rowEcho : t.rowNs;
        const double size = ui.fit(title, frame_ui::kLabelSize, 15, 196 - kInnerX - 8, true);
        ui.text(kInnerX, frame_ui::centerBaseline(top, 56, size), title, size, rgb(kText), true);
        const bool value = isEcho ? echo : ns;
        const std::vector<Rect> hits =
            frame_ui::drawSegmented(ui, {196, top, 210, 56}, {t.on, t.off}, known ? (value ? 0 : 1) : -1);
        addButton({isEcho ? PanelAction::EchoOn : PanelAction::NsOn}, hits[0]);
        addButton({isEcho ? PanelAction::EchoOff : PanelAction::NsOff}, hits[1]);
        const std::vector<std::string> lines = wrapText(ui, isEcho ? t.echoHint : t.nsHint, 14, false, kInnerRight - 420, 2);
        drawLines(ui, 420, top + 28 - (static_cast<double>(lines.size()) - 1) * 10 + 5, 20, lines, 14, kTextMuted);
    }

    // ノイズ除去の強さのバー 2 本。ノイズ除去がオフの間は効かないので、塗りとつまみを灰色にして説明を替える（押せるまま）
    double vad = 0.0;
    double grace = 0.0;
    displayedNsValues(vad, grace);
    const double trackX0 = kInnerX + 132 + 14 + 12;
    const double trackX1 = kInnerRight - 70 - 14 - 12;
    for (int row = 0; row < 2; ++row) {
        const bool isVad = row == 0;
        const double top = 404 + row * 54;
        const double cy = top + 24;
        const PanelAction action = isVad ? PanelAction::NsVadSlider : PanelAction::NsGraceSlider;
        const double min = isVad ? kNsVadMin : kNsGraceMin;
        const double max = isVad ? kNsVadMax : kNsGraceMax;
        const double value = isVad ? vad : grace;
        const std::string title = isVad ? t.nsVad : t.nsGrace;
        const double titleSize = ui.fit(title, 17, 13, 130, true);
        ui.text(kInnerX, top + 21, title, titleSize, rgb(kText), true);
        const std::string hint = (!ns && isVad) ? t.nsInactive : (isVad ? t.nsVadHint : t.nsGraceHint);
        const double hintSize = ui.fit(hint, 12, 10, 130);
        ui.text(kInnerX, top + 40, ui.ellipsize(hint, hintSize, 130), hintSize, rgb(kTextMuted));

        // 溝・今の値までの塗り・つまみ
        fillRounded(cr, {trackX0, cy - 3, trackX1 - trackX0, 6}, 3, kDivider);
        const double fraction = nsParamsKnown ? std::clamp((value - min) / (max - min), 0.0, 1.0) : 0.0;
        const double thumbX = trackX0 + (trackX1 - trackX0) * fraction;
        const Color fill = ns ? kAccent : kTextMuted;
        if (nsParamsKnown && thumbX > trackX0 + 1) fillRounded(cr, {trackX0, cy - 3, thumbX - trackX0, 6}, 3, fill);
        if (nsParamsKnown) {
            const bool dragged = dragAction_ == action;
            const int l = look({trackX0 - 12, top, trackX1 - trackX0 + 24, 48});
            const double r = dragged ? 14 : (l > 0 ? 13 : 12);
            if (dragged && ns) {
                setColor(cr, kAccent, 0.3);
                cairo_new_sub_path(cr);
                cairo_arc(cr, thumbX, cy, r + 6, 0, 2 * M_PI);
                cairo_fill(cr);
            }
            fillCircle(cr, thumbX, cy, r + 2, kCard);  // 塗りと見分けるための縁
            fillCircle(cr, thumbX, cy, r, ns ? kText : kTextMuted);
        }
        tracks_.push_back({action, trackX0, trackX1, min, max, cy});
        if (nsParamsKnown) addButton({action}, {trackX0 - 12, top, trackX1 - trackX0 + 24, 48});

        char text[32];
        if (!nsParamsKnown) {
            std::snprintf(text, sizeof(text), "--");
        } else if (isVad) {
            std::snprintf(text, sizeof(text), "%.0f%%", value);
        } else {
            std::snprintf(text, sizeof(text), "%.0fms", value);
        }
        ui.text(kInnerRight, frame_ui::centerBaseline(top, 48, 20), text, 20, rgb(ns ? kText : kTextMuted), true, true);
    }

    // 標準に戻す（SteamOS の既定 23% / 500ms）と、標準の値の説明
    const Rect reset {424, 514, 158, 44};
    const double noteSize = ui.fit(t.nsDefaultNote, 14, 11, reset.x - kInnerX - 12);
    ui.text(kInnerX, frame_ui::centerBaseline(reset.y, reset.h, noteSize), t.nsDefaultNote, noteSize, rgb(kTextMuted));
    addButton({PanelAction::NsReset}, frame_ui::drawButton(ui, reset, t.nsReset, frame_ui::ButtonKind::Normal, nsParamsKnown));
}

void MicPanel::drawExternal(const frame_ui::Canvas& ui, const UiText& t, const PanelModel& model, bool echo, bool ns,
                            const std::string& outputName) {
    const AudioDevices& devices = model.state.devices;
    const AudioEndpoint* input = devices.findInput(devices.defaultInputKey);
    const std::string micName = format1(t.micOfFormat, input != nullptr ? input->name : devices.defaultInputKey);
    const std::string now = format1(t.externalNowFormat, micName);
    const double size = ui.fit(now, 21, 16, kInnerW, true);
    ui.text(kInnerX, 268 + 23, ui.ellipsize(now, size, kInnerW, true), size, rgb(kText), true);
    drawLines(ui, kInnerX, 306 + 19, 26, wrapText(ui, t.externalExplain, 16, false, kInnerW, 3), 16, kTextSoft);
    drawLines(ui, kInnerX, 392 + 15, 21,
              wrapText(ui, format2(t.externalSavedFormat, outputName, summaryText(echo, ns, t)), 14, false, kInnerW, 2), 14,
              kTextMuted);
    // 内蔵マイクに戻す（内蔵マイクが一覧にあるときだけ押せる）
    bool builtinAvailable = false;
    for (const AudioEndpoint& e : devices.inputs) builtinAvailable |= isBuiltinMic(e.key);
    const Rect button {kInnerX, 452, std::max(300.0, ui.measure(t.useBuiltinMic, frame_ui::kControlSize, true) + 48), 56};
    addButton({PanelAction::UseBuiltinMic},
              frame_ui::drawButton(ui, button, t.useBuiltinMic, frame_ui::ButtonKind::Normal, builtinAvailable));
}

void MicPanel::drawPipeline(const frame_ui::Canvas& ui, const UiText& t, const MicState& state, bool external) {
    cairo_t* cr = ui.cr;
    const Rect box = kPipelineRect;
    fillRounded(cr, box, 14, kInset);
    const double labelBaseline = box.y + 8 + 13;
    const double labelW = ui.text(box.x + 14, labelBaseline, t.rowChain, 13, rgb(kTextMuted), true);
    std::string note;
    if (!state.loaded) {
        note = t.loading;
    } else if (!state.linksKnown) {
        note = t.chainUnknown;
    } else if (!state.inUse && !external) {
        note = t.chainIdle;
    }
    if (!note.empty()) {
        const double room = box.w - 28 - labelW - 16;
        const double size = ui.fit(note, 13, 11, room);
        ui.text(box.right() - 14, labelBaseline, ui.ellipsize(note, size, room), size, rgb(kTextMuted), false, true);
    }

    // 5 つの段: マイク ─ 音質補正 ─ エコー除去 ─ ノイズ除去 ─ アプリへ。通っている段だけピンク
    std::string micLabel = t.stageMic;
    if (external) {
        const AudioEndpoint* input = state.devices.findInput(state.devices.defaultInputKey);
        micLabel = shortDeviceName(input != nullptr ? input->name : state.devices.defaultInputKey);
    }
    const std::string labels[5] = {micLabel, t.stageEq, t.stageEcho, t.stageNs, t.stageOut};
    bool active[5] = {external || state.linksKnown, false, false, false, external || state.linksKnown};
    if (!external) {
        for (const ChainStage& stage : state.chain) {
            if (stage.kind == ChainStage::Kind::Eq) active[1] = true;
            if (stage.kind == ChainStage::Kind::EchoCancel) active[2] = true;
            if (stage.kind == ChainStage::Kind::NoiseSuppression) active[3] = true;
        }
    }
    const double inner = box.w - 28;
    const double minWire = 6;
    double size = 15;
    double widths[5];
    double total = 0;
    for (; size >= 11; size -= 1) {
        total = 0;
        for (int i = 0; i < 5; ++i) {
            widths[i] = ui.measure(labels[i], size, active[i]) + 24;
            total += widths[i];
        }
        if (total + 4 * minWire <= inner) break;
    }
    const double wire = std::max(minWire, (inner - total) / 4);
    const double nodeH = 36;
    const double top = box.y + 8 + 19 + 6;
    const double cy = top + nodeH / 2;
    double x = box.x + 14;
    for (int i = 0; i < 5; ++i) {
        const Rect node {x, top, widths[i], nodeH};
        if (active[i]) {
            fillRounded(cr, node, nodeH / 2, kAccentTint);
            strokeRounded(cr, node, nodeH / 2, kAccent, 2);
        } else {
            strokeRounded(cr, node, nodeH / 2, kBorder, 2, true);
        }
        ui.text(node.x + 12, frame_ui::centerBaseline(node.y, node.h, size), labels[i], size,
                rgb(active[i] ? kText : kTextMuted), active[i]);
        x += widths[i];
        if (i == 4) break;
        // 段と段の線: 両側が通っていればピンクの実線、ほかは点線
        const bool on = active[i] && active[i + 1];
        setColor(cr, on ? kAccent : kBorder);
        cairo_set_line_width(cr, 3);
        if (!on) {
            const double dash[] = {6, 5};
            cairo_set_dash(cr, dash, 2, 0);
        }
        cairo_move_to(cr, x, cy);
        cairo_line_to(cr, x + wire, cy);
        cairo_stroke(cr);
        cairo_set_dash(cr, nullptr, 0, 0);
        x += wire;
    }
}

void MicPanel::drawVoice(const frame_ui::Canvas& ui, const UiText& t, const VoiceView& voice) {
    cairo_t* cr = ui.cr;
    const Rect card = kVoiceCard;
    frame_ui::drawCard(ui, card);
    const double x = card.x + frame_ui::kCardPadX;
    const double right = card.right() - frame_ui::kCardPadX;
    const Rect record {right - 150, 110, 150, 56};
    ui.text(x, frame_ui::centerBaseline(110, 32, frame_ui::kCardTitleSize), t.voiceTitle,
            ui.fit(t.voiceTitle, frame_ui::kCardTitleSize, 16, record.x - x - 12, true), rgb(kText), true);
    const double hintSize = ui.fit(t.voiceHint, 14, 11, record.x - x - 12);
    ui.text(x, 142 + 15, ui.ellipsize(t.voiceHint, hintSize, record.x - x - 12), hintSize, rgb(kTextMuted));

    // 録音のボタン: 待機中は赤い枠に「● 録音」、録音中は赤い塗りに「■ 停止」
    const Rect recordHit = frame_ui::drawButton(ui, record, "",
                                                voice.recording ? frame_ui::ButtonKind::DangerArmed : frame_ui::ButtonKind::Danger);
    {
        const std::string label = voice.recording ? t.stop : t.record;
        const double size = ui.fit(label, 21, 14, record.w - 60, true);
        const double w = 16 + 8 + ui.measure(label, size, true);
        const double bx = record.x + (record.w - w) / 2;
        const double cy = record.y + record.h / 2;
        if (voice.recording) {
            drawStopIcon(cr, bx + 8, cy, 14, kOnAccent);
        } else {
            fillCircle(cr, bx + 8, cy, 8, kDanger);
        }
        ui.text(bx + 24, frame_ui::centerBaseline(record.y, record.h, size), label, size,
                rgb(voice.recording ? kOnAccent : kText), true);
    }
    addButton({PanelAction::Record}, recordHit);

    // 録音のボタンの下の行: 録音中は「● 録音中」・経過と残り・ピークのメーター、失敗は赤、ほかは説明
    const double lineTop = 180;
    const double lineH = 28;
    const double lineBaseline = frame_ui::centerBaseline(lineTop, lineH, 14);
    if (voice.recording) {
        fillCircle(cr, x + 6, lineTop + lineH / 2, 6, kDanger);
        double cursor = x + 18 + ui.text(x + 18, lineBaseline, t.recording, 15, rgb(kDangerText), true);
        char elapsed[64];
        std::snprintf(elapsed, sizeof(elapsed), t.elapsedFormat, voice.recordSec,
                      std::max(0.0, kVoiceMaxSec - voice.recordSec));
        cursor += 12;
        cursor += ui.text(cursor, lineBaseline, elapsed, 14, rgb(kText));
        char level[32];
        std::snprintf(level, sizeof(level), "%.1f dBFS", std::max(-60.0f, voice.levelDb));
        const double levelW = ui.measure("-60.0 dBFS", 14);
        ui.text(right, lineBaseline, level, 14, rgb(kText), false, true);
        const double barX = cursor + 14;
        const double barW = right - levelW - 12 - barX;
        if (barW > 30) {
            const Rect bar {barX, lineTop + lineH / 2 - 7, barW, 14};
            fillRounded(cr, bar, 7, kBg);
            const double fraction = std::clamp((voice.levelDb + 60.0) / 60.0, 0.0, 1.0);
            if (fraction > 0) fillRounded(cr, {bar.x, bar.y, std::max(bar.h, bar.w * fraction), bar.h}, 7, kAccent);
            strokeRounded(cr, bar, 7, kBorder, 1.5);
        }
    } else if (voice.error != VoiceError::None) {
        const std::string message = voice.error == VoiceError::Record ? t.errRecord : t.errPlay;
        const double size = ui.fit(message, 15, 11, right - x, true);
        ui.text(x, lineBaseline, ui.ellipsize(message, size, right - x, true), size, rgb(kDangerText), true);
    } else {
        const double size = ui.fit(t.voiceIdle, 14, 11, right - x);
        ui.text(x, lineBaseline, ui.ellipsize(t.voiceIdle, size, right - x), size, rgb(kTextMuted));
    }

    // 履歴（新しい順に 5 件まで）
    fillRounded(cr, {x, 216, right - x, 1}, 0, kDivider);
    if (voice.clips.empty()) ui.text(x, 226 + 40, t.noClips, 16, rgb(kTextMuted));
    for (size_t i = 0; i < voice.clips.size() && i < kVoiceHistory; ++i) {
        const VoiceClip& clip = *voice.clips[i];
        const double top = 226 + i * 84.0;
        const double cy = top + 36;
        const bool playing = voice.playing && voice.playingId == clip.id;
        const int index = static_cast<int>(i);
        // ▶ / ■ のボタン（丸）
        const Rect play {x, cy - 26, 52, 52};
        const int l = look(play);
        if (playing) {
            fillCircle(cr, play.x + 26, cy, 26, l == 2 ? kAccentPressed : (l == 1 ? kAccentHover : kAccent));
            drawStopIcon(cr, play.x + 26, cy, 14, kOnAccent);
        } else {
            if (l > 0) fillCircle(cr, play.x + 26, cy, 26, l == 2 ? kControlDown : kControlHover);
            setColor(cr, kBorder);
            cairo_set_line_width(cr, 2);
            cairo_new_sub_path(cr);
            cairo_arc(cr, play.x + 26, cy, 25, 0, 2 * M_PI);
            cairo_stroke(cr);
            drawPlayIcon(cr, play.x + 27, cy, 16, kText);
        }
        addButton({PanelAction::Play, index}, play);

        // 時刻・長さ（1 行目）と、録ったときの出口と設定（2 行目。例:「AB13X・エコー除去オフ」）
        const double tx = play.right() + 14;
        const double textW = 176;
        const double timeW = ui.text(tx, top + 31, clockText(clip.recordedAt), 18, rgb(kText), true);
        char length[32];
        std::snprintf(length, sizeof(length), "%.1f %s", clip.seconds(), t.seconds);
        ui.text(tx + timeW + 8, top + 31, length, 13, rgb(kTextMuted));
        std::string label;
        if (clip.externalMic) {
            label = format1(t.clipExternalFormat, shortDeviceName(clip.micName));
        } else if (!clip.echoKnown) {
            label = t.unknownSetting;
        } else {
            label = summaryText(clip.echo, clip.nsKnown && clip.ns, t);
            if (clip.nsKnown && clip.ns && clip.nsParamsKnown) {
                char numbers[32];
                std::snprintf(numbers, sizeof(numbers), " %.0f%%/%.0fms", clip.nsVad, clip.nsGrace);
                label += numbers;
            }
            if (clip.outputKnown) {
                const std::string output = clip.outputSpeaker ? std::string(t.speaker) : shortDeviceName(clip.outputName);
                if (!output.empty()) label = output + t.labelSeparator + label;
            }
        }
        const double labelSize = ui.fit(label, 13, 11, textW);
        ui.text(tx, top + 54, ui.ellipsize(label, labelSize, textW), labelSize, rgb(kTextSoft));

        // 波形（3px の棒を 2px おき）。再生中は再生済みの部分をピンクに
        const double waveX = tx + textW + 14;
        const double waveW = right - waveX;
        const int bars = std::max(1, static_cast<int>((waveW + 2) / 5));
        const double playedX = playing ? waveX + waveW * std::clamp(voice.playSec / std::max(0.01, clip.seconds()), 0.0, 1.0)
                                       : waveX;
        const int bins = static_cast<int>(clip.wave.size());
        for (int b = 0; b < bars && bins > 0; ++b) {
            float peak = 0.0f;
            for (int k = b * bins / bars; k < std::max(b * bins / bars + 1, (b + 1) * bins / bars) && k < bins; ++k) {
                peak = std::max(peak, clip.wave[k]);
            }
            const double bx = waveX + b * 5;
            const double amplitude = std::sqrt(std::clamp(static_cast<double>(peak), 0.0, 1.0));  // 小さい音も見えるように
            const double bh = std::max(6.0, amplitude * 44);
            fillRounded(cr, {bx, cy - bh / 2, 3, bh}, 1.5, playing && bx < playedX ? kAccent : kTextMuted);
        }
    }
}

// ============================================================================
// アプリと更新
// ============================================================================

void MicPanel::drawUpdateBox(const frame_ui::Canvas& ui, const UiText& t, frame_ui::Lang lang, const Config& config,
                             const frame_updater::UpdateStatus& update, Rect box) {
    using frame_updater::UpdateState;
    cairo_t* cr = ui.cr;
    const frame_ui::Strings& s = frame_ui::strings(lang);
    const auto fill = [](const char* format, const std::string& arg) { return format1(format, arg); };

    /** 箱の右に並べるボタン 1 つ。 */
    struct BoxButton {
        PanelAction action;
        std::string label;
        frame_ui::ButtonKind kind;
        bool enabled;
    };
    std::string line;
    std::string hint = config.updateCheck ? t.updateAutoHint : t.updateOffHint;
    Color lineColor = kTextSoft;
    bool lineBold = false;
    Color edge = kInset;
    bool hasEdge = false;
    std::vector<BoxButton> buttons;
    const BoxButton checkNow {PanelAction::UpdateCheckNow, s.updateCheckNow, frame_ui::ButtonKind::Normal, !update.checking};
    switch (update.state) {
        case UpdateState::Installing:
            line = fill(s.updateInstallingFormat, frame_ui::updateStepText(lang, update.step) + "…");
            hint = t.updateConfirmHint;  // 確認のときと同じ補足（終わったら起動し直す）
            break;
        case UpdateState::Installed:
            line = fill(t.updateInstalledFormat, update.version);
            lineColor = kText;
            lineBold = true;
            edge = kAccent;
            hasEdge = true;
            hint = t.updateInstalledHint;
            buttons.push_back({PanelAction::UpdateDismiss, s.updateDismiss, frame_ui::ButtonKind::Normal, true});
            break;
        case UpdateState::InstallFailed:
            line = std::string(s.updateInstallFailed) + " " + frame_ui::updateErrorText(lang, update.error);
            lineColor = kDangerText;
            lineBold = true;
            edge = kDanger;
            hasEdge = true;
            hint.clear();
            buttons.push_back({PanelAction::UpdateRetry, s.updateRetry, frame_ui::ButtonKind::Normal, true});
            buttons.push_back({PanelAction::UpdateDismiss, s.updateDismiss, frame_ui::ButtonKind::Normal, true});
            break;
        case UpdateState::Available:
            edge = kAccent;
            hasEdge = true;
            lineBold = true;
            if (update.installable && updateArmed_) {
                line = fill(s.updateConfirmFormat, update.latest);
                lineColor = kText;
                hint = t.updateConfirmHint;
                buttons.push_back({PanelAction::UpdateConfirmNo, s.updateConfirmNo, frame_ui::ButtonKind::Normal, true});
                buttons.push_back({PanelAction::UpdateConfirmYes, s.updateConfirmYes, frame_ui::ButtonKind::Primary, true});
            } else if (update.installable) {
                // 箱の幅に合わせて、ボタンは「更新する」だけ（新しい版はもう分かっているので「今すぐ確かめる」は出さない）
                line = fill(s.updateAvailableFormat, update.latest);
                lineColor = kAccentText;
                hint.clear();
                buttons.push_back({PanelAction::UpdateInstall, s.updateButton, frame_ui::ButtonKind::Primary, true});
            } else {
                line = fill(s.updateAvailableFormat, update.latest);
                lineColor = kAccentText;
                hint = s.updateManual;
                if (!update.url.empty()) hint += std::string("  ") + t.updateReleasePage + update.url;
                buttons.push_back(checkNow);
            }
            break;
        case UpdateState::UpToDate:
            line = update.checking ? s.updateChecking : fill(s.updateUpToDateFormat, update.current);
            buttons.push_back(checkNow);
            break;
        case UpdateState::CheckFailed:
            if (update.checking) {
                line = s.updateChecking;
            } else {
                line = std::string(s.updateCheckFailed) + " " + frame_ui::updateErrorText(lang, update.error);
                lineColor = kDangerText;
            }
            buttons.push_back(checkNow);
            break;
        case UpdateState::Unknown:
            line = update.checking ? s.updateChecking : fill(s.updateCurrentFormat, update.current);
            buttons.push_back(checkNow);
            break;
    }

    fillRounded(cr, box, 14, kInset);
    if (hasEdge) strokeRounded(cr, box, 14, edge, 2);
    // ボタンは右から（右の余白 18、間 10、縦は真ん中）
    double left = box.right() - 18;
    for (auto it = buttons.rbegin(); it != buttons.rend(); ++it) {
        const double w = std::max(110.0, ui.measure(it->label, frame_ui::kControlSize, true) + 40);
        left -= w;
        const Rect hit = frame_ui::drawButton(ui, {left, box.y + (box.h - 56) / 2, w, 56}, it->label, it->kind, it->enabled);
        addButton({it->action}, hit);
        left -= 10;
    }
    const double textX = box.x + 20;
    const double room = (buttons.empty() ? box.right() - 20 : left - 6) - textX;
    // 文は 2 行まで（補足が無ければ 4 行まで）折り返し、補足は箱の高さに入るだけ（3 行まで）出す
    const double lineSize = ui.fit(line, frame_ui::kControlSize, 15, room, lineBold);
    const double rowH = lineSize + 7;
    const double hintH = 19;
    const std::vector<std::string> lineRows = wrapText(ui, line, lineSize, lineBold, room, hint.empty() ? 4 : 2);
    const int hintRoom = static_cast<int>((box.h - 16 - lineRows.size() * rowH - 6) / hintH);
    const std::vector<std::string> hintRows =
        hint.empty() || hintRoom < 1 ? std::vector<std::string>() : wrapText(ui, hint, 14, false, room, std::min(3, hintRoom));
    const double blockH = lineRows.size() * rowH + (hintRows.empty() ? 0 : 6 + hintRows.size() * hintH);
    double y = box.y + (box.h - blockH) / 2;
    for (const std::string& row : lineRows) {
        ui.text(textX, y + lineSize, row, lineSize, rgb(lineColor), lineBold);
        y += rowH;
    }
    if (!hintRows.empty()) drawLines(ui, textX, y + 6 + 14, hintH, hintRows, 14, kTextMuted);
}

void MicPanel::drawApps(const frame_ui::Canvas& ui, const UiText& t, const Config& config, const PanelModel& model) {
    cairo_t* cr = ui.cr;
    const frame_ui::Lang lang = uiLang(config.language);
    frame_ui::drawCard(ui, kAppCard);
    const double x = kAppCard.x + frame_ui::kCardPadX;
    const double right = kAppCard.right() - frame_ui::kCardPadX;
    ui.text(x, frame_ui::centerBaseline(110, 32, frame_ui::kCardTitleSize), t.thisApp, frame_ui::kCardTitleSize, rgb(kText),
            true);

    // アイコン（サムネイルと同じ絵）・名前・版・説明
    cairo_save(cr);
    cairo_translate(cr, x, 156);
    cairo_scale(cr, 64.0 / 256, 64.0 / 256);
    cairo_set_source_surface(cr, appIcon_, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
    cairo_paint(cr);
    cairo_restore(cr);
    const double nameW = ui.text(132, 160 + 24, "Frame Mic Tuner", 22, rgb(kText), true);
    ui.text(132 + nameW + 8, 160 + 24, model.update.current, 16, rgb(kTextMuted));
    const double descSize = ui.fit(t.thisAppDesc, 14, 11, right - 132);
    ui.text(132, 194 + 15, ui.ellipsize(t.thisAppDesc, descSize, right - 132), descSize, rgb(kTextMuted));

    drawUpdateBox(ui, t, lang, config, model.update, kUpdateBox);

    // 新しい版の確認（オン / オフ。オフならアプリの一覧も取りに行かない）
    const double labelSize = ui.fit(t.rowUpdateCheck, 18, 13, 216 - x - 10, true);
    ui.text(x, frame_ui::centerBaseline(380, 56, labelSize), t.rowUpdateCheck, labelSize, rgb(kText), true);
    const std::vector<Rect> toggle = frame_ui::drawSegmented(ui, {216, 380, 180, 56}, {t.on, t.off}, config.updateCheck ? 0 : 1);
    addButton({PanelAction::UpdateCheckOn}, toggle[0]);
    addButton({PanelAction::UpdateCheckOff}, toggle[1]);
    drawLines(ui, x, 446 + 15, 21, wrapText(ui, t.updateCheckNote, 14, false, right - x, 2), 14, kTextMuted);

    fillRounded(cr, {x, 508, right - x, 1}, 0, kDivider);
    ui.text(x, 526 + 15, t.helpTitle, 14, rgb(kTextMuted));
    ui.text(x, 548 + 15, "github.com/sasaken1102r/frame-mic-tuner", 14, rgb(kTextSoft));

    drawRelatedApps(ui, t, config, model);
}

void MicPanel::drawAppRow(const frame_ui::Canvas& ui, const UiText& t, Language language, const frame_apps::Entry& entry,
                          Rect row, bool inList) {
    using frame_apps::AppState;
    cairo_t* cr = ui.cr;
    fillRounded(cr, row, 16, kInset);
    const double cy = row.y + row.h / 2;
    // 左: アイコン（取ってきたもの → 同梱のもの。どちらも無ければ札の文字）
    const double tile = 52;
    const double tx = row.x + 16;
    if (!frame_apps::drawIcon(cr, entry, tx, cy - tile / 2, tile)) {
        fillRounded(cr, {tx, cy - tile / 2, tile, tile}, 13, kIconBox);
        const double monoSize = ui.fit(entry.app.mono, 16, 11, tile - 8, true);
        const double w = ui.measure(entry.app.mono, monoSize, true);
        ui.text(tx + (tile - w) / 2, frame_ui::centerBaseline(cy - tile / 2, tile, monoSize), entry.app.mono, monoSize,
                rgb(kAccent), true);
    }
    // 右: 「入れる」か状態の札
    double rightEdge = row.right() - 16;
    if (entry.state == AppState::Missing && !entry.busy) {
        const double w = std::max(110.0, ui.measure(t.appsInstall, frame_ui::kControlSize, true) + 40);
        const Rect button {rightEdge - w, cy - frame_ui::kControlH / 2, w, frame_ui::kControlH};
        const Rect hit = frame_ui::drawButton(ui, button, t.appsInstall, frame_ui::ButtonKind::Primary);
        addButton({PanelAction::AppInstall, 0, entry.app.key}, inList ? intersect(hit, listRect_) : hit, inList);
        rightEdge = button.x - 16;
    } else if (entry.state != AppState::Unknown || entry.busy) {
        const char* label = entry.busy ? t.appsChipBusy : (entry.state == AppState::Running ? t.appsChipRunning : t.appsChipInstalled);
        const bool ok = !entry.busy && entry.state == AppState::Running;
        const double size = entry.busy ? 15 : 16;
        const double dot = ok ? 12 + 8 : 0;
        const double w = 18 + dot + ui.measure(label, size, true) + 18;
        const Rect chip {rightEdge - w, cy - 22, w, 44};
        fillRounded(cr, chip, 22, ok ? kSuccessTint : kIdleFill);
        if (ok) fillCircle(cr, chip.x + 18 + 6, cy, 6, kSuccess);
        ui.text(chip.x + 18 + dot, frame_ui::centerBaseline(chip.y, chip.h, size), label, size,
                rgb(ok ? kSuccess : kIdleText), true);
        rightEdge = chip.x - 16;
    }
    // 真ん中: 名前（一覧では「このアプリで使う」の札も）と、説明か理由（簡体字中国語の説明は英語）
    const double textX = tx + tile + 16;
    const double textW = std::max(40.0, rightEdge - textX);
    const double nameSize = 19;
    std::string tag;
    double tagW = 0;
    if (inList && entry.related) {
        tag = t.appsRelatedTag;
        tagW = ui.measure(tag, 13, true) + 20;
    }
    const double nameW = ui.measure(entry.app.name, nameSize, true);
    const bool tagFits = !tag.empty() && nameW + 10 + tagW <= textW;
    const std::string name = ui.ellipsize(entry.app.name, nameSize, tagFits ? textW - 10 - tagW : textW, true);
    const double drawnW = ui.text(textX, cy - 4, name, nameSize, rgb(kText), true);
    if (tagFits) {
        const Rect tagRect {textX + drawnW + 10, cy - 4 - 17, tagW, 24};
        fillRounded(cr, tagRect, 12, kAccentTint);
        ui.text(tagRect.x + 10, frame_ui::centerBaseline(tagRect.y, tagRect.h, 13), tag, 13, rgb(kAccentText), true);
    }
    const frame_apps::Lang appsLang = language == Language::Ja ? frame_apps::Lang::Ja : frame_apps::Lang::En;
    std::string sub = entry.app.desc(appsLang);
    if (!inList && entry.related) sub = entry.app.name == "frame-aux-shortcuts" ? std::string(t.appsReasonAux) : entry.reason(appsLang);
    const double subSize = ui.fit(sub, 14, 11, textW);
    ui.text(textX, cy + 20, ui.ellipsize(sub, subSize, textW), subSize, rgb(kTextMuted));
}

void MicPanel::drawRelatedApps(const frame_ui::Canvas& ui, const UiText& t, const Config& config, const PanelModel& model) {
    cairo_t* cr = ui.cr;
    frame_ui::drawCard(ui, kRelatedCard);
    const double x = kRelatedCard.x + frame_ui::kCardPadX;
    const double right = kRelatedCard.right() - frame_ui::kCardPadX;
    const double w = right - x;
    ui.text(x, frame_ui::centerBaseline(110, 32, frame_ui::kCardTitleSize), t.appsRelatedTitle,
            ui.fit(t.appsRelatedTitle, frame_ui::kCardTitleSize, 16, w, true), rgb(kText), true);
    const double hintSize = ui.fit(t.appsRelatedHint, 14, 11, w);
    ui.text(x, 142 + 15, ui.ellipsize(t.appsRelatedHint, hintSize, w), hintSize, rgb(kTextMuted));

    // このアプリで使うものだけ（理由付き）
    double y = 178;
    for (const frame_apps::Entry& entry : model.apps) {
        if (!entry.related || y + 96 > 560) continue;
        drawAppRow(ui, t, config.language, entry, {x, y, w, 96}, false);
        y += 96 + 16;
    }

    // 「ささけんの Frame アプリ　N 個のうち M 個が入っていません ›」（押すと一覧）
    const Rect open {x, y, w, 88};
    const int l = look(open);
    fillRounded(cr, open, 16, l == 2 ? kControlDown : (l == 1 ? kControlHover : kControl));
    strokeRounded(cr, open, 16, kBorder, 2);
    double iconX = open.x + 18;
    int shown = 0;
    for (const frame_apps::Entry& entry : model.apps) {
        if (shown == 4) break;
        // 重ねたアイコン（下のボタンの色の縁で区切る）
        fillRounded(cr, {iconX - 2, open.y + 26 - 2, 40, 40}, 11, kControl);
        if (!frame_apps::drawIcon(cr, entry, iconX, open.y + 26, 36)) {
            fillRounded(cr, {iconX, open.y + 26, 36, 36}, 9, kIconBox);
        }
        iconX += 24;
        ++shown;
    }
    const double textX = open.x + 18 + (shown > 0 ? 24 * (shown - 1) + 36 : 0) + 16;
    int missing = 0;
    for (const frame_apps::Entry& entry : model.apps) missing += entry.state == frame_apps::AppState::Missing ? 1 : 0;
    char count[200];
    if (missing > 0) {
        std::snprintf(count, sizeof(count), t.appsAllCountFormat, static_cast<int>(model.apps.size()), missing);
    } else {
        std::snprintf(count, sizeof(count), t.appsAllInstalledFormat, static_cast<int>(model.apps.size()));
    }
    const double textW = open.right() - 18 - 20 - 12 - textX;
    const double titleSize = ui.fit(t.appsAllTitle, 19, 15, textW, true);
    ui.text(textX, open.y + 40, ui.ellipsize(t.appsAllTitle, titleSize, textW, true), titleSize, rgb(kText), true);
    ui.text(textX, open.y + 64, ui.ellipsize(count, 14, textW), 14, rgb(kTextMuted));
    drawPolyline(cr, open.right() - 18 - 20, open.y + open.h / 2 - 10, 20, 20, kText, 2.4, {7, 4, 13, 10, 7, 16});
    addButton({PanelAction::AppsOpenList}, open);

    drawLines(ui, x, open.bottom() + 18 + 15, 22, wrapText(ui, t.appsInstallNote, 14, false, w, 3), 14, kTextMuted);
}

void MicPanel::drawAppsList(const frame_ui::Canvas& ui, const UiText& t, const Config& config, const PanelModel& model) {
    cairo_t* cr = ui.cr;
    drawModal(ui, kAppsModal, t.appsAllTitle, t.appsListSub);
    const double contentH = model.apps.empty() ? 0 : model.apps.size() * (kAppRowH + kAppRowGap) - kAppRowGap;
    beginList(kAppsList, contentH);
    cairo_save(cr);
    cairo_rectangle(cr, kAppsList.x - 2, kAppsList.y, kAppsList.w + 4, kAppsList.h);
    cairo_clip(cr);
    double y = kAppsList.y - listScroll_;
    const double rowW = listMaxScroll_ > 0 ? kAppsList.w - 16 : kAppsList.w;
    for (const frame_apps::Entry& entry : model.apps) {
        if (y + kAppRowH >= kAppsList.y && y <= kAppsList.bottom()) {
            drawAppRow(ui, t, config.language, entry, {kAppsList.x, y, rowW, kAppRowH}, true);
        }
        y += kAppRowH + kAppRowGap;
    }
    cairo_restore(cr);
    endList(ui, contentH, kAppsList.right() - 6);

    // 下: 補足と「インストーラーを開く」（メニューの Konsole がまだ開いていれば押せない）
    fillRounded(cr, {kAppsList.x, 556, kAppsList.w, 1}, 0, kDivider);
    const Rect menu {884, 648, 232, 56};
    drawLines(ui, kAppsList.x, 574 + 14, 20, wrapText(ui, t.appsListNote, 13, false, 760, 3), 13, kTextMuted);
    const std::string label = model.menuBusy ? t.appsChipBusy : t.appsOpenInstaller;
    addButton({PanelAction::AppsOpenMenu}, frame_ui::drawButton(ui, menu, label, frame_ui::ButtonKind::Normal, !model.menuBusy));
}

void MicPanel::drawAppsConfirm(const frame_ui::Canvas& ui, const UiText& t, const Config& /*config*/,
                               const PanelModel& model) {
    using frame_apps::LaunchResult;
    cairo_t* cr = ui.cr;
    setColor(cr, kBackdrop, kBackdropAlpha);
    frame_ui::roundedRect(cr, {0, 0, static_cast<double>(kWidth), static_cast<double>(kHeight)}, 24);
    cairo_fill(cr);
    const Rect card = kConfirmCard;
    fillRounded(cr, card, 18, kCard);
    strokeRounded(cr, card, 18, kBorder, 2);
    const double x = card.x + 30;
    const double w = card.w - 60;

    // アプリ名とコマンドは、今の一覧（取ってきたものかもしれない）から呼び名で引く。Konsole に渡すのと同じ関数で作る
    const frame_apps::Entry* entry = nullptr;
    for (const frame_apps::Entry& e : model.apps) {
        if (!confirmKey_.empty() && e.app.key == confirmKey_) entry = &e;
    }
    const bool unknownKey = !confirmKey_.empty() && entry == nullptr;  // 一覧が変わって消えた: 開かせない
    double y = card.y + 28;
    double titleX = x;
    if (entry != nullptr) {
        frame_apps::drawIcon(cr, *entry, x, y, 44);
        titleX = x + 44 + 14;
    }
    const std::string title = entry != nullptr ? format1(t.appsConfirmTitleFormat, entry->app.name) : std::string(t.appsConfirmMenuTitle);
    const double titleSize = ui.fit(title, 24, 16, card.right() - 30 - titleX, true);
    ui.text(titleX, frame_ui::centerBaseline(y, 44, titleSize), ui.ellipsize(title, titleSize, card.right() - 30 - titleX, true),
            titleSize, rgb(kText), true);
    y += 44 + 22;
    ui.text(x, y + 18, ui.ellipsize(t.appsConfirmLead, 17, w), 17, rgb(kTextSoft));
    y += 25 + 10;
    // 実行するコマンド（等幅。入りきらなければ空白で折り返す）
    const frame_ui::Canvas mono {cr, fonts_.mono(), fonts_.mono(), frame_ui::Pointer {}};
    const std::string command = frame_apps::installerCommand(entry != nullptr ? entry->app.key : std::string());
    const std::vector<std::string> lines = wrapText(mono, command, 15, false, w - 32, 3);
    const Rect code {x, y, w, 28 + lines.size() * 22.0};
    fillRounded(cr, code, 12, kBg);
    strokeRounded(cr, code, 12, kDivider, 1);
    drawLines(mono, x + 16, y + 14 + 16, 22, lines, 15, kText);
    y = code.bottom() + 18;
    // 補足 3 つ（進み具合かメニュー・sudo を使わない・終わったら閉じる）
    for (const char* note : {entry != nullptr ? t.appsConfirmProgress : t.appsConfirmMenu, t.appsConfirmNoSudo, t.appsConfirmClose}) {
        const std::vector<std::string> rows = wrapText(ui, note, 14, false, w, 2);
        drawLines(ui, x, y + 16, 23, rows, 14, kTextMuted);
        y += rows.size() * 23.0;
    }
    // 開けなかった理由（赤）
    const char* error = unknownKey ? t.appsErrorFailed : nullptr;
    switch (launchResult_) {
        case LaunchResult::Started: break;
        case LaunchResult::Busy: error = t.appsErrorBusy; break;
        case LaunchResult::NoDisplay: error = t.appsErrorNoDisplay; break;
        case LaunchResult::NoKonsole: error = t.appsErrorNoKonsole; break;
        case LaunchResult::UnknownApp:
        case LaunchResult::Failed: error = t.appsErrorFailed; break;
    }
    if (error != nullptr) {
        const double size = ui.fit(error, 14, 11, w, true);
        ui.text(x, 544 - 14, ui.ellipsize(error, size, w, true), size, rgb(kDangerText), true);
    }
    // やめる・Konsole で開く（同じアプリ・メニューの Konsole がまだ開いていれば開けない）
    const bool busy = unknownKey || (entry == nullptr && model.menuBusy) || (entry != nullptr && entry->busy);
    addButton({PanelAction::AppsConfirmCancel}, frame_ui::drawButton(ui, {560, 544, 150, 56}, t.appsConfirmCancel));
    addButton({PanelAction::AppsConfirmLaunch, 0, confirmKey_},
              frame_ui::drawButton(ui, {724, 544, 186, 56}, t.appsConfirmLaunch, frame_ui::ButtonKind::Primary, !busy));
}

// ============================================================================
// 一番下の行
// ============================================================================

void MicPanel::drawFooter(const frame_ui::Canvas& ui, const UiText& t, const Config& config, const MicState& state) {
    frame_ui::FooterView view;
    view.lang = uiLang(config.language);
    view.autostart = state.autostart == Autostart::Enabled ? 1 : (state.autostart == Autostart::Disabled ? 0 : -1);
    view.autostartEnabled = state.autostart == Autostart::Enabled || state.autostart == Autostart::Disabled;
    view.quitArmed = quitArmed_;
    // 注意書き（1 行だけ）: 失敗（赤）があればそれ、無ければ自動起動が使えない理由、どちらも無ければ説明
    const MicError error = state.writeError != MicError::None ? state.writeError : state.readError;
    if (error != MicError::None) {
        view.note = errorText(error, t);
        view.noteIsError = true;
    } else if (state.loaded && !view.autostartEnabled) {
        view.note = state.autostart == Autostart::Missing ? t.autostartMissing : t.autostartUnknown;
    } else {
        view.note = t.footer;
    }
    const frame_ui::FooterHits hits = frame_ui::drawFooter(ui, kFooterRect, view);
    addButton({PanelAction::LanguageJa}, hits.langJa);
    addButton({PanelAction::LanguageEn}, hits.langEn);
    addButton({PanelAction::LanguageSc}, hits.langSc);
    addButton({PanelAction::AutostartOn}, hits.autostartOn);
    addButton({PanelAction::AutostartOff}, hits.autostartOff);
    addButton({PanelAction::Quit}, hits.quit);
}

// ============================================================================
// 重ねた画面
// ============================================================================

double MicPanel::drawModal(const frame_ui::Canvas& ui, Rect r, const std::string& title, const std::string& sub) {
    cairo_t* cr = ui.cr;
    setColor(cr, kBackdrop, kBackdropAlpha);
    frame_ui::roundedRect(cr, {0, 0, static_cast<double>(kWidth), static_cast<double>(kHeight)}, 24);
    cairo_fill(cr);
    fillRounded(cr, r, 18, kCard);
    strokeRounded(cr, r, 18, kBorder, 2);
    const Rect close {r.right() - 28 - 56, r.y + 20, 56, 56};
    const double textRight = close.x - 16;
    const double titleSize = ui.fit(title, 24, 16, textRight - (r.x + 28), true);
    ui.text(r.x + 28, r.y + 24 + 26, title, titleSize, rgb(kText), true);
    const double subSize = ui.fit(sub, 14, 11, textRight - (r.x + 28));
    ui.text(r.x + 28, r.y + 60 + 15, ui.ellipsize(sub, subSize, textRight - (r.x + 28)), subSize, rgb(kTextMuted));
    addButton({PanelAction::CloseOverlay}, frame_ui::drawButton(ui, close, ""));
    drawPolyline(cr, close.x + 19, close.y + 19, 18, 18, kText, 2.4, {4, 4, 14, 14, NAN, NAN, 14, 4, 4, 14});
    return r.y + 92;
}

void MicPanel::beginList(Rect box, double contentH) {
    listRect_ = box;
    listMaxScroll_ = std::max(0.0, contentH - box.h);
    listScroll_ = std::clamp(listScroll_, 0.0, listMaxScroll_);
}

void MicPanel::endList(const frame_ui::Canvas& ui, double contentH, double barX) {
    if (listMaxScroll_ <= 0) return;
    cairo_t* cr = ui.cr;
    const Rect box = listRect_;
    // 下に続きがあるときは、下の端をカードの色へぼかす
    if (listScroll_ < listMaxScroll_ - 0.5) {
        cairo_pattern_t* fade = cairo_pattern_create_linear(0, box.bottom() - 28, 0, box.bottom());
        cairo_pattern_add_color_stop_rgba(fade, 0, kCard.r, kCard.g, kCard.b, 0);
        cairo_pattern_add_color_stop_rgba(fade, 1, kCard.r, kCard.g, kCard.b, 1);
        cairo_set_source(cr, fade);
        cairo_rectangle(cr, box.x - 2, box.bottom() - 28, box.w + 4, 28);
        cairo_fill(cr);
        cairo_pattern_destroy(fade);
    }
    // 細いスクロールバー（溝とつまみ）
    fillRounded(cr, {barX, box.y, 6, box.h}, 3, kIconBox);
    const double thumbH = std::max(40.0, box.h * box.h / contentH);
    const double thumbY = box.y + (box.h - thumbH) * (listScroll_ / listMaxScroll_);
    fillRounded(cr, {barX, thumbY, 6, thumbH}, 3, kBorder);
}

void MicPanel::drawOutputPicker(const frame_ui::Canvas& ui, const UiText& t, const Config& config, const PanelModel& model) {
    cairo_t* cr = ui.cr;
    drawModal(ui, kOutputModal, t.outPickerTitle, t.outPickerSub);
    const AudioDevices& devices = model.state.devices;
    const std::string activeKey = model.activeOutputKey.empty() ? devices.defaultOutputKey : model.activeOutputKey;
    const std::vector<OutputEntry> entries = outputEntries(config, devices, activeKey, t);

    // 並び: 「つながっている出口」の見出し・行 → 「前に使った出口」の見出し・行（どれも間 8）
    /** 一覧の 1 項目（見出しか行）。 */
    struct Item {
        bool header;
        bool firstHeader;
        const OutputEntry* entry;
    };
    std::vector<Item> items;
    bool pastStarted = false;
    for (size_t i = 0; i < entries.size(); ++i) {
        if (i == 0 && entries[i].connected) items.push_back({true, true, nullptr});
        if (!entries[i].connected && !pastStarted) {
            items.push_back({true, false, nullptr});
            pastStarted = true;
        }
        items.push_back({false, false, &entries[i]});
    }
    const auto itemH = [](const Item& item) { return item.header ? (item.firstHeader ? 30.0 : 38.0) : kRowH; };
    double contentH = 0;
    for (const Item& item : items) contentH += itemH(item) + kRowGap;
    if (!items.empty()) contentH -= kRowGap;
    beginList(kOutputList, contentH);

    cairo_save(cr);
    cairo_rectangle(cr, kOutputList.x - 2, kOutputList.y, kOutputList.w + 4, kOutputList.h);
    cairo_clip(cr);
    double y = kOutputList.y - listScroll_;
    for (const Item& item : items) {
        const double h = itemH(item);
        if (y + h < kOutputList.y || y > kOutputList.bottom()) {
            y += h + kRowGap;
            continue;
        }
        if (item.header) {
            const std::string text = item.firstHeader ? t.outConnected : t.outPast;
            ui.text(kOutputList.x, y + h - 10, text, 15, rgb(kLabelSoft), true);
            y += h + kRowGap;
            continue;
        }
        const OutputEntry& e = *item.entry;
        const Rect row {kOutputList.x, y, kOutputList.w, kRowH};
        const bool viewing = e.key == viewedKey_;
        const Rect button {row.right() - 12 - kRowButtonW, y + (kRowH - 56) / 2, kRowButtonW, 56};
        const Rect rowArea {row.x, row.y, button.x - 12 - row.x, row.h};
        const int l = look(rowArea);
        fillRounded(cr, row, 16, viewing ? kRowSelected : (l == 2 ? kControlDown : (l == 1 ? kControl : kInset)));
        if (viewing) strokeRounded(cr, row, 16, kAccent, 2);
        // 右: ここから音を出す / いま使用中 / 忘れる
        if (e.active) {
            const Rect chip {button.x, y + (kRowH - 44) / 2, kRowButtonW, 44};
            fillRounded(cr, chip, 22, kSuccessTint);
            const double size = ui.fit(t.statusActive, 16, 12, chip.w - 48, true);
            const double w = 12 + 8 + ui.measure(t.statusActive, size, true);
            const double cx = chip.x + (chip.w - w) / 2;
            fillCircle(cr, cx + 6, chip.y + chip.h / 2, 6, kSuccess);
            ui.text(cx + 20, frame_ui::centerBaseline(chip.y, chip.h, size), t.statusActive, size, rgb(kSuccess), true);
        } else {
            const PanelAction action = e.connected ? PanelAction::OutputUse : PanelAction::OutputForget;
            const Rect hit = frame_ui::drawButton(ui, button, e.connected ? t.useOutput : t.forget);
            addButton({action, 0, e.key}, intersect(hit, kOutputList), true);
        }
        // 左: 絵・名前（設定を表示中の札）・状態（前に使った出口は絵と名前を少し薄く）
        const double cy = y + kRowH / 2;
        const double nameX = row.x + 14 + 44 + 14;
        const std::string summary = summaryText(e.profile.echo, e.profile.ns, t);
        const double summaryW = ui.measure(summary, 14);
        const double summaryRight = button.x - 20;
        const double nameRight = summaryRight - summaryW - 20;
        withAlpha(cr, e.connected ? 1.0 : 0.7, [&] {
            fillRounded(cr, {row.x + 14, cy - 22, 44, 44}, 11, kIconBox);
            drawOutputIcon(cr, isBuiltinSpeaker(e.key), row.x + 14 + 10, cy - 12, 24, kText);
        });
        double tagW = 0;
        if (viewing) tagW = ui.measure(t.viewingTag, 13, true) + 20 + 10;
        const double nameRoom = std::max(60.0, nameRight - nameX - tagW);
        const double nameSize = ui.fit(e.name, 19, 15, nameRoom, true);
        const std::string name = ui.ellipsize(e.name, nameSize, nameRoom, true);
        double nameW = 0;
        withAlpha(cr, e.connected ? 1.0 : 0.7,
                  [&] { nameW = ui.text(nameX, cy - 4, name, nameSize, rgb(kText), true); });
        if (viewing) {
            const Rect tag {nameX + nameW + 10, cy - 4 - 17, tagW - 10, 24};
            fillRounded(cr, tag, 12, kAccentTint);
            ui.text(tag.x + 10, frame_ui::centerBaseline(tag.y, tag.h, 13), t.viewingTag, 13, rgb(kAccentText), true);
        }
        std::string status;
        Color statusColor = kTextMuted;
        if (e.connected) {
            status = e.active ? t.statusActive : t.statusConnected;
            if (e.active) statusColor = kSuccess;
            fillCircle(cr, nameX + 4, cy + 17, 4, statusColor);
            ui.text(nameX + 14, cy + 22, status, 14, rgb(statusColor));
        } else {
            const std::string date = monthDay(e.profile.lastUsed);
            status = date.empty() ? std::string(t.lastUsedUnknown) : format1(t.lastUsedFormat, date);
            ui.text(nameX, cy + 22, status, 14, rgb(kTextMuted));
        }
        ui.text(summaryRight, frame_ui::centerBaseline(row.y, row.h, 14), summary, 14, rgb(kTextMuted), false, true);
        addButton({PanelAction::OutputView, 0, e.key}, intersect(rowArea, kOutputList), true);
        y += kRowH + kRowGap;
    }
    cairo_restore(cr);
    endList(ui, contentH, kOutputList.right() + 8);

    drawLines(ui, kOutputList.x, 650 + 14, 20, wrapText(ui, t.outPickerNote, 13, false, kOutputList.w, 2), 13, kTextMuted);
}

void MicPanel::drawMicPicker(const frame_ui::Canvas& ui, const UiText& t, const PanelModel& model) {
    cairo_t* cr = ui.cr;
    drawModal(ui, kMicModal, t.micPickerTitle, t.micPickerSub);
    const AudioDevices& devices = model.state.devices;
    const double contentH = devices.inputs.empty() ? 0 : devices.inputs.size() * (kRowH + kRowGap) - kRowGap;
    beginList(kMicList, contentH);
    if (devices.inputs.empty()) ui.text(kMicList.x, kMicList.y + 30, t.loading, 16, rgb(kTextMuted));
    cairo_save(cr);
    cairo_rectangle(cr, kMicList.x - 2, kMicList.y, kMicList.w + 4, kMicList.h);
    cairo_clip(cr);
    double y = kMicList.y - listScroll_;
    for (const AudioEndpoint& e : devices.inputs) {
        if (y + kRowH >= kMicList.y && y <= kMicList.bottom()) {
            const Rect row {kMicList.x, y, kMicList.w, kRowH};
            fillRounded(cr, row, 16, kInset);
            const double cy = y + kRowH / 2;
            fillRounded(cr, {row.x + 14, cy - 22, 44, 44}, 11, kIconBox);
            drawMicIcon(cr, row.x + 14 + 11, cy - 11, 22, kText, false);
            const bool builtin = isBuiltinMic(e.key);
            const Rect button {row.right() - 12 - kRowButtonW, y + (kRowH - 56) / 2, kRowButtonW, 56};
            const double textX = row.x + 14 + 44 + 14;
            const double room = button.x - 16 - textX;
            const std::string name = builtin ? std::string(t.builtinMicName) : format1(t.micOfFormat, e.name);
            const double nameSize = ui.fit(name, 19, 15, room, true);
            ui.text(textX, cy - 4, ui.ellipsize(name, nameSize, room, true), nameSize, rgb(kText), true);
            const std::string note = builtin ? t.micNoteBuiltin : t.micNoteExternal;
            ui.text(textX, cy + 22, ui.ellipsize(note, 14, room), 14, rgb(kTextMuted));
            if (e.key == devices.defaultInputKey) {
                const Rect chip {button.x, y + (kRowH - 44) / 2, kRowButtonW, 44};
                fillRounded(cr, chip, 22, kSuccessTint);
                const double size = ui.fit(t.statusActive, 16, 12, chip.w - 48, true);
                const double w = 12 + 8 + ui.measure(t.statusActive, size, true);
                const double cx = chip.x + (chip.w - w) / 2;
                fillCircle(cr, cx + 6, chip.y + chip.h / 2, 6, kSuccess);
                ui.text(cx + 20, frame_ui::centerBaseline(chip.y, chip.h, size), t.statusActive, size, rgb(kSuccess), true);
            } else {
                addButton({PanelAction::MicUse, 0, e.key}, intersect(frame_ui::drawButton(ui, button, t.useMic), kMicList), true);
            }
        }
        y += kRowH + kRowGap;
    }
    cairo_restore(cr);
    endList(ui, contentH, kMicList.right() + 8);
}

// ============================================================================
// 全体
// ============================================================================

void MicPanel::render(const Config& config, const PanelModel& model) {
    const UiText& t = uiText(config.language);
    buttons_.clear();
    tracks_.clear();

    // 地（不透明。コントラスト比は不透明な地で計算している）
    cairo_save(cr_);
    cairo_set_operator(cr_, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr_);
    cairo_restore(cr_);

    // 重ねた画面を出している間は、後ろの部品は乗っている・押している見た目にしない（押せない）
    const bool modal = overlay_ != PanelOverlay::None;
    const frame_ui::Pointer live = pointer_;
    frame_ui::Pointer shown = pointer_;
    if (listDragging_) shown.inside = false;
    if (modal) pointer_.inside = false;  // look() と frame-ui の部品が後ろでポインターを見ないように
    const frame_ui::Canvas back {cr_, fonts_.regular(), fonts_.bold(), modal ? frame_ui::Pointer {} : shown};
    frame_ui::drawPanelBackground(back, kWidth, kHeight);
    if (view_ == PanelView::Settings) {
        drawSettings(back, t, config, model);
    } else {
        drawApps(back, t, config, model);
    }
    drawHeader(back, t, model);
    drawFooter(back, t, config, model.state);

    if (modal) {
        pointer_ = live;
        buttons_.clear();  // 後ろのボタンは押せない
        tracks_.clear();
        const frame_ui::Canvas top {cr_, fonts_.regular(), fonts_.bold(), shown};
        switch (overlay_) {
            case PanelOverlay::OutputPicker: drawOutputPicker(top, t, config, model); break;
            case PanelOverlay::MicPicker: drawMicPicker(top, t, model); break;
            case PanelOverlay::AppsList: drawAppsList(top, t, config, model); break;
            case PanelOverlay::AppsConfirm:
                if (confirmBack_ == PanelOverlay::AppsList) {
                    // 一覧の上に確認を重ねる: 一覧は押せないようにして、もう一段暗くする
                    const frame_ui::Canvas listUi {cr_, fonts_.regular(), fonts_.bold(), frame_ui::Pointer {}};
                    drawAppsList(listUi, t, config, model);
                    buttons_.clear();
                }
                listRect_ = {};
                listMaxScroll_ = 0;
                drawAppsConfirm(top, t, config, model);
                break;
            case PanelOverlay::None: break;
        }
    } else {
        listRect_ = {};
        listMaxScroll_ = 0;
    }

    // 押せなくなったボタンに乗っていた印は外す
    bool hoverFound = false;
    for (const auto& b : buttons_) hoverFound |= b.hit == hover_;
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

bool MicPanel::buttonCenter(PanelAction action, double& x, double& y, const std::string& key) const {
    for (const auto& b : buttons_) {
        if (b.hit.action != action || (!key.empty() && b.hit.key != key)) continue;
        x = b.rect.x + b.rect.w / 2;
        y = b.rect.y + b.rect.h / 2;
        return true;
    }
    return false;
}

Rect MicPanel::buttonRect(PanelAction action) const {
    for (const auto& b : buttons_) {
        if (b.hit.action == action) return b.rect;
    }
    return {};
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
    drawThumbnail(cr, fonts, size);
    cairo_surface_flush(surface);
    surfaceToRgba(surface, rgba);
    if (!pngPath.empty()) cairo_surface_write_to_png(surface, pngPath.c_str());
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}
