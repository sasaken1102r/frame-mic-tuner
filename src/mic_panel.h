// SteamVR ダッシュボードに出すマイクのパネル（描画・ボタンの当たり判定）と、サムネイル（アイコン）の絵。
// 部品（切り替え・ボタン・カード・一番下の行）は共通の UI 部品 frame-ui で描き、色は theme.h（frame-ui と同じ）から取る。
// 画面は 2 つ（マイクの設定 / アプリと更新）と、重ねて出す画面（音の出口を選ぶ・使うマイク）。配置は見本（1200×788）のとおり。
#pragma once

#include "config.h"
#include "mic_state.h"
#include "voice_check.h"

#include "frame_apps.h"
#include "frame_ui.h"
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
    // ---- 見出し ----
    MuteToggle,       ///< 「ミュートする」/「ミュートを解除」（確認なし。今の状態の逆にする）
    ShowApps,         ///< 「アプリと更新」（パネルの中で画面を切り替える）
    ShowSettings,     ///< 「← マイクの設定へ」
    // ---- マイクの設定 ----
    OpenOutputPicker, ///< 「音の出口」の欄（音の出口を選ぶ画面を重ねる）
    OpenMicPicker,    ///< 「マイク」の欄（使うマイクの画面を重ねる）
    TabQuick,         ///< 「かんたん」
    TabFine,          ///< 「細かく調整」
    Earphone,         ///< プリセット: イヤホン・ヘッドホン（エコー除去オフ・ノイズ除去オフ）
    Speaker,          ///< プリセット: Frame のスピーカー（エコー除去オン・ノイズ除去オフ）
    EchoOn,
    EchoOff,
    NsOn,
    NsOff,
    NsVadSlider,      ///< 判定の厳しさのバー（押したところへ飛び、そのままドラッグ）
    NsGraceSlider,    ///< 余韻のバー
    NsReset,          ///< 標準（SteamOS の既定 23% / 500ms）に戻す
    UndoSwitch,       ///< 自動で切り替えた知らせの「元に戻す」
    UseBuiltinMic,    ///< 「Frame 内蔵マイクに戻す」
    Record,           ///< 録音の開始・停止
    Play,             ///< 履歴の再生・停止（index = 履歴の何件目か。新しい順）
    // ---- 重ねた画面 ----
    CloseOverlay,     ///< ✕（パネルの中で閉じる）
    OutputView,       ///< 出口の行: その出口の設定を表示する（key。パネルの中で切り替えて閉じる）
    OutputUse,        ///< 「ここから音を出す」（key。既定の出力にする）
    OutputForget,     ///< 「忘れる」（key。つながっていない出口の設定を消す）
    MicUse,           ///< 「このマイクを使う」（key。既定の入力にする）
    // ---- アプリと更新 ----
    UpdateCheckNow,   ///< 「今すぐ確かめる」
    UpdateInstall,    ///< 「更新する」の 1 回目（パネルの中で確認の表示にするだけ）
    UpdateConfirmYes, ///< 確認の「更新する」
    UpdateConfirmNo,  ///< 確認の「やめる」（パネルの中で取り消すだけ）
    UpdateRetry,      ///< 失敗のあとの「もう一度」
    UpdateDismiss,    ///< 「閉じる」
    UpdateCheckOn,    ///< 「新しい版の確認」オン
    UpdateCheckOff,   ///< 「新しい版の確認」オフ
    AppsOpenList,     ///< 「ささけんの Frame アプリ」の行（一覧を重ねる）
    AppInstall,       ///< 「入れる」（key = インストーラーでの呼び名。パネルの中で確認の画面を出す）
    AppsOpenMenu,     ///< 「インストーラーを開く」（パネルの中で確認の画面を出す）
    AppsConfirmCancel,///< 確認の「やめる」（パネルの中で戻る）
    AppsConfirmLaunch,///< 確認の「Konsole で開く」（key = 呼び名。空ならメニュー）
    // ---- 一番下の行 ----
    LanguageJa,
    LanguageEn,
    LanguageSc,
    AutostartOn,      ///< SteamVR と一緒に起動: オン
    AutostartOff,     ///< SteamVR と一緒に起動: オフ
    Quit,             ///< 終了（2 回目の押下で確定したときだけ返る）
};

/** 押されたボタン（操作と、履歴なら何件目か、一覧の行ならその出口・マイクのキー）。 */
struct PanelHit {
    PanelAction action = PanelAction::None;
    int index = 0;
    std::string key {};

    /** @return 同じボタンなら true */
    bool operator==(const PanelHit& other) const {
        return action == other.action && index == other.index && key == other.key;
    }
    /** @return 違うボタンなら true */
    bool operator!=(const PanelHit& other) const { return !(*this == other); }
};

