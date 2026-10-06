// 外部コマンドの実行をまとめて引き受けるワーカースレッド。
// メインスレッド（ポインターへの応答 33ms おき）を止めないよう、読み書きはすべてここで行い、
// メインは mutex で状態の写しを読むだけにする。
#pragma once

#include "mic_state.h"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>

/** ワーカーに頼む書き込み。 */
struct MicCommand {
    enum class Kind {
        SetEcho,       ///< エコー除去（value = オン）
        SetNs,         ///< ノイズ除去（value = オン）
        SetAutostart,  ///< 自動起動（value = 有効）
        SetNsParams,   ///< ノイズ除去の強さ（vad・grace）
        SetPreset,     ///< プリセット（value = スピーカー）。エコー除去 → ノイズ除去の順に書いてから 1 回だけ読み直す
        Unmute,        ///< 既定のマイクのミュートを解除する（wpctl set-mute @DEFAULT_AUDIO_SOURCE@ 0）
    };
    Kind kind;
    bool value = false;
    double vad = 0.0;    ///< SetNsParams: 判定の厳しさ（%）
    double grace = 0.0;  ///< SetNsParams: 余韻（ms）
};

/**
 * マイクの状態を読み書きするワーカースレッド。
 * パネルが見えている間（setActive(true)）だけ 1 秒ごとに読み直し、見えていない間は何も実行せずに待つ。
 * 書き込みを頼まれたら、書いてすぐ読み直す。
 * ノイズ除去の強さは「かけたい値」を覚えておき、起動したとき（ノードが見つかるまで 5 秒おきに試す）と、
 * 読み直しでノードの id が変わっていたとき（PipeWire がノードを作り直したとき）にかけ直す。
 */
class MicWorker {
public:
    /** 読み直しの間隔（秒）。 */
    static constexpr double kRefreshSec = 1.0;
    /** 自動起動の状態（systemctl --user is-enabled）を読み直す間隔（秒）。 */
    static constexpr double kAutostartRefreshSec = 5.0;
    /** 起動した直後、ノイズ除去のノードが見つかるまで試し直す間隔（秒）。 */
    static constexpr double kApplyRetrySec = 5.0;

    MicWorker() = default;
    ~MicWorker();
    MicWorker(const MicWorker&) = delete;
    MicWorker& operator=(const MicWorker&) = delete;

    /** スレッドを始める（始めた直後は何も実行しない）。 */
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

    /**
     * 書き込みを頼む（書いたあと、すぐ読み直す）。ノイズ除去の強さは、まだ実行していない前の値を捨てて最後の値だけにする。
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
    bool stop_ = false;
    MicState state_;
    uint64_t version_ = 0;
    bool hasDesiredNs_ = false;   ///< ノイズ除去の強さの「かけたい値」があるか
    double desiredVad_ = kNsVadDefault;
    double desiredGrace_ = kNsGraceDefault;
    int appliedNodeId_ = -1;      ///< かけたい値を最後にかけたノードの id（-1 = まだ）
    uint64_t requested_ = 0;      ///< 受け付けた書き込みの数
    uint64_t completed_ = 0;      ///< 終わった書き込みの数（捨てた分も含む）

    /** スレッドの本体。 */
    void run();

    /**
     * 書き込みを 1 つ実行する（mutex は持たずに呼ぶ）。
     * @param command 書き込み
     * @param nodeId ノイズ除去のノードの id（SetNsParams のとき。-1 なら読んで探す）
     * @return 成功したら true
     */
    static bool execute(const MicCommand& command, int nodeId);
};
