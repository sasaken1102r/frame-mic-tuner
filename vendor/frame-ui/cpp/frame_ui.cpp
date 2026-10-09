// 共通の見た目の部品の実装（frame_ui.h）。
#include "frame_ui.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>

namespace frame_ui {

namespace {

/** 部品の今の見た目。 */
enum class Look { Idle, Hover, Down };

/**
 * ポインターが矩形の上にあるか・押しているかを見る。
 * @param ui 描く先（ポインター）
 * @param hit 当たり判定（空なら常に Idle）
 * @return 見た目
 */
Look lookAt(const Canvas& ui, Rect hit) {
    if (!ui.pointer.inside || !hit.contains(ui.pointer.x, ui.pointer.y)) return Look::Idle;
    return ui.pointer.down ? Look::Down : Look::Hover;
}

/**
 * 色を設定する。
 * @param cr cairo
 * @param c 色
 */
void setColor(cairo_t* cr, Rgb c) {
    cairo_set_source_rgb(cr, c.r, c.g, c.b);
}

/**
 * 塗りつぶした角丸の四角。
 * @param cr cairo
 * @param r 矩形
 * @param radius 角の半径
 * @param c 色
 */
void fillRounded(cairo_t* cr, Rect r, double radius, Rgb c) {
    setColor(cr, c);
    roundedRect(cr, r, radius);
    cairo_fill(cr);
}

/**
 * 内側に収まる角丸の枠線。
 * @param cr cairo
 * @param r 矩形（線はこの内側に引く）
 * @param radius 角の半径
 * @param c 色
 * @param width 線の太さ
 */
void strokeRounded(cairo_t* cr, Rect r, double radius, Rgb c, double width) {
    setColor(cr, c);
    cairo_set_line_width(cr, width);
    roundedRect(cr, {r.x + width / 2, r.y + width / 2, r.w - width, r.h - width}, std::max(0.0, radius - width / 2));
    cairo_stroke(cr);
}

/**
 * 描いたものを薄くする（使えない部品）。alpha が 1 ならそのまま描く。
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
 * 文言の %s を置き換える（printf を使わない。表の文字列を書式として渡さないため）。
 * @param format %s を 1 つ含む文言
 * @param arg 入れる文字列
 * @return 置き換えた文字列
 */
std::string fill(const char* format, const std::string& arg) {
    std::string out = format;
    const size_t at = out.find("%s");
    if (at != std::string::npos) out.replace(at, 2, arg);
    return out;
}

/**
 * ✓ ＋ 文言を横の真ん中にそろえて描く（✓ は check のときだけ）。
 * @param ui 描く先
 * @param r 置く矩形
 * @param label 文言
 * @param size 最初の大きさ（入らなければ小さくする）
 * @param c 色
 * @param isBold 太字か
 * @param check 先頭に ✓ を付けるか
 */
void drawLabel(const Canvas& ui, Rect r, const std::string& label, double size, Rgb c, bool isBold, bool check) {
    const double checkW = check ? size * 0.8 + 7 : 0;
    const double fitted = ui.fit(label, size, 13, r.w - 24 - checkW, isBold);
    const std::string shown = ui.ellipsize(label, fitted, r.w - 16 - checkW, isBold);
    const double textW = ui.measure(shown, fitted, isBold);
    const double x = r.x + (r.w - checkW - textW) / 2;
    const double cy = r.y + r.h / 2;
    if (check) drawCheck(ui.cr, x + size * 0.4, cy, size * 0.8, c);
    ui.text(x + checkW, centerBaseline(r.y, r.h, fitted), shown, fitted, c, isBold);
}

}  // namespace

// ============================================================================
// 描く道具
// ============================================================================

double Canvas::measure(const std::string& text, double size, bool isBold) const {
    cairo_set_font_face(cr, isBold ? bold : regular);
    cairo_set_font_size(cr, size);
    cairo_text_extents_t extents;
    cairo_text_extents(cr, text.c_str(), &extents);
    return extents.x_advance;
}

double Canvas::text(double x, double baseline, const std::string& text, double size, Rgb c, bool isBold,
                    bool alignRight) const {
    const double width = measure(text, size, isBold);
    setColor(cr, c);
    cairo_move_to(cr, alignRight ? x - width : x, baseline);
    cairo_show_text(cr, text.c_str());
    return width;
}

double Canvas::fit(const std::string& text, double size, double minSize, double maxWidth, bool isBold) const {
    while (size > minSize && measure(text, size, isBold) > maxWidth) size -= 1;
    return size;
}

std::string Canvas::ellipsize(const std::string& text, double size, double maxWidth, bool isBold) const {
    if (measure(text, size, isBold) <= maxWidth) return text;
    std::string cut = text;
    while (!cut.empty()) {
        // UTF-8 の 1 文字ぶん（続きのバイト 10xxxxxx を含めて）後ろから削る
        size_t end = cut.size() - 1;
        while (end > 0 && (static_cast<unsigned char>(cut[end]) & 0xC0) == 0x80) --end;
        cut.erase(end);
        if (measure(cut + "…", size, isBold) <= maxWidth) return cut + "…";
    }
    return "…";
}

double centerBaseline(double top, double h, double size) {
    return top + h / 2 + size * 0.36;
}

void roundedRect(cairo_t* cr, Rect r, double radius) {
    radius = std::max(0.0, std::min(radius, std::min(r.w, r.h) / 2));
    cairo_new_sub_path(cr);
    cairo_arc(cr, r.x + r.w - radius, r.y + radius, radius, -M_PI / 2, 0);
    cairo_arc(cr, r.x + r.w - radius, r.y + r.h - radius, radius, 0, M_PI / 2);
    cairo_arc(cr, r.x + radius, r.y + r.h - radius, radius, M_PI / 2, M_PI);
    cairo_arc(cr, r.x + radius, r.y + radius, radius, M_PI, 3 * M_PI / 2);
    cairo_close_path(cr);
}

void drawCheck(cairo_t* cr, double cx, double cy, double s, Rgb c) {
    cairo_save(cr);
    setColor(cr, c);
    cairo_set_line_width(cr, s * 0.15);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_move_to(cr, cx - s * 0.36, cy + s * 0.02);
    cairo_line_to(cr, cx - s * 0.10, cy + s * 0.28);
    cairo_line_to(cr, cx + s * 0.38, cy - s * 0.26);
    cairo_stroke(cr);
    cairo_restore(cr);
}

void drawPanelBackground(const Canvas& ui, double w, double h) {
    fillRounded(ui.cr, {0, 0, w, h}, 24, kBg);
}

// ============================================================================
// 部品
// ============================================================================

std::vector<Rect> drawSegmented(const Canvas& ui, Rect r, const std::vector<std::string>& labels, int selected,
                                bool enabled) {
    std::vector<Rect> hits;
    const int n = static_cast<int>(labels.size());
    if (n == 0) return hits;
    const double sliceW = r.w / n;
    for (int i = 0; i < n; ++i) hits.push_back({r.x + sliceW * i, r.y, sliceW, r.h});

    withAlpha(ui.cr, enabled ? 1.0 : kDisabledAlpha, [&] {
        strokeRounded(ui.cr, r, kSegRadius, kBorder, 2);
        // 枠 2px ＋ 余白 5px の内側に、4px あけて選択肢を並べる
        const double inset = 2 + kSegInset;
        const double gap = 4;
        const Rect inner {r.x + inset, r.y + inset, r.w - inset * 2, r.h - inset * 2};
        const double optionW = (inner.w - gap * (n - 1)) / n;
        for (int i = 0; i < n; ++i) {
            const Rect option {inner.x + (optionW + gap) * i, inner.y, optionW, inner.h};
            const Look look = enabled ? lookAt(ui, hits[i]) : Look::Idle;
            const bool on = i == selected;
            if (on) {
                fillRounded(ui.cr, option, 24, look == Look::Down ? kAccentDown : look == Look::Hover ? kAccentHover : kAccent);
            } else if (look != Look::Idle) {
                fillRounded(ui.cr, option, 24, look == Look::Down ? kControlDown : kControlHover);
            }
            drawLabel(ui, option, labels[i], kControlSize, on ? kOnAccent : kOptionText, on, on);
        }
    });
    if (!enabled) hits.assign(hits.size(), Rect {});
    return hits;
}

StepperHits drawStepper(const Canvas& ui, Rect r, const std::string& value, bool enabled, bool canMinus, bool canPlus) {
    StepperHits hits {{r.x, r.y, kStepperButtonW, r.h}, {r.right() - kStepperButtonW, r.y, kStepperButtonW, r.h}};
    if (!enabled || !canMinus) hits.minus = {};
    if (!enabled || !canPlus) hits.plus = {};
    cairo_t* cr = ui.cr;

    withAlpha(cr, enabled ? 1.0 : kDisabledAlpha, [&] {
        // 乗っている・押している側だけ、カプセルの形に切り抜いて塗る
        for (const Rect& part : {hits.minus, hits.plus}) {
            const Look look = lookAt(ui, part);
            if (look == Look::Idle) continue;
            cairo_save(cr);
            roundedRect(cr, r, kSegRadius);
            cairo_clip(cr);
            setColor(cr, look == Look::Down ? kControlDown : kControlHover);
            cairo_rectangle(cr, part.x, part.y, part.w, part.h);
            cairo_fill(cr);
            cairo_restore(cr);
        }
        strokeRounded(cr, r, kSegRadius, kBorder, 2);
        // 値の左右の区切り線（1px）
        setColor(cr, kDivider);
        cairo_set_line_width(cr, 1);
        for (const double x : {r.x + kStepperButtonW + 0.5, r.right() - kStepperButtonW - 0.5}) {
            cairo_move_to(cr, x, r.y + 2);
            cairo_line_to(cr, x, r.bottom() - 2);
        }
        cairo_stroke(cr);

        // − と ＋ は線で描く（フォントによらず同じ太さ・位置に）
        const double cy = r.y + r.h / 2;
        const double arm = 8;
        cairo_set_line_width(cr, 2.2);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        const double minusCx = r.x + kStepperButtonW / 2;
        const double plusCx = r.right() - kStepperButtonW / 2;
        cairo_set_source_rgba(cr, kText.r, kText.g, kText.b, canMinus ? 1.0 : kDisabledAlpha);
        cairo_move_to(cr, minusCx - arm, cy);
        cairo_line_to(cr, minusCx + arm, cy);
        cairo_stroke(cr);
        cairo_set_source_rgba(cr, kText.r, kText.g, kText.b, canPlus ? 1.0 : kDisabledAlpha);
        cairo_move_to(cr, plusCx - arm, cy);
        cairo_line_to(cr, plusCx + arm, cy);
        cairo_move_to(cr, plusCx, cy - arm);
        cairo_line_to(cr, plusCx, cy + arm);
        cairo_stroke(cr);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);

        const Rect middle {r.x + kStepperButtonW, r.y, r.w - kStepperButtonW * 2, r.h};
        drawLabel(ui, middle, value, kValueSize, kText, true, false);
    });
    return hits;
}

