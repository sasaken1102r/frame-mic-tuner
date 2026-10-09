// 画面に出す文言の表（日本語・英語・簡体字中国語）。描画コードの中に文言を書かず、ここから引く。
// 一番下の行（言語・SteamVR と一緒に起動・終了）と更新の文言の多くは、共通の UI 部品 frame-ui の表（frame_ui::strings）を使う。
// ログや --print の出力は日本語のまま（ここには入れない）。
#pragma once

#include <string>

/** 表示の言語。 */
enum class Language { Ja, En, Sc };

/**
 * Frame のシステム言語（設定ファイルに language が無いときの既定値）。
 * Steam の言語設定（~/.steam/registry.vdf の "language"、読むだけ）が日本語なら日本語、簡体字中国語なら簡体字中国語。
 * Steam の設定が読めなければ LC_ALL / LC_MESSAGES / LANG を見て、どれでもなければ英語。結果は最初の 1 回だけ調べて覚えておく。
 * @return 言語
 */
Language systemLanguage();

/**
 * パネルの文言一式（3 つの表は、このフィールドの順に並べる）。
 */
struct UiText {
    // ---- 見出し ----
    const char* title;             ///< パネルの見出し（マイク）
    const char* micInUse;          ///< 状態ラベル: 使用中
    const char* micIdle;           ///< 状態ラベル: 未使用
    const char* micMuted;          ///< 状態ラベル: ミュート中
    const char* loading;           ///< 状態ラベル: まだ読んでいない
    const char* chipUnknown;       ///< 状態ラベル: つながりを読めない
    const char* mute;              ///< 「ミュートする」
    const char* unmute;            ///< 「ミュートを解除」
    const char* appsButton;        ///< 「アプリと更新」
    const char* backToSettings;    ///< 「← マイクの設定へ」（矢印は絵）
    // ---- 音の出口・マイクの欄 ----
    const char* outputLabel;       ///< 欄の小さな見出し（音の出口）
    const char* micLabel;          ///< 欄の小さな見出し（マイク）
    const char* countFormat;       ///< 出口の数（%d）
    const char* speakerName;       ///< Frame のスピーカーの表示名
    const char* speaker;           ///< Frame のスピーカーの短い名前（声のチェックの履歴）
    const char* builtinMicName;    ///< Frame 内蔵マイクの表示名
    const char* builtinMicShort;   ///< その短い名前（マイクの欄）
    const char* micOfFormat;       ///< 外付けのマイクの表示名（%s は機器の名前）
    // ---- タブ ----
    const char* tabQuick;          ///< 「かんたん」
    const char* fineTune;          ///< 「細かく調整」
    // ---- かんたん ----
    const char* listeningFormat;   ///< 見出し（%s は出口の名前）
    const char* earphoneCard;      ///< イヤホンのプリセットの名前
    const char* speakerCard;       ///< スピーカーのプリセットの名前
    const char* earphoneHint;      ///< イヤホンのプリセットの効果（1 行）
    const char* speakerHint;       ///< スピーカーのプリセットの効果（1 行）
    const char* chipEchoOff;       ///< イヤホンのプリセットの札（エコー除去 オフ）
    const char* chipEchoOn;        ///< スピーカーのプリセットの札（エコー除去 オン）
    const char* chipNsOff;         ///< 両方のプリセットの札（ノイズ除去 オフ）
    const char* quickNoteFormat;   ///< プリセットの下の説明（%s は出口の名前）
    const char* quickCustom;       ///< どちらのプリセットとも一致しないときの説明
    const char* switchedFormat;    ///< 自動で切り替えた知らせ（%s は出口の名前）
    const char* undo;              ///< 「元に戻す」
    // ---- 細かく調整 ----
    const char* rowEcho;           ///< エコー除去
    const char* echoHint;          ///< エコー除去の説明
    const char* rowNs;             ///< ノイズ除去
    const char* nsHint;            ///< ノイズ除去の説明
    const char* on;
    const char* off;
    const char* nsVad;             ///< 判定の厳しさ（VAD しきい値のバー）
    const char* nsVadHint;         ///< 判定の厳しさの説明
    const char* nsGrace;           ///< 余韻（VAD の猶予のバー）
    const char* nsGraceHint;       ///< 余韻の説明
    const char* nsInactive;        ///< ノイズ除去がオフの間の説明
    const char* nsReset;           ///< 「標準に戻す」
    const char* nsDefaultNote;     ///< 「標準に戻す」の左の説明（標準の値）
    // ---- 外付けのマイク ----
    const char* externalNowFormat;    ///< 「いまは %s を使っています」
    const char* externalExplain;      ///< 処理がかかるのは内蔵マイクだけ
    const char* externalSavedFormat;  ///< 覚えている設定（%1$s = 出口、%2$s = 設定のまとめ）
    const char* useBuiltinMic;        ///< 「Frame 内蔵マイクに戻す」
    // ---- 設定のまとめ（出口の一覧・声のチェックの履歴） ----
    const char* sumEchoOn;         ///< エコー除去オン
    const char* sumEchoOff;        ///< エコー除去オフ
    const char* sumNsOn;           ///< 後ろに足す「・ノイズ除去オン」
    const char* labelSeparator;    ///< 「AB13X・エコー除去オフ」の区切り
    // ---- つながり ----
    const char* rowChain;          ///< つながり
    const char* stageMic;          ///< 通り道の最初（マイク）
    const char* stageEq;           ///< EQ（音質補正）
    const char* stageEcho;         ///< エコー除去
    const char* stageNs;           ///< ノイズ除去
    const char* stageOut;          ///< 通り道の最後（アプリへ）
    const char* chainIdle;         ///< 使っていないときの説明
    const char* chainUnknown;      ///< つながりを読めない
    // ---- 声のチェック ----
    const char* voiceTitle;        ///< 声のチェック
    const char* voiceHint;         ///< 声のチェックの説明
    const char* record;            ///< 録音（ボタン）
    const char* stop;              ///< 停止（ボタン）
    const char* recording;         ///< 録音中
    const char* seconds;           ///< 秒の単位（前に数字）
    const char* elapsedFormat;     ///< 録音中の経過と残り（printf 形式。%.1f が 2 つ）
    const char* voiceIdle;         ///< 録音していないときの説明
    const char* noClips;           ///< 履歴が空
    const char* clipExternalFormat;///< 外付けのマイクで録った履歴（%s はマイクの短い名前）
    const char* unknownSetting;    ///< 録ったときの設定が分からない
    // ---- 一番下の行の注意書き ----
    const char* footer;            ///< 押すとすぐ反映される旨
    const char* autostartMissing;  ///< ユニットファイルが無いときの説明
    const char* autostartUnknown;  ///< 状態を読めないときの説明
    // ---- 失敗 ----
    const char* errReadSettings;   ///< 設定を読めない（wpctl）
    const char* errNotInstalled;   ///< 切り替えの仕組みが入っていない
    const char* errReadLinks;      ///< つながりを読めない（pw-link）
    const char* errWriteSettings;  ///< 切り替えに失敗（wpctl）
    const char* errWriteAutostart; ///< 自動起動の切り替えに失敗（systemctl）
    const char* errRecord;         ///< 録音を始められない（PipeWire）
    const char* errPlay;           ///< 再生できない（PipeWire）
    const char* errWriteNsParams;  ///< ノイズ除去の強さを変えられない（pw-cli）
    const char* errWriteEcho;      ///< まとめての切り替えのうち、エコー除去に失敗（wpctl）
    const char* errWriteNs;        ///< まとめての切り替えのうち、ノイズ除去に失敗（wpctl）
    const char* errWriteMute;      ///< ミュートを解除できない（wpctl）
    const char* errWriteMuteOn;    ///< ミュートできない（wpctl）
    const char* errWriteOutput;    ///< 音の出口を切り替えられない（wpctl set-default）
    const char* errWriteInput;     ///< 使うマイクを切り替えられない（wpctl set-default）
    // ---- 音の出口を選ぶ ----
    const char* outPickerTitle;
    const char* outPickerSub;
    const char* outConnected;      ///< 見出し: つながっている出口
    const char* outPast;           ///< 見出し: 前に使った出口
    const char* statusActive;      ///< いま使用中（行の状態・札）
    const char* statusConnected;   ///< つながっています
    const char* lastUsedFormat;    ///< 最後に使ったのは %s（M/D）
    const char* lastUsedUnknown;   ///< 最後に使った日が分からない
    const char* viewingTag;        ///< 札: 設定を表示中
    const char* useOutput;         ///< 「ここから音を出す」
    const char* forget;            ///< 「忘れる」
    const char* outPickerNote;     ///< 下の説明
    // ---- 使うマイク ----
    const char* micPickerTitle;
    const char* micPickerSub;
    const char* micNoteBuiltin;    ///< 内蔵マイクの行の説明
    const char* micNoteExternal;   ///< 外付けのマイクの行の説明
    const char* useMic;            ///< 「このマイクを使う」
    // ---- アプリと更新: このアプリ ----
    const char* thisApp;           ///< カードの見出し
    const char* thisAppDesc;       ///< このアプリの説明
    const char* updateAutoHint;    ///< 新しい版の確認がオンのときの説明
    const char* updateOffHint;     ///< 新しい版の確認がオフのときの説明
    const char* rowUpdateCheck;    ///< 「新しい版の確認」
    const char* updateCheckNote;   ///< オフにしたときの説明
    const char* helpTitle;         ///< 「使い方・不具合の報告」
    // 更新のうちこのアプリだけの文言（install.sh が常駐を再起動しないので、終了して起動し直す。ほかは frame-ui の表）
    const char* updateConfirmHint;      ///< 確認・更新中の補足
    const char* updateInstalledFormat;  ///< 入れ終わった（%s は版）
    const char* updateInstalledHint;    ///< 入れ終わりの補足（WirePlumber が変わったらヘッドセットの再起動）
    const char* updateReleasePage;      ///< 「リリースページ: 」（手で更新するときに URL の前に出す）
};

/**
 * 言語の文言の表を返す。
 * @param language 言語
 * @return 文言の表（プログラムの終わりまで有効）
 */
const UiText& uiText(Language language);

/**
 * 設定ファイルに書く言語の名前。
 * @param language 言語
 * @return "ja" / "en" / "sc"
 */
const char* languageCode(Language language);

/**
 * 設定ファイルの言語の名前を読む。
 * @param code "ja" / "en" / "sc"
 * @param language 読めたときの書き込み先
 * @return 知っている名前なら true
 */
bool parseLanguage(const std::string& code, Language& language);
