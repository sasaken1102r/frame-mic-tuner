// 外部コマンドの実行をまとめて引き受けるワーカースレッド。
// メインスレッド（ポインターへの応答 33ms おき）を止めないよう、読み書きはすべてここで行い、
// メインは mutex で状態の写しを読むだけにする。
#pragma once

#include "mic_state.h"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

/** ワーカーに頼む書き込み。 */
struct MicCommand {
    enum class Kind {
        SetEcho,           ///< エコー除去（value = オン）
        SetGain,           ///< エコー除去の自動音量調整（value = オン）
        SetNs,             ///< ノイズ除去（value = オン）。--print の --set-ns だけが使う（パネルは SetNoiseMode）
        SetNoiseMode,      ///< ノイズ除去の種類（ns・steady。オフにする方を先に書き、2 つが同時にオンにならないようにする）
        SetSteadyDb,       ///< 一定の雑音を消す強さ（steadyDb）
        SetAutostart,      ///< 自動起動（value = 有効）
        SetNsParams,       ///< ノイズ除去の強さ（vad・grace）
        ApplySettings,     ///< 出口の設定をまとめてかける（value = エコー除去、autoGain、ns・steady、withNsParams なら
                           ///< vad・grace・steadyDb も）。自動音量調整 → エコー除去 → ノイズ除去の種類 → 強さの順に書いてから
                           ///< 1 回だけ読み直す
        SetMute,           ///< 既定のマイクのミュート（value = ミュートする。wpctl set-mute @DEFAULT_AUDIO_SOURCE@ 1|0）
        SetDefaultOutput,  ///< 音の出口を既定にする（nodeId・nodeName。wpctl set-default）
        SetDefaultInput,   ///< 使うマイクを既定にする（nodeId・nodeName）
    };
    Kind kind;
    bool value = false;
    double vad = 0.0;            ///< SetNsParams・ApplySettings: 判定の厳しさ（%）
    double grace = 0.0;          ///< SetNsParams・ApplySettings: 余韻（ms）
    bool autoGain = true;        ///< ApplySettings: エコー除去の自動音量調整
    bool ns = false;             ///< ApplySettings・SetNoiseMode: Valve のノイズ除去
    bool steady = false;         ///< ApplySettings・SetNoiseMode: 一定の雑音を消す
    double steadyDb = 0.0;       ///< SetSteadyDb・ApplySettings: 一定の雑音を消す強さ（dB）
    bool withNsParams = false;   ///< ApplySettings: 強さ（ノイズ除去の判定の厳しさ・余韻と、一定の雑音を消す強さ）もかける
    bool onlyIfChanged = false;  ///< ApplySettings: 今の値と同じものは書かない（自動の切り替え）
    int nodeId = -1;             ///< SetDefault*: ノードの id
    std::string nodeName {};     ///< SetDefault*: ノードの名前（読み返しで比べる）

    /**
     * 出口の設定をまとめてかける書き込みを作る。
     * @param echo エコー除去
     * @param autoGain エコー除去の自動音量調整
     * @param mode ノイズ除去の種類
     * @param withNsParams 強さもかけるか
     * @param vad 判定の厳しさ（%）
     * @param grace 余韻（ms）
     * @param steadyDb 一定の雑音を消す強さ（dB）
     * @param onlyIfChanged 今の値と同じものは書かないか
     * @return 書き込み
     */
    static MicCommand applySettings(bool echo, bool autoGain, NoiseMode mode, bool withNsParams, double vad, double grace,
                                    double steadyDb, bool onlyIfChanged);

    /**
     * ノイズ除去の種類を切り替える書き込みを作る（パネルの「細かく調整」）。
     * @param mode 種類
     * @return 書き込み
     */
    static MicCommand noiseMode(NoiseMode mode);

    /**
     * 既定の出力・入力を切り替える書き込みを作る。
     * @param output 出力なら true、入力なら false
     * @param endpoint 切り替え先
     * @return 書き込み
     */
    static MicCommand setDefault(bool output, const AudioEndpoint& endpoint);
};

/**
 * マイクの状態を読み書きするワーカースレッド。
 * パネルが見えている間（setActive(true)）だけ 1 秒ごとに読み直し（出口とマイクの一覧は 5 秒ごと）、
 * 見えていない間は、既定の出力・入力（pw-metadata -n default、約 10ms）を 2 秒ごとに見るだけにする。
 * 既定が変わったら、一覧と今の設定を読み直す（メインが出口の設定をかけるため）。
 * 書き込みを頼まれたら、書いてすぐ読み直す。
 * ノイズ除去の強さは「かけたい値」を覚えておき、起動したとき（ノードが見つかるまで 5 秒おきに試す）と、
 * 読み直しでノードの id が変わっていたとき（PipeWire がノードを作り直したとき）にかけ直す。
 * 一定の雑音を消す強さも同じ（frame-mic-tuner-denoise.service が起動し直すとノードの id が変わる）。
 */
