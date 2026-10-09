// OpenVR との接続と、ダッシュボードのパネル（とサムネイル）。
#pragma once

#include "vk_texture.h"

#include <cstdint>
#include <string>
#include <vector>

/** ダッシュボードのパネルへのポインター操作（座標は画像の左上が原点の px）。 */
struct PointerInput {
    enum class Type {
        Move,
        Down,
        Up,
        Leave,
        Scroll,  ///< コントローラーのスティックのスクロール（y に縦の量。正で上）
    };
    Type type;
    double x = 0.0;
    double y = 0.0;
};

/** 自分のダッシュボードのオーバーレイが、まだ SteamVR にあるかの判定。 */
enum class OverlayHealth {
    Alive,     ///< ある（キーで探すと、自分の持っているハンドルが返る）
    Missing,   ///< 無い（キーで探しても見つからない。SteamVR の側で消えた）
    Replaced,  ///< キーは見つかるが、自分の持っているハンドルと違う（別のものがキーを持っている）
};

/**
 * FindOverlay の結果から判定する（OpenVR を呼ばない。--self-test で試せる）。
 * @param findOk FindOverlay が VROverlayError_None を返したか
 * @param found FindOverlay が返したハンドル
 * @param own 自分の持っているハンドル（0 = 持っていない）
 * @return 判定
 */
OverlayHealth judgeOverlay(bool findOk, uint64_t found, uint64_t own);

/** ensureOverlay() の結果。 */
enum class OverlayRepair {
    Ok,        ///< ある（何もしていない）
    Repaired,  ///< 消えていたので作り直した（画像を送り直すこと）
    Failed,    ///< 消えていて、作り直せなかった（次の確かめで再試行する）
};

/** pollEvents() の結果。 */
struct VrEvents {
    bool quit = false;                  ///< SteamVR から終了を求められた（VREvent_Quit。SteamVR 自体が終わる）
    bool closeRequested = false;        ///< ダッシュボードのアイコンの「閉じる」が押された（VREvent_OverlayClosed）
    std::vector<PointerInput> pointer;  ///< パネルへの操作（届いた順）
};

/**
 * OpenVR のオーバーレイアプリとしての接続をまとめたクラス。
 * SteamVR の設定は変えない（ダッシュボードにパネルを作って出すだけ）。
 * 画像は SetOverlayRaw ではなく Vulkan の画像を SetOverlayTexture で渡す。
 */
class VrOverlay {
public:
    /** connect() の結果。 */
    enum class ConnectResult {
        Ok,          ///< つながった
        NotRunning,  ///< SteamVR が起動していない（待って再試行する）
        Error,       ///< それ以外の失敗
    };

    VrOverlay();
    ~VrOverlay();
    VrOverlay(const VrOverlay&) = delete;
    VrOverlay& operator=(const VrOverlay&) = delete;

    /**
     * SteamVR が起動していればオーバーレイアプリとしてつなぎ、Vulkan とダッシュボードのパネルを用意する。
     * 起動していないときに SteamVR を立ち上げてしまわないよう、先に Background 型で有無を確かめる。
     * @param width パネルの画像の幅（px）
     * @param height パネルの画像の高さ（px）
     * @param message 失敗したときの理由
     * @return 接続結果
     */
    ConnectResult connect(int width, int height, std::string& message);

    /**
     * 安全な順番で片付ける: テクスチャを外す → オーバーレイを消す → コンポジタの数フレーム分待つ
     * → VR_Shutdown → Vulkan の画像とデバイスを壊す。各 API の戻り値はログに出す。
     */
    void shutdown();

    /**
     * たまったイベントを処理する。SteamVR の終了（VREvent_Quit）には AcknowledgeQuit_Exiting で応える。
     * @return 終了要求とパネルへの操作
     */
    VrEvents pollEvents();

    /**
     * 接続時に見つけた vrserver のプロセスがまだ生きているか。
     * @return 生きている（または確かめられない）なら true
     */
    bool steamVrAlive() const;

    /**
     * パネル（ダッシュボードでこのアプリを選んだ状態）が今見えているか。
     * @return 見えていれば true
     */
    bool panelVisible() const;

    /**
     * ダッシュボードを開いて、このアプリのパネルを出す（IVROverlay::ShowDashboard）。
     */
    void showPanel();

    /**
     * ダッシュボードのサムネイル画像を送る（接続直後に 1 回）。
     * @param rgba 非乗算済み RGBA
     * @param size 一辺の px
     * @return 送れたら true
     */
    bool submitThumbnail(const uint8_t* rgba, int size);

    /**
     * パネルの画像を送る。
     * @param rgba 非乗算済み RGBA 8bit（connect で指定した大きさ）
     * @return 送れたら true
     */
    bool submitPanel(const uint8_t* rgba);

    /**
     * 自己修復: 自分のダッシュボードのオーバーレイがまだ SteamVR にあるかを FindOverlay（軽い呼び出し 1 回）で確かめ、
     * 消えていれば（別のハンドルになっていれば）作り直す。古いハンドルへの Clear / Destroy はエラーでも無視する。
     * 別のプロセスが同じキーを持っていて作れない（KeyInUse）ときは、相手を消さずに Failed を返す。
     * 画像（パネル・サムネイル）の送り直しは呼び出し側が行う。
     * @return 結果
     */
    OverlayRepair ensureOverlay();

    /**
     * 確認用: キーからオーバーレイを探し直し、画像の大きさなどをログに出す。
     * @param when いつの確認か（ログ用）
     */
    void logOverlayState(const char* when) const;

    /**
     * 確認用（--probe）: Background 型でつなぎ、常駐しているインスタンスのダッシュボードのオーバーレイを
     * キーで探して、見えているか・フラグ・画像の大きさを標準出力に出す。オーバーレイも Vulkan も作らない。
     * @return 見つかったら 0、SteamVR が無い・見つからなければ 1
     */
    static int probe();

    /**
     * 確認用（--probe-switch-away）: 一時的な空のダッシュボードのオーバーレイを作って ShowDashboard で切り替え、
     * 常駐しているインスタンスの Mic のパネルが見えなくなる（閉じた）状態を作る。数秒後に消して終わる。
     * @param seconds 切り替えたままにする秒数
     * @return 成功なら 0
     */
    static int switchAway(double seconds);

private:
    bool connected_ = false;
    uint64_t dashboardHandle_ = 0;  ///< vr::VROverlayHandle_t（ダッシュボードのパネル）
    uint64_t thumbnailHandle_ = 0;  ///< ダッシュボードのサムネイル
    int panelWidth_ = 0;
    int panelHeight_ = 0;
    int vrserverPid_ = -1;
    std::string lastPanelError_;
    bool repairFailing_ = false;      ///< 作り直しに失敗している間 true（同じログを連打しない）
    std::string lastRepairMessage_;   ///< 最後にログに出した作り直しの失敗の理由

    /**
     * ダッシュボードのパネルとサムネイルのオーバーレイを作り、幅・入力・マウスの目盛り・閉じるボタンを設定する
     * （connect と作り直しで共通）。
     * @param message 失敗したときの理由
     * @return 作れたら true（dashboardHandle_ / thumbnailHandle_ に入る）
     */
    bool createDashboardOverlay(std::string& message);

    VulkanContext vulkan_;
    OverlayTexture panelTexture_;
    OverlayTexture thumbnailTexture_;

    /**
     * /proc を一度だけ走査して vrserver の PID を探す。
     * @return 見つかった PID。無ければ -1
     */
    static int findVrserverPid();
};
