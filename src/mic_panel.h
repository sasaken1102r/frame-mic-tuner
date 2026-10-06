// SteamVR ダッシュボードに出すマイクのパネル（描画・ボタンの当たり判定）と、サムネイル（アイコン）の絵。
// 色はすべて theme.h から取る（コントラスト比の確認と同じ定義）。
#pragma once

#include "config.h"
#include "mic_state.h"
#include "voice_check.h"

#include "update_check.h"

#include <cstdint>
#include <string>
#include <vector>

class FontSet;
struct Pen;
struct Color;
typedef struct _cairo cairo_t;
typedef struct _cairo_surface cairo_surface_t;

/** パネルのボタンが表す操作。 */
enum class PanelAction {
    None,
    Earphone,      ///< イヤホン（エコー除去オフ）
    Speaker,       ///< スピーカー（エコー除去オン）
    EchoOn,
    EchoOff,
    NsOn,
    NsOff,
    Record,        ///< 録音の開始・停止
    Play,          ///< 履歴の再生・停止（index = 履歴の何件目か。新しい順）
    LanguageJa,
    LanguageEn,
    AutostartOn,   ///< SteamVR と一緒に起動: オン
    AutostartOff,  ///< SteamVR と一緒に起動: オフ
    Quit,          ///< 終了（2 回目の押下で確定したときだけ返る）
    NsVadSlider,   ///< 判定の厳しさのバー（押したところへ飛び、そのままドラッグ）
    NsGraceSlider, ///< 余韻のバー
    NsVadMinus,    ///< 判定の厳しさ −1%
    NsVadPlus,     ///< 判定の厳しさ ＋1%
    NsGraceMinus,  ///< 余韻 −50ms
    NsGracePlus,   ///< 余韻 ＋50ms
    NsReset,       ///< 標準（SteamOS の既定 23% / 500ms）に戻す
    TabQuick,      ///< 「かんたん」のタブ
    TabFine,       ///< 「細かく調整」のタブ（かんたんのタブの「細かく調整を見る →」も。そちらは index = 1）
    UpdateCheckNow,  ///< 更新の帯の「今すぐ確かめる」（24 時間のキャッシュを無視してその場で確認。update_check が false でも押せる）
    UpdateInstall,   ///< 更新の帯の「更新する」（1 回目は確認の表示にするだけ。kQuitConfirmSec 以内の 2 回目で返る）
    UpdateRetry,     ///< 更新に失敗したあとの「もう一度」（確認なしでもう一度 install を頼む）
    UpdateDismiss,   ///< 「入れました」「更新できませんでした」の表示を閉じる
    UpdateCancel,    ///< 確認の表示の「やめる」（パネルの中で確認を取り消すだけ。呼び出し側は何もしない）
    Unmute,          ///< ミュートの帯の「ミュートを解除」（確認なしで解除する）
};

/** 押されたボタン（操作と、履歴なら何件目か）。 */
struct PanelHit {
    PanelAction action = PanelAction::None;
    int index = 0;

    /** @return 同じボタンなら true */
    bool operator==(const PanelHit& other) const { return action == other.action && index == other.index; }
    /** @return 違うボタンなら true */
    bool operator!=(const PanelHit& other) const { return !(*this == other); }
};

/**
 * パネルの画像を描き、レーザーポインターの位置からボタンを判定する係。
 */
class MicPanel {
public:
    /** 「終了」を 1 回押してから、確定の 2 回目を待つ秒数。 */
    static constexpr double kQuitConfirmSec = 3.0;

    /**
     * @param fonts 使うフォント（このオブジェクトより長く生きていること）
     */
    explicit MicPanel(const FontSet& fonts);
    ~MicPanel();
    MicPanel(const MicPanel&) = delete;
    MicPanel& operator=(const MicPanel&) = delete;

