// 音の頭切れを防ぐ: 既定の出力（USB のイヤホン変換など）へ、聞こえない小さな音（5 Hz のサイン・振幅 0.001）を流し続ける。
// USB-C のイヤホン変換（AB13X）は、完全な無音が約 1 秒続くと自分でアンプを切り、次の音の頭が約 0.15 秒聞こえない
// （Frame で確かめた。WirePlumber の suspend のせいではなく、0 を流し続けても切れる。±32 / 16 ビットの 5 Hz なら切れない）。
// libpipewire-0.3 の pw_stream をアプリの中で使う（再生だけ。録音にはしない）。WirePlumber の設定は変えない。
// PipeWire のループは声のチェックとは別の専用のスレッド（パネルを閉じている間も流すため）。
#pragma once

#include "outputs.h"

#include <atomic>
#include <map>
#include <string>

struct pw_thread_loop;
struct pw_stream;
struct pw_stream_events;

constexpr int kKeepAwakeRate = 48000;       ///< サンプリングレート
constexpr int kKeepAwakeChannels = 2;       ///< チャンネル数（F32・インターリーブ）
constexpr double kKeepAwakeHz = 5.0;        ///< サインの周波数（聞こえない）
constexpr double kKeepAwakeAmplitude = 0.001;  ///< 振幅（16 ビットで ±32。音量 30〜69% で効いた。±4 の雑音では効かない）

/**
 * 音の頭切れを防ぐ音を、どのノードへ流すかを決める（判断だけ。--self-test で試せる）。
 * 今の既定の出力の出口（K）が本物の機器の出口で Frame のスピーカーでなく、K の keepAwake がオンなら、
 * 既定の出力のノード（default.audio.sink の名前。USB なら Valve のラッパー alsa_loopback_device.alsa_output.usb-...）。
 * まだ覚えていない出口は keepAwakeDefault で決める。既定が分からないときは流さない。
 * @param devices 今の一覧（既定の出力）
 * @param outputs 出口ごとの設定
 * @return 流す先のノード名（空 = 流さない）
 */
std::string keepAwakeTarget(const AudioDevices& devices, const std::map<std::string, OutputProfile>& outputs);

/**
 * 聞こえない音を流す係。メインスレッドから呼ぶ。
 * PipeWire のスレッドは初めて流すときに作り、shutdown() で片付ける。
 */
class KeepAwake {
public:
    /** ストリームが失敗・切断したあと、また試すまでの秒数（既定の出力の見張りと同じ 2 秒）。 */
    static constexpr double kRetrySec = 2.0;

    KeepAwake() = default;
    ~KeepAwake();
    KeepAwake(const KeepAwake&) = delete;
    KeepAwake& operator=(const KeepAwake&) = delete;

    /**
     * 流す先を合わせる（メインスレッドで毎回呼ぶ。軽い）。先が変わったら止めて向け直す。空なら止める。
     * ストリームが失敗・切断していたら止めて、kRetrySec 秒後にまた試す。
     * @param target 流す先のノード名（keepAwakeTarget。空 = 止める）
     * @param now 現在時刻（秒、単調増加）
     */
    void update(const std::string& target, double now);

    /** 止めて、PipeWire のストリームとスレッドを片付ける（アプリの終了時）。 */
    void shutdown();

    /** @return 流している（ストリームがある）なら true */
    bool playing() const { return stream_ != nullptr; }

    /** @return 今の流す先（空 = 流さない） */
    const std::string& target() const { return target_; }

private:
    pw_thread_loop* loop_ = nullptr;
    pw_stream* stream_ = nullptr;
    std::string target_;            ///< 流す先のノード名
    double retryAt_ = 0.0;          ///< この時刻より前は、止まったストリームを作り直さない
    int failures_ = 0;              ///< 続けて失敗した回数（ログを連打しない）
    std::atomic<bool> failed_ {false};  ///< ストリームが失敗・切断した（PipeWire のスレッドで立てる）
    std::atomic<bool> streaming_ {false};  ///< ストリームが流れ始めた（PipeWire のスレッドで立てる。失敗の回数を 0 に戻す）
    std::string failReason_;        ///< その理由（ログ用。ループのロックを持って読み書きする）
    double phase_ = 0.0;            ///< サインの位相（PipeWire のスレッドだけが触る。作り直してもつなぐ）

    /**
     * PipeWire のスレッドを用意する（無ければ作る）。
     * @return 使えるなら true
     */
    bool ensureLoop();

    /**
     * target_ へ向けてストリームを作ってつなぐ。
     * @return 始められたら true
     */
    bool start();

    /**
     * ストリームを壊す（スレッドは残す）。
     * @return 失敗・切断して止めたなら、その理由（ほかは空）
     */
    std::string stop();

    /**
     * ストリームのイベント（process・状態の変化。PipeWire のスレッドで呼ばれる）。
     * @return イベントの表
     */
    static const pw_stream_events& events();
};