class MicWorker {
public:
    /** 読み直しの間隔（秒）。 */
    static constexpr double kRefreshSec = 1.0;
    /** 自動起動の状態（systemctl --user is-enabled）を読み直す間隔（秒）。 */
    static constexpr double kAutostartRefreshSec = 5.0;
    /** パネルが見えている間に、出口とマイクの一覧（pw-dump）を読み直す間隔（秒）。 */
    static constexpr double kDevicesRefreshSec = 5.0;
    /** 既定の出力・入力（pw-metadata）を見る間隔（秒。パネルを閉じている間も）。 */
    static constexpr double kDefaultPollSec = 2.0;
    /** 起動した直後、ノイズ除去のノードが見つかるまで試し直す間隔（秒）。 */
    static constexpr double kApplyRetrySec = 5.0;

    MicWorker() = default;
    ~MicWorker();
    MicWorker(const MicWorker&) = delete;
    MicWorker& operator=(const MicWorker&) = delete;

    /** スレッドを始める（始めた直後に 1 回だけ、設定と一覧をまとめて読む）。 */
    void start();

    /**
     * スレッドを止めて待つ（実行中のコマンドがあれば、その終わり（最長でタイムアウト）まで待つ）。
     */
    void stop();

    /**
     * パネルが見えているかを知らせる。見えていない → 見えているに変わったら、すぐ読み直す。
     * @param active 見えていれば true
     */
    void setActive(bool active);

    /** 出口とマイクの一覧をすぐ読み直してもらう（選ぶ画面を開いたとき）。 */
    void refreshDevicesNow();

    /**
     * 書き込みを頼む（書いたあと、すぐ読み直す）。ノイズ除去・一定の雑音を消す強さは、まだ実行していない前の値を捨てて
     * 最後の値だけにする。
     * @param command 書き込み
     * @return 受付番号（completed() がこの番号以上になったら、書き込みと読み直しが終わっている）
     */
    uint64_t request(MicCommand command);

    /** @return 終わった書き込みの数（捨てた分も含む。request() の受付番号と比べる） */
    uint64_t completed() const;

    /**
     * ノイズ除去の強さの「かけたい値」を決める（設定ファイルから）。まだかけていなければ、ノードが見つかりしだいかける。
     * @param vad 判定の厳しさ（%）
     * @param grace 余韻（ms）
     */
    void setDesiredNsParams(double vad, double grace);

    /**
     * 今の状態の写しを取る。
     * @param state 書き込み先
     * @return 状態が変わるたびに増える番号（描き直しの判定用）
     */
    uint64_t snapshot(MicState& state) const;

    /** @return 状態が変わるたびに増える番号 */
    uint64_t version() const;

private:
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::thread thread_;
    std::deque<MicCommand> queue_;
    bool active_ = false;
    bool refreshNow_ = false;
    bool devicesNow_ = false;     ///< 一覧をすぐ読み直す
    bool stop_ = false;
    MicState state_;
    uint64_t version_ = 0;
    bool hasDesiredNs_ = false;   ///< ノイズ除去の強さの「かけたい値」があるか
    double desiredVad_ = kNsVadDefault;
    double desiredGrace_ = kNsGraceDefault;
    int appliedNodeId_ = -1;      ///< かけたい値を最後にかけたノードの id（-1 = まだ）
    NsHiddenWrite hiddenNs_;      ///< 書いた値が読み返しにまだ出ていない（PipeWire 1.6。読み返しの代わりにかけたい値を見せる）
    bool hasDesiredSteady_ = false;  ///< 一定の雑音を消す強さの「かけたい値」があるか
    double desiredSteadyDb_ = kSteadyDbDefault;
    int appliedSteadyNodeId_ = -1;   ///< かけたい値を最後にかけたノードの id（-1 = まだ）
    SteadyHiddenWrite hiddenSteady_; ///< 書いた値が読み返しにまだ出ていない（NsHiddenWrite と同じ）
    uint64_t requested_ = 0;      ///< 受け付けた書き込みの数
    uint64_t completed_ = 0;      ///< 終わった書き込みの数（捨てた分も含む）

    /** スレッドの本体。 */
    void run();

    /**
     * 書き込みを 1 つ実行する（mutex は持たずに呼ぶ）。
     * @param command 書き込み
     * @param nodeId 強さを書くノードの id（SetNsParams なら ns_capture、SetSteadyDb なら frame_mic_denoise_capture。
     *               -1 なら読んで探す）
     * @return 成功したら true
     */
    static bool execute(const MicCommand& command, int nodeId);
};