/** パネルの画面。 */
enum class PanelView {
    Settings,  ///< マイクの設定
    Apps,      ///< アプリと更新
};

/** 重ねて出す画面。 */
enum class PanelOverlay {
    None,
    OutputPicker,  ///< 音の出口を選ぶ
    MicPicker,     ///< 使うマイク
    AppsList,      ///< ささけんの Frame アプリ（アプリと更新の画面から）
    AppsConfirm,   ///< 入れる前の確認（実行するコマンドを見せる）
};

/** 描くときに渡す、今の状態（メインが作る）。 */
struct PanelModel {
    MicState state;                          ///< ワーカーの状態の写し
    VoiceView voice;                         ///< 声のチェック
    frame_updater::UpdateStatus update {};   ///< 新しい版の確認・更新
    std::string activeOutputKey;             ///< 今の出口（設定をかけている出口。空なら一覧の既定）
    bool switchNotice = false;               ///< 自動で切り替えた知らせを出すか
    std::string switchKey;                   ///< その出口
    std::vector<frame_apps::Entry> apps;     ///< ささけんのほかのアプリ（使うものが先、自分は入っていない）
    bool menuBusy = false;                   ///< インストーラーのメニューの Konsole がまだ開いている
};

/**
 * パネルの画像を描き、レーザーポインターの位置からボタンを判定する係。
 */
class MicPanel {
public:
    /** 「終了」「更新する」を 1 回押してから、確定の 2 回目を待つ秒数。 */
    static constexpr double kQuitConfirmSec = 3.0;
    /** なめらかなスクロールの 1 の量で動かす一覧の量（px）。 */
    static constexpr double kScrollPxPerUnit = 120;

    /**
     * @param fonts 使うフォント（このオブジェクトより長く生きていること）
     */
    explicit MicPanel(const FontSet& fonts);
    ~MicPanel();
    MicPanel(const MicPanel&) = delete;
    MicPanel& operator=(const MicPanel&) = delete;

    /**
     * 今の設定と状態でパネルを描く。ボタンの位置もここで作り直す。
     * @param config 今の設定（言語・タブ・新しい版の確認・出口ごとの設定）
     * @param model 今の状態
     */
    void render(const Config& config, const PanelModel& model);

    /**
     * ポインターが動いた。
     * @param x 左端からの px
     * @param y 上端からの px
     * @return 描き直しが要るなら true
     */
    bool pointerMove(double x, double y);

    /**
     * ボタンが押された。「終了」「更新する」は 1 回目で確認の表示に変わり、2 回目で返す。
     * 一覧の中（行とその中のボタン）は、ドラッグでスクロールできるよう離したときに返す（pointerUp）。
     * パネルの中だけで済む操作（画面の切り替え・重ねた画面を開く・閉じる）は、ここで状態を変えてから返す。
     * @param x 左端からの px
     * @param y 上端からの px
     * @param now 現在時刻（秒、単調増加）
     * @return 押されたボタン（何も無い・確認の 1 回目・一覧の中なら None）
     */
    PanelHit pointerDown(double x, double y, double now);

    /**
     * ボタンが離された。一覧の中を押してドラッグせずに離したら、その行・ボタンを返す。
     * @return 一覧の中で押されたボタン（ほかは None）
     */
    PanelHit pointerUp();

    /**
     * ポインターがパネルから外れた。
     * @return 描き直しが要るなら true
     */
    bool pointerLeave();

    /**
     * コントローラーのスティックのスクロール（VREvent_ScrollSmooth / ScrollDiscrete）。重ねた画面の一覧を動かす。
     * @param dy 縦の量（正で上）
     * @return 動いたら true
     */
    bool scroll(double dy);

    /**
     * 時間で変わる表示（終了・更新の確認の期限切れ、書いた値の表示の期限）を進める。
     * @param now 現在時刻（秒、単調増加）
     * @return 描き直しが要るなら true
     */
    bool tick(double now);

    /** パネルが閉じた: マイクの設定の画面・今の出口の表示に戻し、重ねた画面を閉じる。 */
    void resetView();

    /** 今の出口が変わった（自動の切り替え・ここから音を出す）: 設定の表示を今の出口に戻す。 */
    void followActiveOutput() { viewKey_.clear(); }

    /**
     * Konsole を開いた結果を知らせる。開けたら確認の画面を閉じ（一覧から開いたなら一覧に戻る）、開けなければ理由を出す。
     * @param result AppsManager::openInstaller の結果
     */
    void setLaunchResult(frame_apps::LaunchResult result);

