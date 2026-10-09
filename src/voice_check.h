// 声のチェック: アプリに届く最終の音（既定の入力）を最大 10 秒録って、メモリの中に 5 件まで残し、既定の出力で再生する。
// libpipewire-0.3 の pw_stream をアプリの中で使う（pw-record / pw-play は使わない）。PipeWire のループは専用のスレッド。
// 音声はメモリの中だけ。ディスクにもログにも書かない。
#pragma once

#include "mic_state.h"

#include <cstdint>
#include <ctime>
#include <deque>
#include <memory>
#include <string>
#include <vector>

struct pw_thread_loop;
struct pw_stream;
struct pw_stream_events;

constexpr int kVoiceRate = 48000;          ///< 録音・再生のサンプリングレート（mono・int16）
constexpr double kVoiceMaxSec = 10.0;      ///< 1 回の録音の上限（秒）
constexpr size_t kVoiceHistory = 5;        ///< 残す件数
constexpr int kWaveBins = 160;             ///< 小さい波形の本数

/** 録った 1 件。 */
struct VoiceClip {
    uint64_t id = 0;
    std::time_t recordedAt = 0;   ///< 録り始めた時刻
    bool echoKnown = false;       ///< 録ったときの設定
    bool echo = false;
    bool nsKnown = false;
    bool ns = false;
    bool nsParamsKnown = false;   ///< 録ったときのノイズ除去の強さが読めていたか
    double nsVad = 0.0;           ///< 判定の厳しさ（%）
    double nsGrace = 0.0;         ///< 余韻（ms）
    bool outputKnown = false;     ///< 録ったときの音の出口が分かっているか
    bool outputSpeaker = false;   ///< その出口が Frame のスピーカーか
    std::string outputName;       ///< その出口の機器の名前（Frame のスピーカーのときは使わない）
    bool externalMic = false;     ///< 外付けのマイクで録った（エコー除去・ノイズ除去はかかっていない）
    std::string micName;          ///< そのマイクの機器の名前
    std::vector<int16_t> samples;  ///< mono・int16・kVoiceRate
    std::vector<float> wave;       ///< kWaveBins 本の区間ごとのピーク（0〜1）
    float peakDb = -120.0f;        ///< 全体のピーク（dBFS）

    /** @return 長さ（秒） */
    double seconds() const { return static_cast<double>(samples.size()) / kVoiceRate; }
};

/** 声のチェックの失敗の種類。 */
enum class VoiceError {
    None,
    Record,  ///< 録音を始められない・途中で止まった
    Play,    ///< 再生できない
};

/** 描画用の写し。 */
struct VoiceView {
    bool recording = false;
    double recordSec = 0.0;    ///< 録った長さ（秒）
    float levelDb = -120.0f;   ///< 前回の写しから今までのピーク（dBFS）
    bool playing = false;
    uint64_t playingId = 0;
    double playSec = 0.0;      ///< 再生位置（秒）
    std::vector<std::shared_ptr<const VoiceClip>> clips;  ///< 新しい順
    VoiceError error = VoiceError::None;
};

/**
 * 録音・再生・履歴をまとめたもの。メインスレッドから呼ぶ。
 * PipeWire のスレッドとストリームは録音か再生を始めたときに作り、shutdown() で片付ける
 * （パネルを閉じている間は何も作らない）。
 */
class VoiceCheck {
public:
    VoiceCheck() = default;
    ~VoiceCheck();
    VoiceCheck(const VoiceCheck&) = delete;
    VoiceCheck& operator=(const VoiceCheck&) = delete;

    /**
     * 録音を始める（再生中なら止める）。既定の入力につなぐ。node.virtual は付けない
     * （tracker にマイク使用中と数えてもらい、本番と同じフィルターを通った音で録るため）。
     * @param settings 録り始めたときの設定と音の出口・マイク（履歴に「AB13X・エコー除去オフ」などと出す）
     * @return 始められたら true
     */
    bool startRecording(const MicState& settings);

