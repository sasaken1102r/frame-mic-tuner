// 描画の共通部品（色・フォント・文字や角丸の描画・OpenVR 用の画素変換）。
#pragma once

#include <cairo.h>

#include <cstdint>
#include <string>
#include <vector>

/** RGB の色（0〜1）。 */
struct Color {
    double r, g, b;
};

// 色の定数は theme.h にまとめてある（コントラスト比の確認と同じ定義を使うため）

/**
 * 本文用と太字用の 2 つのフォント。一度読み込んで使い回す。
 */
class FontSet {
public:
    FontSet();
    ~FontSet();
    FontSet(const FontSet&) = delete;
    FontSet& operator=(const FontSet&) = delete;

    /**
     * フォントを読み込む（前と同じパスなら何もしない）。読めなければ fontconfig の "Noto Sans CJK JP" を使う。
     * @param regularPath 本文用フォントのパス（.ttc は 0 番 = 日本語）
     * @param boldPath 太字用フォントのパス
     */
    void load(const std::string& regularPath, const std::string& boldPath);

    /** @return 本文用フォント */
    cairo_font_face_t* regular() const { return regular_; }
    /** @return 太字用フォント */
    cairo_font_face_t* bold() const { return bold_; }
    /** @return 等幅フォント（本文用の .ttc の 5 番 = Noto Sans Mono CJK JP。インストーラーのコマンドの表示に使う） */
    cairo_font_face_t* mono() const { return mono_; }

private:
    void* ftLibrary_ = nullptr;  ///< FT_Library
    cairo_font_face_t* regular_ = nullptr;
    cairo_font_face_t* bold_ = nullptr;
    cairo_font_face_t* mono_ = nullptr;
    std::string regularPath_;
    std::string boldPath_;

    /**
     * フォントファイルから cairo のフォントを作る。
     * @param path フォントファイル
     * @param bold 代わりのフォントを太字にするか
     * @param index .ttc の何番目か（0 = 日本語）
     * @param fallback 読めないときに fontconfig で探す名前
     * @return 作ったフォント
     */
    cairo_font_face_t* createFace(const std::string& path, bool bold, long index = 0,
                                  const char* fallback = "Noto Sans CJK JP");

    /** 読み込んだフォントを手放す。 */
    void release();
};

/**
 * 描画中に使う道具（cairo とフォント）をまとめたもの。
 */
struct Pen {
    cairo_t* cr;
    const FontSet* fonts;

    /**
     * 色を設定する。
     * @param c 色
     * @param alpha 不透明度
     */
    void color(Color c, double alpha = 1.0) const { cairo_set_source_rgba(cr, c.r, c.g, c.b, alpha); }

    /**
     * 文字列の幅を測る。
     * @param text UTF-8 の文字列
     * @param size 文字の大きさ（px）
     * @param isBold 太字か
     * @return 幅（px）
     */
    double measure(const std::string& text, double size, bool isBold = false) const;

    /**
     * 文字を描く。
     * @param x 左端（alignRight なら右端）
     * @param y ベースライン
     * @param text UTF-8 の文字列
     * @param size 文字の大きさ（px）
     * @param c 色
     * @param isBold 太字にするか
     * @param alignRight 右揃えにするか
     * @return 描いた文字列の幅（px）
     */
    double text(double x, double y, const std::string& text, double size, Color c, bool isBold = false,
                bool alignRight = false) const;

    /**
     * 角の丸い四角の道筋を作る。
     * @param x 左
     * @param y 上
     * @param w 幅
     * @param h 高さ
     * @param r 角の半径
     */
    void roundedRect(double x, double y, double w, double h, double r) const;

    /**
     * 小さな色つきの丸（凡例用）を描く。
     * @param x 中心 x
     * @param y 中心 y
     * @param c 色
     */
    void dot(double x, double y, Color c) const;
};

/**
 * cairo の画像（乗算済み ARGB）を OpenVR 用の非乗算済み RGBA に変換する。
 * @param surface cairo の画像（CAIRO_FORMAT_ARGB32）
 * @param out 書き込み先（幅 * 高さ * 4 バイトに合わせる）
 */
void surfaceToRgba(cairo_surface_t* surface, std::vector<uint8_t>& out);