double buttonWidth(const Canvas& ui, const std::string& label, double minWidth) {
    return std::max(minWidth, std::ceil(ui.measure(label, kControlSize, true)) + kButtonPadX * 2);
}

Rect drawButton(const Canvas& ui, Rect r, const std::string& label, ButtonKind kind, bool enabled) {
    const Rect hit = enabled ? r : Rect {};
    const Look look = lookAt(ui, hit);
    Rgb fillColor = kControl;
    Rgb border = kBorder;
    Rgb textColor = kText;
    switch (kind) {
        case ButtonKind::Normal:
            fillColor = look == Look::Down ? kControlDown : look == Look::Hover ? kControlHover : kControl;
            border = look == Look::Down ? kAccent : kBorder;
            break;
        case ButtonKind::Primary:
        case ButtonKind::Selected:
            fillColor = look == Look::Down ? kAccentDown : look == Look::Hover ? kAccentHover : kAccent;
            border = fillColor;
            textColor = kOnAccent;
            break;
        case ButtonKind::Danger:
            fillColor = look == Look::Down ? kDangerDown : look == Look::Hover ? kDangerHover : kDangerFill;
            border = kDanger;
            break;
        case ButtonKind::DangerArmed:
            fillColor = kDanger;
            border = kDanger;
            textColor = kOnAccent;
            break;
    }
    withAlpha(ui.cr, enabled ? 1.0 : kDisabledAlpha, [&] {
        fillRounded(ui.cr, r, kButtonRadius, fillColor);
        strokeRounded(ui.cr, r, kButtonRadius, border, 2);
        drawLabel(ui, r, label, kControlSize, textColor, true, kind == ButtonKind::Selected);
    });
    return hit;
}