    /**
     * 今の設定・マイクの状態・声のチェックの状態と、ポインターが乗っている・押しているボタンでパネルを描く。
     * ボタンの位置もここで作り直す（履歴の件数や言語で変わるため）。
     * @param config 今の設定（言語）
     * @param state マイクの状態
     * @param voice 声のチェックの状態
     * @param update 新しい版の確認・更新の状態（既定値のままなら「まだ確かめていない」の表示になる）
     */
    void render(const Config& config, const MicState& state, const VoiceView& voice,
               const frame_updater::UpdateStatus& update = {});

    /**
     * ポインターが動いた。
     * @param x 左端からの px
     * @param y 上端からの px
     * @return 乗っているボタンが変わった（描き直しが要る）なら true
     */
    bool pointerMove(double x, double y);

    /**
     * ボタンが押された。「終了」は 1 回目で確認の表示に変わり、kQuitConfirmSec 秒以内の 2 回目で Quit を返す。
     * 押せない状態のボタン（自動起動の準備が無いとき）は None。
     * @param x 左端からの px
     * @param y 上端からの px
     * @param now 現在時刻（秒、単調増加）
     * @return 押されたボタン（ボタンの外・終了の 1 回目なら None）
     */
    PanelHit pointerDown(double x, double y, double now);

    /**
     * ボタンが離された。
     * @return 押している表示を消した（描き直しが要る）なら true
     */
    bool pointerUp();

    /**
     * ポインターがパネルから外れた。
     * @return 描き直しが要るなら true
     */
    bool pointerLeave();

    /**
     * 時間で変わる表示（終了の確認の期限切れ）を進める。
     * @param now 現在時刻（秒、単調増加）
     * @return 描き直しが要るなら true
     */
    bool tick(double now);

    /**
     * 描いた画像を OpenVR 用の非乗算済み RGBA にして返す。
     * @return width() * height() * 4 バイト
     */
    const std::vector<uint8_t>& toRgba();

    /**
     * 描いた画像を PNG で保存する。
     * @param path 保存先
     * @return 保存できたら true
     */
    bool writePng(const std::string& path) const;

    /**
     * 見た目の確認用に「もう一度押すと終了」の状態にする（--dump-png 用）。
     */
    void armQuitForPreview();

    /**
     * 見た目の確認用に、更新の「確認しますか？」の状態にする（--dump-png 用）。
     */
    void armUpdateForPreview();

    /**
     * 見た目の確認用に、ポインターが乗っている・押しているボタンを決める（--dump-png 用）。
     * @param hover 乗っているボタン
     * @param pressed 押しているボタン
     */
    void setPointerForPreview(PanelHit hover, PanelHit pressed);

    /**
     * 見た目の確認用に、バーをドラッグしている状態にする（--dump-png 用）。
     * @param action NsVadSlider か NsGraceSlider
     * @param value ドラッグ中の値
     */
    void setDragForPreview(PanelAction action, double value);

    /** @return バーをドラッグしている間 true */
    bool dragging() const { return dragAction_ != PanelAction::None; }
    /** @return ドラッグしているバー（NsVadSlider / NsGraceSlider） */
    PanelAction dragAction() const { return dragAction_; }
    /** @return ドラッグ中の値（範囲に丸め済み） */
    double dragValue() const { return dragValue_; }

    /**
     * バーの値を書き込みに出した直後、読み直しが追いつくまでの間、その値を表示に使う（表示が戻ってちらつかないように）。
     * @param vad 判定の厳しさ（%）
     * @param grace 余韻（ms）
     * @param until この時刻（秒、単調増加）まで
     */
    void holdNsValues(double vad, double grace, double until);

    /**
     * プリセットを書き込んでいる間、押したカードを選択中の見た目で保つ（読み直しが終わるまでグレーに戻さない）。
     * @param action Earphone か Speaker
     * @param until この時刻（秒、単調増加）まで（書き込みが終わったら clearPresetHold() で早めに外す）
     */
    void holdPreset(PanelAction action, double until);