    /** @return 今の画面 */
    PanelView view() const { return view_; }
    /** @return 重ねて出している画面 */
    PanelOverlay overlay() const { return overlay_; }
    /** @return 最後に描いたときに設定を表示していた出口のキー */
    const std::string& viewedOutputKey() const { return viewedKey_; }
    /** @return 最後に描いたとき、表示していた出口が今の出口（設定をかけている出口）だったか */
    bool viewingActive() const { return viewingActive_; }

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

    /** @return バーをドラッグしている間 true */
    bool dragging() const { return dragAction_ != PanelAction::None; }

    /**
     * バーの値を書き込みに出した直後、読み直しが追いつくまでの間、その値を表示に使う（表示が戻ってちらつかないように）。
     * @param vad 判定の厳しさ（%）
     * @param grace 余韻（ms）
     * @param until この時刻（秒、単調増加）まで
     */
    void holdNsValues(double vad, double grace, double until);

    /**
     * プリセットを書き込んでいる間、押したカードを選択中の見た目で保つ（読み直しが終わるまで戻さない）。
     * @param action Earphone か Speaker
     * @param until この時刻（秒、単調増加）まで（書き込みが終わったら clearPresetHold() で早めに外す）
     */
    void holdPreset(PanelAction action, double until);

    /** プリセットの書き込みが終わったので、選択中の見た目を実際の値に戻す。 */
    void clearPresetHold() { presetHold_ = PanelAction::None; }

    /** @return プリセットの見た目を保っている間 true */
    bool presetHeld() const { return presetHold_ != PanelAction::None; }

    /**
     * 今バーに出している値（ドラッグ中ならその値、書いた直後なら書いた値、ほかは最後に描いたときの値）。
     * @param vad 判定の厳しさ（%）の書き込み先
     * @param grace 余韻（ms）の書き込み先
     */
    void displayedNsValues(double& vad, double& grace) const;

    /**
     * バーのドラッグを終わらせる（押している見た目も消す）。
     * @return ドラッグしていたら true
     */
    bool endDrag();

    /**
     * 確認用（--self-test）: 最後に描いたときのバーの溝の真ん中の座標。
     * @param action NsVadSlider か NsGraceSlider
     * @param x 書き込み先
     * @param y 書き込み先
     * @return 描いていれば true
     */
    bool trackCenter(PanelAction action, double& x, double& y) const;

    /**
     * 確認用（--self-test・--preview-*）: 最後に描いたときのボタンの真ん中の座標。
     * @param action ボタンの操作
     * @param x 書き込み先
     * @param y 書き込み先
     * @param key 一覧の行なら、そのキー（空なら最初に見つかったもの）
     * @return 描いていれば true
     */
    bool buttonCenter(PanelAction action, double& x, double& y, const std::string& key = "") const;

    /**
     * 確認用（--self-test）: 最後に描いたときのボタンの矩形。
     * @param action ボタンの操作
     * @return 矩形（無ければ空）
     */
    frame_ui::Rect buttonRect(PanelAction action) const;

    // ---- 見た目の確認用（--dump-png） ----
    /** 「もう一度押すと終了」の状態にする。 */
    void armQuitForPreview();
    /** 更新の確認（やめる・更新する）の状態にする。 */
    void armUpdateForPreview();
    /**
     * ポインターを置く。
     * @param x x
     * @param y y
     * @param down 押しているか
     */
    void setPointerForPreview(double x, double y, bool down);
    /**
     * バーをドラッグしている状態にする。
     * @param action NsVadSlider か NsGraceSlider
     * @param value ドラッグ中の値
     */
    void setDragForPreview(PanelAction action, double value);
    /**
     * 画面と重ねた画面を決める。
     * @param view 画面
     * @param overlay 重ねた画面
     */
    void showForPreview(PanelView view, PanelOverlay overlay);
    /**
     * 一覧のスクロールを決める（大きければいちばん下）。
     * @param px スクロール（px）
     */
    void setListScrollForPreview(double px) { listScroll_ = px; }
    /**
     * 設定を表示する出口を決める。
     * @param key 出口のキー
     */
    void setViewedOutputForPreview(const std::string& key) { viewKey_ = key; }
    /**
     * 入れる前の確認の画面を出す。
     * @param key 呼び名（空ならメニュー）
     * @param fromList 一覧から開いたか（やめると一覧に戻る）
     */
    void openConfirmForPreview(const std::string& key, bool fromList);