double drawCard(const Canvas& ui, Rect r, const std::string& title, const std::string& sub, Edge edge) {
    fillRounded(ui.cr, r, kCardRadius, kCard);
    if (edge != Edge::None) strokeRounded(ui.cr, r, kCardRadius, edge == Edge::Pink ? kAccent : kDanger, 2);
    const double innerW = r.w - kCardPadX * 2;
    double titleW = 0;
    // 見出しの行（高さ kCardTitleH）の中で、見出しと補足のベースラインをそろえる
    const double baseline = centerBaseline(r.y + kCardPadY, kCardTitleH, kCardTitleSize);
    if (!title.empty()) {
        const double size = ui.fit(title, kCardTitleSize, 16, innerW, true);
        titleW = ui.text(r.x + kCardPadX, baseline, title, size, kText, true);
    }
    if (!sub.empty()) {
        const double room = innerW - titleW - (titleW > 0 ? 16 : 0);
        const double size = ui.fit(sub, kHintSize, 11, room);
        ui.text(r.right() - kCardPadX, baseline, ui.ellipsize(sub, size, room), size, kTextMuted, false, true);
    }
    return r.y + kCardPadY + (title.empty() && sub.empty() ? 0 : kCardTitleH);
}

double chipWidth(const Canvas& ui, const std::string& label) {
    return 18 + 12 + 10 + std::ceil(ui.measure(label, kControlSize, true)) + 18;
}