    /** プリセットの書き込みが終わったので、選択中の見た目を実際の値に戻す。 */
    void clearPresetHold() { presetHold_ = PanelAction::None; }

    /** @return プリセットの見た目を保っている間 true */
    bool presetHeld() const { return presetHold_ != PanelAction::None; }

    /**
     * 今バーに出している値（ドラッグ中ならその値、書いた直後なら書いた値、ほかは読んだ値。読めなければ SteamOS の既定）。
     * @param state マイクの状態
     * @param vad 判定の厳しさ（%）の書き込み先
     * @param grace 余韻（ms）の書き込み先
     */
    void displayedNsValues(const MicState& state, double& vad, double& grace) const;

    /**
     * 確認用（--self-test）: 最後に描いたときのバーの溝の真ん中の座標。
     * @param action NsVadSlider か NsGraceSlider
     * @param x 左端からの px の書き込み先
     * @param y 上端からの px の書き込み先
     * @return 描いていれば true
     */
    bool trackCenter(PanelAction action, double& x, double& y) const;

    /**
     * 確認用（--self-test）: 最後に描いたときのボタンの真ん中の座標。
     * @param action ボタンの操作（index 0 のもの）
     * @param x 左端からの px の書き込み先
     * @param y 上端からの px の書き込み先
     * @return 描いていれば true
     */
    bool buttonCenter(PanelAction action, double& x, double& y) const;

    /** @return 画像の幅（px） */
    int width() const;
    /** @return 画像の高さ（px） */
    int height() const;

private:
    /** ボタン 1 個の当たり判定。 */
    struct Button {
        PanelHit hit;
        double x, y, w, h;
        bool usable;
    };

    const FontSet& fonts_;
    cairo_surface_t* surface_ = nullptr;
    cairo_t* cr_ = nullptr;
    std::vector<uint8_t> rgba_;
    std::vector<Button> buttons_;  ///< 最後に描いたときの配置
    /** バーの溝の位置（当たり判定とドラッグの値の計算用）。 */
    struct Track {
        PanelAction action;
        double x0, x1;      ///< 溝の左端・右端（px）
        double min, max;    ///< 値の範囲
        double cy;          ///< 溝の縦の真ん中（px）
    };

    PanelHit hover_;
    PanelHit pressed_;
    bool quitArmed_ = false;
    double quitArmedUntil_ = 0.0;
    bool updateArmed_ = false;      ///< 「更新する」の 1 回目が押された（2 回目の確認待ち）
    double updateArmedUntil_ = 0.0;
    double now_ = 0.0;              ///< tick() で知らされた時刻
    std::vector<Track> tracks_;     ///< 最後に描いたときのバーの溝
    PanelAction dragAction_ = PanelAction::None;
    double dragValue_ = 0.0;
    double holdVad_ = 0.0;
    double holdGrace_ = 0.0;
    double holdUntil_ = -1.0;
    PanelAction presetHold_ = PanelAction::None;  ///< 書き込み中のプリセット（その間は選択中の見た目で保つ）
    double presetHoldUntil_ = -1.0;

    /**
     * バーの溝の上の x から値を出す（範囲に丸める）。
     * @param track 溝
     * @param x 左端からの px
     * @return 値
     */
    static double trackValue(const Track& track, double x);

    /**
     * 座標にある押せるボタンを探す。
     * @param x 左端からの px
     * @param y 上端からの px
     * @return ボタン。無ければ action = None
     */
    PanelHit hitTest(double x, double y) const;

    /**
     * ボタンの当たり判定を登録する。
     * @param action 操作
     * @param index 履歴なら何件目か
     * @param x 左
     * @param y 上
     * @param w 幅
     * @param h 高さ
     * @param usable 押せるか
     */
    void addButton(PanelAction action, int index, double x, double y, double w, double h, bool usable = true);