    /** @return 画像の幅（px） */
    int width() const;
    /** @return 画像の高さ（px） */
    int height() const;

private:
    /** ボタン 1 個の当たり判定。 */
    struct Button {
        PanelHit hit;
        frame_ui::Rect rect;
        bool inList;  ///< 重ねた画面の一覧の中（離したときに返す）
    };
    /** バーの溝の位置（当たり判定とドラッグの値の計算用）。 */
    struct Track {
        PanelAction action;
        double x0, x1;      ///< 溝の左端・右端（px）
        double min, max;    ///< 値の範囲
        double cy;          ///< 溝の縦の真ん中（px）
    };

    const FontSet& fonts_;
    cairo_surface_t* surface_ = nullptr;
    cairo_t* cr_ = nullptr;
    cairo_surface_t* appIcon_ = nullptr;  ///< このアプリのアイコン（サムネイルと同じ絵。アプリと更新の画面）
    std::vector<uint8_t> rgba_;
    std::vector<Button> buttons_;  ///< 最後に描いたときの配置
    std::vector<Track> tracks_;    ///< 最後に描いたときのバーの溝

    frame_ui::Pointer pointer_;    ///< レーザーの今（乗っている・押している見た目は frame-ui の部品がこれを見て決める）
    PanelHit hover_;               ///< 乗っているボタン（描き直しの判定用）
    PanelView view_ = PanelView::Settings;
    PanelOverlay overlay_ = PanelOverlay::None;
    std::string viewKey_;          ///< 選んで設定を表示している出口（空 = 今の出口）
    std::string viewedKey_;        ///< 最後に描いたときに表示していた出口
    bool viewingActive_ = true;
    bool quitArmed_ = false;
    double quitArmedUntil_ = 0.0;
    bool updateArmed_ = false;     ///< 「更新する」の 1 回目が押された（確認の表示）
    double updateArmedUntil_ = 0.0;
    double now_ = 0.0;             ///< tick() で知らされた時刻
    PanelAction dragAction_ = PanelAction::None;
    double dragValue_ = 0.0;
    double baseVad_ = 23.0;        ///< 最後に描いたときのバーの元の値（今の出口なら読んだ値、ほかは覚えた値）
    double baseGrace_ = 500.0;
    double holdVad_ = 0.0;
    double holdGrace_ = 0.0;
    double holdUntil_ = -1.0;
    PanelAction presetHold_ = PanelAction::None;  ///< 書き込み中のプリセット（その間は選択中の見た目で保つ）
    double presetHoldUntil_ = -1.0;
    // 重ねた画面の一覧のスクロール（スティック・ドラッグ）
    frame_ui::Rect listRect_;       ///< 一覧の見えている枠（最後に描いたとき）
    double listScroll_ = 0.0;
    double listMaxScroll_ = 0.0;
    bool listTracking_ = false;     ///< 一覧の中を押している
    bool listDragging_ = false;     ///< 押したまま動かしてスクロールしている
    PanelHit listPress_;            ///< 押した行・ボタン（ドラッグせずに離したら返す）
    double listStartY_ = 0.0;
    double listStartScroll_ = 0.0;
    // ほかのアプリを入れる前の確認
    std::string confirmKey_;                         ///< 入れるアプリの呼び名（空ならメニュー）
    PanelOverlay confirmBack_ = PanelOverlay::None;  ///< やめたときに戻る画面（一覧から開いたなら一覧）
    frame_apps::LaunchResult launchResult_ = frame_apps::LaunchResult::Started;  ///< 最後に開けなかった理由（Started なら無し）

    /**
     * バーの溝の上の x から値を出す（範囲に丸める）。
     * @param track 溝
     * @param x 左端からの px
     * @return 値
     */
    static double trackValue(const Track& track, double x);

    /**
     * 座標にある押せるボタンを探す（後から描いたもの = 上にあるものが先）。
     * @param x 左端からの px
     * @param y 上端からの px
     * @param inList 一覧の中のボタンなら true にする
     * @return ボタン。無ければ action = None
     */
    PanelHit hitTest(double x, double y, bool* inList = nullptr) const;

    /**
     * ボタンの当たり判定を登録する（空の矩形は登録しない）。
     * @param hit 操作
     * @param rect 矩形
     * @param inList 一覧の中か
     */
    void addButton(const PanelHit& hit, frame_ui::Rect rect, bool inList = false);

    /**
     * 矩形の上のポインターの見た目。
     * @param rect 矩形
     * @return 0 = ふつう、1 = 乗っている、2 = 押している
     */
    int look(frame_ui::Rect rect) const;

    /** パネルの中だけで済む操作を扱う（画面・重ねた画面・表示する出口）。 */
    void applyLocal(const PanelHit& hit);