    /**
     * 録音を止めて、録った音を履歴のいちばん上に入れる（6 件目ならいちばん古いものを消す）。
     */
    void stopRecording();

    /**
     * 履歴の 1 件を既定の出力で再生する（録音中なら録音を止める。再生中ならそちらに切り替える）。
     * @param id 履歴の id
     * @return 始められたら true
     */
    bool play(uint64_t id);

    /** 再生を止める。 */
    void stopPlayback();

    /**
     * 録音・再生を止めて、PipeWire のストリームとスレッドを片付ける（履歴は残す）。
     */
    void shutdown();

    /**
     * メインスレッドで毎回呼ぶ: 10 秒に達した録音の締めくくり、終わった再生の片付け、ストリームの失敗の処理。
     * @return 状態が変わった（描き直しが要る）なら true
     */
    bool update();

    /** @return 録音中か再生中なら true（描き直しを速くする） */
    bool busy() const { return recordStream_ != nullptr || playStream_ != nullptr; }

    /** @return 録音中なら true */
    bool recording() const { return recordStream_ != nullptr; }

    /** @return 再生中の id（再生していなければ 0） */
    uint64_t playingId() const { return playStream_ != nullptr && playClip_ ? playClip_->id : 0; }

    /**
     * 描画用の写しを取る（音量のピークは、前回の写しから今までの分を返して空にする）。
     * @return 写し
     */
    VoiceView view();

    /** @return 履歴（新しい順） */
    const std::deque<std::shared_ptr<const VoiceClip>>& clips() const { return clips_; }

private:
    pw_thread_loop* loop_ = nullptr;
    VoiceError error_ = VoiceError::None;
    std::deque<std::shared_ptr<const VoiceClip>> clips_;
    uint64_t nextId_ = 1;

    // ---- 録音（process は PipeWire のスレッドで、ループのロックを持った状態で呼ばれる） ----
    pw_stream* recordStream_ = nullptr;
    std::vector<int16_t> recordBuffer_;  ///< 上限ぶんを先に確保しておく（PipeWire のスレッドで確保しない）
    size_t recordCount_ = 0;
    size_t recordSkip_ = 0;     ///< つないだ直後に捨てる残り（フィルターが入るまでの切り替わり）
    bool recordFull_ = false;
    bool recordFailed_ = false;
    float levelPeak_ = 0.0f;    ///< 前回の写しから今までのピーク（0〜1）
    VoiceClip pending_;          ///< 録っている 1 件の設定と時刻

    // ---- 再生 ----
    pw_stream* playStream_ = nullptr;
    std::shared_ptr<const VoiceClip> playClip_;
    size_t playPos_ = 0;
    bool playFlushed_ = false;
    bool playDrained_ = false;
    bool playFailed_ = false;
    double playFlushedAt_ = 0.0;

    /**
     * PipeWire のスレッドを用意する（無ければ作る）。
     * @return 使えるなら true
     */
    bool ensureLoop();

    /**
     * ストリームを壊す（ループのロックを取って）。
     * @param stream 壊すストリーム（nullptr にする）
     */
    void destroyStream(pw_stream*& stream);

    /**
     * 録音のストリームのイベント（process・状態の変化。PipeWire のスレッドで呼ばれる）。
     * @return イベントの表
     */
    static const pw_stream_events& recordEvents();

    /**
     * 再生のストリームのイベント（process・出し切った・状態の変化）。
     * @return イベントの表
     */
    static const pw_stream_events& playEvents();

    /**
     * mono・int16・kVoiceRate のストリームを作ってつなぐ（ループのロックを持って呼ぶ）。
     * @param name ノードの名前
     * @param capture 録音なら true、再生なら false
     * @return ストリーム。失敗したら nullptr
     */
    pw_stream* connectStream(const char* name, bool capture);
};

/**
 * 0〜1 のピークを dBFS にする。
 * @param peak ピーク
 * @return dBFS（無音は -120）
 */
float peakToDb(float peak);