Rect drawChip(const Canvas& ui, double right, double centerY, const std::string& label, Tone tone) {
    const double w = chipWidth(ui, label);
    const Rect r {right - w, centerY - kChipH / 2, w, kChipH};
    const Rgb fillColor = tone == Tone::Ok ? kChipOkFill : tone == Tone::Bad ? kChipBadFill : kChipIdleFill;
    const Rgb textColor = tone == Tone::Ok ? kChipOkText : tone == Tone::Bad ? kChipBadText : kChipIdleText;
    fillRounded(ui.cr, r, kChipH / 2, fillColor);
    const double dotCx = r.x + 18 + 6;
    cairo_new_sub_path(ui.cr);
    if (tone == Tone::Idle) {
        // 止まっているときは ○（色だけで伝えない）
        setColor(ui.cr, textColor);
        cairo_set_line_width(ui.cr, 2);
        cairo_arc(ui.cr, dotCx, centerY, 5, 0, 2 * M_PI);
        cairo_stroke(ui.cr);
    } else {
        setColor(ui.cr, tone == Tone::Bad ? kChipBadDot : textColor);
        cairo_arc(ui.cr, dotCx, centerY, 6, 0, 2 * M_PI);
        cairo_fill(ui.cr);
    }
    ui.text(r.x + 18 + 12 + 10, centerBaseline(r.y, r.h, kControlSize), label, kControlSize, textColor, true);
    return r;
}

void drawHeader(const Canvas& ui, Rect r, const std::string& title, const std::string& chipLabel, Tone tone) {
    const double chipW = chipLabel.empty() ? 0 : chipWidth(ui, chipLabel);
    const double size = ui.fit(title, kTitleSize, 22, r.w - chipW - 24, true);
    ui.text(r.x, centerBaseline(r.y, r.h, size), title, size, kText, true);
    if (!chipLabel.empty()) drawChip(ui, r.right(), r.y + r.h / 2, chipLabel, tone);
}

// ============================================================================
// 文言
// ============================================================================