    // ---- 描く ----
    void drawHeader(const frame_ui::Canvas& ui, const UiText& t, const PanelModel& model);
    void drawSettings(const frame_ui::Canvas& ui, const UiText& t, const Config& config, const PanelModel& model);
    void drawSelectors(const frame_ui::Canvas& ui, const UiText& t, const Config& config, const PanelModel& model);
    void drawQuick(const frame_ui::Canvas& ui, const UiText& t, const Config& config, const PanelModel& model,
                   bool echo, bool ns, bool known, const std::string& outputName);
    void drawFine(const frame_ui::Canvas& ui, const UiText& t, const PanelModel& model, bool echo, bool ns, bool known,
                  bool nsParamsKnown);
    void drawExternal(const frame_ui::Canvas& ui, const UiText& t, const PanelModel& model, bool echo, bool ns,
                      const std::string& outputName);
    void drawPipeline(const frame_ui::Canvas& ui, const UiText& t, const MicState& state, bool external);
    void drawVoice(const frame_ui::Canvas& ui, const UiText& t, const VoiceView& voice);
    void drawApps(const frame_ui::Canvas& ui, const UiText& t, const Config& config, const PanelModel& model);
    void drawUpdateBox(const frame_ui::Canvas& ui, const UiText& t, frame_ui::Lang lang, const Config& config,
                       const frame_updater::UpdateStatus& update, frame_ui::Rect box);
    void drawFooter(const frame_ui::Canvas& ui, const UiText& t, const Config& config, const MicState& state);
    void drawOutputPicker(const frame_ui::Canvas& ui, const UiText& t, const Config& config, const PanelModel& model);
    void drawMicPicker(const frame_ui::Canvas& ui, const UiText& t, const PanelModel& model);
    /** アプリと更新の画面の右のカード（いっしょに使うアプリと、一覧を開く行）。 */
    void drawRelatedApps(const frame_ui::Canvas& ui, const UiText& t, const Config& config, const PanelModel& model);
    /**
     * アプリの行 1 つ（アイコン・名前・説明か理由・右に「入れる」か状態の札）。
     * @param inList 重ねた一覧の中か（「このアプリで使う」の札を出し、ボタンは離したときに返す）
     */
    void drawAppRow(const frame_ui::Canvas& ui, const UiText& t, Language language, const frame_apps::Entry& entry,
                    frame_ui::Rect row, bool inList);
    void drawAppsList(const frame_ui::Canvas& ui, const UiText& t, const Config& config, const PanelModel& model);
    void drawAppsConfirm(const frame_ui::Canvas& ui, const UiText& t, const Config& config, const PanelModel& model);
    /**
     * 重ねた画面の地（後ろを暗くし、枠のあるカード）と見出し・✕ を描く。
     * @return 中身を置き始める y
     */
    double drawModal(const frame_ui::Canvas& ui, frame_ui::Rect r, const std::string& title, const std::string& sub);
    /**
     * スクロールする一覧の枠を決める（中身の高さから上限を出し、スクロールを範囲に収める）。
     * @param box 見えている枠
     * @param contentH 中身の高さ
     */
    void beginList(frame_ui::Rect box, double contentH);
    /** 一覧のスクロールバーと、下に続きがあるときの下の端のぼかしを描く。 */
    void endList(const frame_ui::Canvas& ui, double contentH, double barX);
};

/**
 * 失敗の種類を画面の文言にする。
 * @param error 失敗の種類
 * @param text 言語の表
 * @return 文言（None なら空）
 */
std::string errorText(MicError error, const UiText& text);

/**
 * このアプリの言語を frame-ui の言語にする。
 * @param language 言語
 * @return frame-ui の言語
 */
frame_ui::Lang uiLang(Language language);

/**
 * 出口の表示名（Frame のスピーカーは言語の表の名前、ほかは機器の名前）。
 * @param key 出口のキー
 * @param name 機器の名前（覚えた名前）
 * @param t 言語の表
 * @return 表示名
 */
std::string outputDisplayName(const std::string& key, const std::string& name, const UiText& t);

/**
 * ダッシュボードの下に並ぶサムネイル（アイコン）を描く。言語によらず同じ（マイクの絵と「Mic」の文字）。
 * ＋（プログラムを起動）のアイコン（contrib/icons）と、アプリと更新の画面のアイコンも同じ絵。
 * @param fonts 使うフォント
 * @param size 一辺の px
 * @param rgba 非乗算済み RGBA の書き込み先
 * @param pngPath 空でなければ PNG にも保存する
 */
void renderThumbnail(const FontSet& fonts, int size, std::vector<uint8_t>& rgba, const std::string& pngPath = "");