    /**
     * そのボタンの今の見た目の状態。
     * @param action 操作
     * @param index 履歴なら何件目か
     * @return 0 = ふつう、1 = 乗っている、2 = 押している
     */
    int pointerState(PanelAction action, int index = 0) const;

    /** 見出しと使用中のバッジ。 */
    void drawHeader(const Pen& pen, const UiText& t, const MicState& state);
    /** イヤホン・スピーカーの大きなカード。 */
    void drawModeCards(const Pen& pen, const UiText& t, const MicState& state, double y);
    /** エコー除去・ノイズ除去のスライド式の切り替え。 */
    void drawToggleRows(const Pen& pen, const UiText& t, const MicState& state, double y);
    /** 左の列の上のタブ（かんたん / 細かく調整）。 */
    void drawTabs(const Pen& pen, const UiText& t, PanelTab tab);
    /** ノイズ除去の強さのバー 2 本（判定の厳しさ・余韻）と「標準に戻す」。 */
    void drawNsSliders(const Pen& pen, const UiText& t, const MicState& state, double y);
    /** つながりのパイプライン図。 */
    void drawPipeline(const Pen& pen, const UiText& t, const MicState& state, double y);
    /** 声のチェック（録音ボタン・メーター・履歴）。 */
    void drawVoice(const Pen& pen, const UiText& t, const VoiceView& voice, double y);
    /** 下の 1 行（言語・自動起動・終了）と最下行の説明・失敗。 */
    void drawFooter(const Pen& pen, const UiText& t, const MicState& state, Language language, double y);
    /**
     * 更新の帯（カード。左に状態の文、右にボタン。新しい版・確認・入れ終わりはアクセントの枠、更新の失敗は赤い枠）。
     * vendor/frame-updater の状態を表示する。
     */
    void drawUpdateRow(const Pen& pen, const UiText& t, const frame_updater::UpdateStatus& update, double y);
    /**
     * ミュートの帯（ミュート中だけ、更新の帯の場所に出す。赤い地と枠に、マイクに斜線の絵・文・「ミュートを解除」）。
     */
    void drawMuteRow(const Pen& pen, const UiText& t, double y);

    /**
     * 2 つの選択肢を 1 本のピルに並べ、選択中の側に塗りを置く（スライド式）。
     * @param pen 描画の道具
     * @param x 左
     * @param y 上
     * @param w 幅
     * @param h 高さ
     * @param labels 左右の文言
     * @param actions 左右の操作
     * @param selected 選択中の側（0 / 1、分からなければ -1）
     * @param size 文字の大きさ
     * @param usable 押せるか
     */
    void drawSegmented(const Pen& pen, double x, double y, double w, double h, const std::string labels[2],
                       const PanelAction actions[2], int selected, double size, bool usable = true);
};

/**
 * 失敗の種類を画面の文言にする。
 * @param error 失敗の種類
 * @param text 言語の表
 * @return 文言（None なら空）
 */
std::string errorText(MicError error, const UiText& text);

/**
 * 更新の失敗の理由（UpdateStatus::error）を画面の文言にする。知らない理由は updateErrOther。
 * @param error 失敗の種類（"network" など。strings.md の一覧）
 * @param text 言語の表
 * @return 文言
 */
std::string updateErrorText(const std::string& error, const UiText& text);

/**
 * ダッシュボードの下に並ぶサムネイル（アイコン）を描く。言語によらず同じ（マイクの絵と「Mic」の文字）。
 * ＋（プログラムを起動）のアイコン（contrib/icons）も同じ絵。
 * @param fonts 使うフォント
 * @param size 一辺の px
 * @param rgba 非乗算済み RGBA の書き込み先
 * @param pngPath 空でなければ PNG にも保存する
 */
void renderThumbnail(const FontSet& fonts, int size, std::vector<uint8_t>& rgba, const std::string& pngPath = "");
