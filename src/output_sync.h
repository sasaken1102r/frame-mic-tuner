// 音の出口ごとの設定を、今の出口に合わせてかける係（メインスレッド）。
// - 起動したとき・出口が変わったとき: その出口の覚えた設定をかける（初めての出口はプリセットから）
// - 今の出口の設定が外から変わったとき（ほかのツール・aux ボタン・wpctl settings）: 読み直しで気づいて覚える
// - 前の版から初めて起動したとき: 今の設定を今の出口の設定として移す
// 判断だけをし、書き込みはワーカーに頼む（Request）。OpenVR も外部コマンドも使わないので --self-test で試せる。
#pragma once

#include "config.h"
#include "mic_worker.h"

#include <cstdint>
#include <functional>
#include <string>

/** 出口の設定を、今の出口に合わせてかける係。 */
class OutputSync {
public:
    /** ワーカーに書き込みを頼む関数（受付番号を返す）。 */
    using Request = std::function<uint64_t(const MicCommand&)>;

    /** 自動で切り替えた知らせ（パネルに「〜をつないだので、覚えていた設定にしました［元に戻す］」）。 */
    struct Notice {
        bool shown = false;
        std::string key;         ///< 切り替えた先の出口
        OutputProfile previous;  ///< 切り替える前にかかっていた値（元に戻す）
    };

    /**
     * @param request ワーカーに書き込みを頼む関数
     */
    explicit OutputSync(Request request);

    /**
     * 新しい状態の写しで、移し替え・切り替え・外からの変化の取り込みを決める。
     * @param state ワーカーの状態の写し
     * @param config 設定（出口ごとの設定を書き換える）
     * @param today 今日の日付（YYYY-MM-DD。最後に使った日）
     * @return 設定を変えた（保存が要る）なら true
     */
    bool update(const MicState& state, Config& config, const std::string& today);

    /**
     * パネルから今の出口の設定を書き込んだ（その書き込みが終わるまで、外からの変化として取り込まない）。
     * @param ticket ワーカーの受付番号
     */
    void noteWrite(uint64_t ticket);

    /**
     * 知らせの「元に戻す」: 切り替える前の値をかけ、その出口の設定として覚える。
     * @param config 設定
     * @return 設定を変えたら true
     */
    bool undo(Config& config);

    /** 知らせを消す（出口の設定を手で変えた・別の出口を見ている）。 */
    void dismissNotice() { notice_.shown = false; }

    /** @return 知らせ */
    const Notice& notice() const { return notice_; }

    /** @return 今の出口のキー（まだ決まっていなければ空） */
    const std::string& activeKey() const { return activeKey_; }

private:
    Request request_;
    std::string activeKey_;    ///< 今の出口（設定をかけた出口）
    bool started_ = false;     ///< 起動したときの設定をかけた
    uint64_t waitTicket_ = 0;  ///< この受付番号の書き込みが終わるまで、外からの変化を取り込まない
    Notice notice_;

    /**
     * 出口の設定をかける（ワーカーに頼む）。
     * @param profile 設定
     * @param onlyIfChanged 今と同じものは書かないか
     */
    void apply(const OutputProfile& profile, bool onlyIfChanged);
};

/**
 * 出口の設定を探し、無ければ初めての設定で作る。名前・種類は今の一覧に合わせる。
 * @param config 設定
 * @param key 出口のキー
 * @param devices 今の一覧（名前を取る）
 * @param changed 作った・名前を直したら true にする
 * @return その出口の設定
 */
OutputProfile& ensureProfile(Config& config, const std::string& key, const AudioDevices& devices, bool& changed);

/**
 * 今かかっている値を出口の設定の形にする（読めていない値は base のまま）。
 * @param state 状態
 * @param base 名前などの元
 * @return 設定
 */
OutputProfile liveProfile(const MicState& state, const OutputProfile& base);