const Strings& strings(Lang lang) {
    static const Strings ja {
        "言語",
        "日本語",
        "English",
        "简体中文",
        "SteamVR と一緒に起動",
        "オン",
        "オフ",
        "終了",
        "もう一度押すと終了",
        "既定に戻す",
        "最新版です（%s）",
        "新しい版を確かめています…",
        "新しい版 %s があります",
        "更新する",
        "ここからは入れられない版です。GitHub から手で更新してね",
        "%s に更新しますか？",
        "更新する",
        "やめる",
        "更新中: %s",
        "この画面が閉じて開き直すことがあります",
        "%s を入れました。開き直すと新しい版になります",
        "更新できませんでした（今の版のままです）:",
        "新しい版を確かめられませんでした:",
        "今すぐ確かめる",
        "もう一度",
        "閉じる",
        "今の版: %s",
    };
    static const Strings en {
        "Language",
        "日本語",
        "English",
        "简体中文",
        "Start with SteamVR",
        "On",
        "Off",
        "Quit",
        "Press again to quit",
        "Reset",
        "Up to date (%s)",
        "Checking for updates…",
        "Version %s is available",
        "Update",
        "This version can't be installed from here. Update by hand from GitHub",
        "Update to %s?",
        "Update",
        "Cancel",
        "Updating: %s",
        "This panel may close and reopen",
        "%s is installed. Reopen to use it",
        "The update failed (nothing was changed):",
        "Couldn't check for updates:",
        "Check now",
        "Try again",
        "Close",
        "Current version: %s",
    };
    // 簡体字中国語（frame-mic-tuner の簡体字中国語の表 #1 の訳から。更新中の補足・入れ終わり・今の版は frame-ui の言い回しに合わせた）
    static const Strings sc {
        "语言",
        "日本語",
        "English",
        "简体中文",
        "随 SteamVR 启动",
        "开",
        "关",
        "退出",
        "再按一次退出",
        "恢复默认",
        "已是最新版本（%s）",
        "正在检查新版本…",
        "有新版本 %s 可用",
        "更新",
        "此版本无法自动安装。请到 GitHub 手动更新",
        "要更新到 %s 吗？",
        "更新",
        "取消",
        "更新中：%s",
        "此面板可能会关闭并重新打开",
        "已安装 %s。重新打开即可使用新版本",
        "更新失败（仍为当前版本）：",
        "无法检查新版本：",
        "立即检查",
        "重试",
        "关闭",
        "当前版本：%s",
    };
    if (lang == Lang::En) return en;
    if (lang == Lang::Sc) return sc;
    return ja;
}

namespace {

/** 1 つの文言の 3 言語ぶん。 */
struct Text3 {
    const char* ja;
    const char* en;
    const char* sc;

    /**
     * @param lang 言語
     * @return その言語の文言
     */
    const char* in(Lang lang) const { return lang == Lang::En ? en : lang == Lang::Sc ? sc : ja; }
};

}  // namespace

std::string updateStepText(Lang lang, const std::string& step) {
    static const std::map<std::string, Text3> table = {
        {"start", {"準備中", "Preparing", "准备中"}},
        {"download", {"ダウンロード中", "Downloading", "下载中"}},
        {"verify", {"ファイルを確認中", "Verifying", "校验文件中"}},
        {"extract", {"展開中", "Unpacking", "解压中"}},
        {"install", {"入れ替え中", "Installing", "替换中"}},
    };
    const auto it = table.find(step);
    if (it == table.end()) return step;
    return it->second.in(lang);
}

std::string updateErrorText(Lang lang, const std::string& error) {
    static const Text3 updaterBroken {"更新の仕組みが動きませんでした", "The updater didn't run", "更新组件没有运行"};
    static const std::map<std::string, Text3> table = {
        {"network", {"GitHub につながりません", "Can't reach GitHub", "无法连接 GitHub"}},
        {"rate-limited",
         {"GitHub の回数制限にかかりました。1 時間ほどあとで試してね", "GitHub's rate limit was hit. Try again in an hour",
          "触发了 GitHub 的频率限制。请过一小时左右再试"}},
        {"not-found", {"公開されている版がありません", "No published release", "没有已发布的版本"}},
        {"bad-response", {"GitHub の返事を読めませんでした", "Couldn't read GitHub's answer", "无法解析 GitHub 的响应"}},
        {"bad-version", {"版の番号を読めませんでした", "Couldn't read the version number", "无法读取版本号"}},
        {"bad-url",
         {"GitHub 以外の場所へ向かったので止めました", "Stopped: the download led outside GitHub",
          "已中止：下载地址指向了 GitHub 以外的位置"}},
        {"missing-tool",
         {"必要なコマンド（python3）がありません", "A required command (python3) is missing", "缺少必需的命令（python3）"}},
        {"no-checksums",
         {"この版には確認用の SHA256SUMS がありません。手で更新してね", "This release has no SHA256SUMS. Update by hand",
          "此版本没有用于校验的 SHA256SUMS。请手动更新"}},
        {"no-asset", {"この版には入れるファイルがありません", "This release has no file to install", "此版本没有可安装的文件"}},
        {"checksum-mismatch",
         {"ダウンロードしたファイルが壊れています", "The download is corrupt (checksum mismatch)", "下载的文件已损坏"}},
        {"unsafe-archive",
         {"ファイルの中身が安全でないので止めました", "Stopped: the archive has unsafe contents", "已中止：压缩包内容不安全"}},
        {"no-installer", {"ファイルに install.sh がありません", "The archive has no install.sh", "压缩包中没有 install.sh"}},
        {"install-failed", {"install.sh が失敗しました", "install.sh failed", "install.sh 执行失败"}},
        {"bad-args",
         {"前回のインストールのオプションを読めません", "The saved install options are invalid", "无法读取上次安装的选项"}},
        {"busy", {"別の更新が動いています", "Another update is running", "另一个更新正在运行"}},
        {"not-newer", {"もう最新版です", "Already up to date", "已是最新版本"}},
        {"detach-failed",
         {"更新を始められませんでした（systemd-run）", "Couldn't start the update (systemd-run)", "无法启动更新（systemd-run）"}},
        {"interrupted", {"更新が途中で止まりました", "The update was interrupted", "更新中途停止"}},
        {"io", {"ファイルを書けませんでした", "Couldn't write files", "无法写入文件"}},
        {"usage", updaterBroken},
        {"script-failed", updaterBroken},
        {"spawn-failed", updaterBroken},
        {"other", {"うまくいきませんでした", "Something went wrong", "出现了问题"}},
    };
    auto it = table.find(error);
    if (it == table.end()) it = table.find("other");
    return it->second.in(lang);
}

// ============================================================================
// 更新の帯と一番下の行
// ============================================================================

std::vector<UpdateHit> drawUpdateBar(const Canvas& ui, Rect r, const frame_updater::UpdateStatus& status,
                                     bool confirming, Lang lang) {
    using frame_updater::UpdateState;
    const Strings& t = strings(lang);

    /** 帯の右に並べるボタン 1 つ。 */
    struct BarButton {
        UpdateAction action;
        const char* label;
        ButtonKind kind;
        bool enabled;
    };
    std::string line;       // 左の文
    std::string subLine;    // その下の 14px の補足（手で更新するとき）
    std::string rightHint;  // 右の 14px の補足（更新中）
    Rgb lineColor = kTextSoft;
    bool lineBold = false;
    Edge edge = Edge::None;
    // ボタンは push_back で積む（vector に初期化リストを代入すると、GCC 15 の -O3 で -Wnonnull の誤検知が出るため）
    std::vector<BarButton> buttons;
    const BarButton checkNow {UpdateAction::CheckNow, t.updateCheckNow, ButtonKind::Normal, !status.checking};

    switch (status.state) {
        case UpdateState::Installing:
            line = fill(t.updateInstallingFormat, updateStepText(lang, status.step) + "…");
            rightHint = t.updateInstallingHint;
            break;
        case UpdateState::Installed:
            line = fill(t.updateInstalledFormat, status.version);
            lineColor = kText;
            lineBold = true;
            edge = Edge::Pink;
            buttons.push_back({UpdateAction::Dismiss, t.updateDismiss, ButtonKind::Normal, true});
            break;
        case UpdateState::InstallFailed:
            line = std::string(t.updateInstallFailed) + " " + updateErrorText(lang, status.error);
            lineColor = kDangerText;
            lineBold = true;
            edge = Edge::Red;
            buttons.push_back({UpdateAction::Retry, t.updateRetry, ButtonKind::Normal, true});
            buttons.push_back({UpdateAction::Dismiss, t.updateDismiss, ButtonKind::Normal, true});
            break;
        case UpdateState::Available:
            edge = Edge::Pink;
            lineBold = true;
            if (status.installable && confirming) {
                line = fill(t.updateConfirmFormat, status.latest);
                lineColor = kText;
                buttons.push_back({UpdateAction::ConfirmNo, t.updateConfirmNo, ButtonKind::Normal, true});
                buttons.push_back({UpdateAction::ConfirmYes, t.updateConfirmYes, ButtonKind::Primary, true});
            } else {
                line = fill(t.updateAvailableFormat, status.latest);
                lineColor = kAccentText;
                buttons.push_back(checkNow);
                if (status.installable) {
                    buttons.push_back({UpdateAction::Install, t.updateButton, ButtonKind::Primary, true});
                } else {
                    subLine = t.updateManual;
                }
            }
            break;
        case UpdateState::UpToDate:
            line = status.checking ? t.updateChecking : fill(t.updateUpToDateFormat, status.current);
            buttons.push_back(checkNow);
            break;
        case UpdateState::CheckFailed:
            if (status.checking) {
                line = t.updateChecking;
            } else {
                line = std::string(t.updateCheckFailed) + " " + updateErrorText(lang, status.error);
                lineColor = kDangerText;
            }
            buttons.push_back(checkNow);
            break;
        case UpdateState::Unknown:
            line = status.checking ? t.updateChecking : fill(t.updateCurrentFormat, status.current);
            buttons.push_back(checkNow);
            break;
    }

    drawCard(ui, r, "", "", edge);

    // ボタンは右から（右の余白 10、間 10、縦は真ん中）
    std::vector<UpdateHit> hits;
    double left = r.right() - 10;
    const double by = r.y + (r.h - kControlH) / 2;
    for (auto it = buttons.rbegin(); it != buttons.rend(); ++it) {
        const double w = buttonWidth(ui, it->label, 110);
        left -= w;
        const Rect hit = drawButton(ui, {left, by, w, kControlH}, it->label, it->kind, it->enabled);
        if (hit.w > 0) hits.push_back({it->action, hit});
        left -= 10;
    }
    double textRight = buttons.empty() ? r.right() - kCardPadX : left - 6;
    if (!rightHint.empty()) {
        const double room = (r.w - kCardPadX * 2) * 0.45;
        const double size = ui.fit(rightHint, kHintSize, 11, room);
        const std::string shown = ui.ellipsize(rightHint, size, room);
        const double w = ui.text(r.right() - kCardPadX, centerBaseline(r.y, r.h, size), shown, size, kTextMuted, false, true);
        textRight = r.right() - kCardPadX - w - 16;
    }

    const double x = r.x + kCardPadX;
    const double room = textRight - x;
    const double size = ui.fit(line, kControlSize, 15, room, lineBold);
    const std::string shown = ui.ellipsize(line, size, room, lineBold);
    if (subLine.empty()) {
        ui.text(x, centerBaseline(r.y, r.h, size), shown, size, lineColor, lineBold);
    } else {
        ui.text(x, r.y + 32, shown, size, lineColor, lineBold);
        const double subSize = ui.fit(subLine, kHintSize, 11, room);
        ui.text(x, r.y + 55, ui.ellipsize(subLine, subSize, room), subSize, kTextMuted);
    }
    return hits;
}

FooterHits drawFooter(const Canvas& ui, Rect r, const FooterView& view) {
    const Strings& t = strings(view.lang);
    FooterHits hits;
    const double cy = r.y + kFooterRowH / 2;
    const double controlY = cy - kControlH / 2;
    const double labelBaseline = centerBaseline(r.y, kFooterRowH, kFooterLabelSize);
    double x = r.x;

    x += ui.text(x, labelBaseline, t.language, kFooterLabelSize, kLabelSoft) + 14;
    // 言語の名前は、どの言語でもその言語自身の書き方。3 つのときは 1 つあたり 120（「简体中文」が 16px 以上で入る幅）
    const bool sc = view.offerSimplifiedChinese;
    const double langW = sc ? 360 : 230;
    std::vector<std::string> langLabels {t.japanese, t.english};
    if (sc) langLabels.push_back(t.simplifiedChinese);
    const int langSelected = view.lang == Lang::Ja ? 0 : view.lang == Lang::En ? 1 : (sc ? 2 : -1);
    const auto lang = drawSegmented(ui, {x, controlY, langW, kControlH}, langLabels, langSelected);
    hits.langJa = lang[0];
    hits.langEn = lang[1];
    if (sc) hits.langSc = lang[2];
    x += langW + 14 + 18;

    x += ui.text(x, labelBaseline, t.startWithSteamVr, kFooterLabelSize, kLabelSoft) + 14;
    const int autostart = view.autostart == 1 ? 0 : view.autostart == 0 ? 1 : -1;
    const auto on = drawSegmented(ui, {x, controlY, 180, kControlH}, {t.on, t.off}, autostart, view.autostartEnabled);
    hits.autostartOn = on[0];
    hits.autostartOff = on[1];

    // 終了は幅 160 の赤い枠。確認中は赤い塗りで「もう一度押すと終了」（入らなければ左へ広げる）
    const std::string quitLabel = view.quitArmed ? t.quitConfirm : t.quit;
    const double quitW = view.quitArmed ? buttonWidth(ui, quitLabel, 160) : 160;
    hits.quit = drawButton(ui, {r.right() - quitW, controlY, quitW, kControlH}, quitLabel,
                           view.quitArmed ? ButtonKind::DangerArmed : ButtonKind::Danger);

    if (!view.note.empty()) {
        const double size = ui.fit(view.note, kHintSize, 12, r.w, view.noteIsError);
        ui.text(r.x, r.y + kFooterRowH + 8 + 15, ui.ellipsize(view.note, size, r.w, view.noteIsError), size,
                view.noteIsError ? kDangerText : kTextMuted, view.noteIsError);
    }
    return hits;
}

// ============================================================================
// コントラスト
// ============================================================================

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
 * WCAG 2.x の相対輝度。
 * @param c 色
 * @return 0〜1
 */
double luminance(Rgb c) {
    return 0.2126 * linearChannel(c.r) + 0.7152 * linearChannel(c.g) + 0.0722 * linearChannel(c.b);
}

}  // namespace

double contrastRatio(Rgb a, Rgb b) {
    const double la = luminance(a);
    const double lb = luminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

const std::vector<ContrastPair>& contrastPairs() {
    static const std::vector<ContrastPair> pairs = {
        // 文字（4.5:1）
        {"本文・見出し（パネルの地）", kText, kBg, 4.5},
        {"本文・見出し・値（カード）", kText, kCard, 4.5},
        {"説明（カード）", kTextMuted, kCard, 4.5},
        {"注意書き（パネルの地）", kTextMuted, kBg, 4.5},
        {"一番下の行の項目名（パネルの地）", kLabelSoft, kBg, 4.5},
        {"切り替えの選ばれていない側（カード）", kOptionText, kCard, 4.5},
        {"切り替えの選ばれていない側（乗っている）", kOptionText, kControlHover, 4.5},
        {"切り替えの選ばれていない側（押している）", kOptionText, kControlDown, 4.5},
        {"選択中・強調の文字（ピンクの塗り）", kOnAccent, kAccent, 4.5},
        {"選択中・強調の文字（乗っている）", kOnAccent, kAccentHover, 4.5},
        {"選択中・強調の文字（押している）", kOnAccent, kAccentDown, 4.5},
        {"普通のボタンの文字", kText, kControl, 4.5},
        {"普通のボタンの文字（押している）", kText, kControlDown, 4.5},
        {"危ないボタンの文字", kText, kDangerFill, 4.5},
        {"危ないボタンの文字（押している）", kText, kDangerDown, 4.5},
        {"「もう一度押すと終了」（赤い塗り）", kOnAccent, kDanger, 4.5},
        {"更新の帯のふだんの文", kTextSoft, kCard, 4.5},
        {"「新しい版があります」", kAccentText, kCard, 4.5},
        {"失敗の文字（カード）", kDangerText, kCard, 4.5},
        {"失敗の文字（パネルの地）", kDangerText, kBg, 4.5},
        {"状態ラベル 緑", kChipOkText, kChipOkFill, 4.5},
        {"状態ラベル 灰", kChipIdleText, kChipIdleFill, 4.5},
        {"状態ラベル 赤", kChipBadText, kChipBadFill, 4.5},
        // 部品の見分け（3:1。WCAG 1.4.11）
        {"選択中の塗り（カード）", kAccent, kCard, 3.0},
        {"選択中の塗り（パネルの地）", kAccent, kBg, 3.0},
        {"新しい版の枠（パネルの地）", kAccent, kBg, 3.0},
        {"危ないボタン・失敗の枠（カード）", kDanger, kCard, 3.0},
        {"危ないボタンの枠（パネルの地）", kDanger, kBg, 3.0},
        {"切り替え・増減・ボタンの枠（カード）", kBorder, kCard, 3.0},
        {"切り替え・増減・ボタンの枠（パネルの地）", kBorder, kBg, 3.0},
    };
    return pairs;
}

}  // namespace frame_ui
