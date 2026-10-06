// 画面に出す文言の表（日本語・英語）。描画コードの中に文言を書かず、ここから引く。
// ログや --print の出力は日本語のまま（ここには入れない）。
#pragma once

#include <string>

/** 表示の言語。 */
enum class Language { Ja, En };

/**
 * Frame のシステム言語（設定ファイルに language が無いときの既定値）。
 * Steam の言語設定（~/.steam/registry.vdf の "language"、読むだけ）が日本語なら日本語、
 * 読めなければ LC_ALL / LC_MESSAGES / LANG を見て、どれでもなければ英語。結果は最初の 1 回だけ調べて覚えておく。
 * @return 言語
 */
Language systemLanguage();

/**
 * パネルの文言一式。
 */
struct UiText {
    const char* title;             ///< パネルの見出し
    const char* micInUse;          ///< マイク使用中（バッジ）
    const char* micIdle;           ///< マイク未使用（バッジ）
    const char* loading;           ///< まだ読んでいない
    const char* earphone;          ///< イヤホン
    const char* speaker;           ///< スピーカー
    const char* earphoneHint;      ///< イヤホンのカードの効果（1 行）
    const char* speakerHint;       ///< スピーカーのカードの効果（1 行）
    const char* rowEcho;           ///< エコー除去
    const char* echoHint;          ///< エコー除去の説明
    const char* rowNs;             ///< ノイズ除去
    const char* nsHint;            ///< ノイズ除去の説明
    const char* on;
    const char* off;
    const char* rowChain;          ///< つながり
    const char* stageMic;          ///< 通り道の最初（マイク）
    const char* stageEq;           ///< EQ（音質補正）
    const char* stageEcho;         ///< エコー除去
    const char* stageNs;           ///< ノイズ除去
    const char* stageOut;          ///< 通り道の最後（アプリへ）
    const char* chainIdle;         ///< 使っていないときの説明
    const char* chainUnknown;      ///< つながりを読めない
    const char* voiceTitle;        ///< 声のチェック
    const char* voiceHint;         ///< 声のチェックの説明
    const char* record;            ///< 録音（ボタン）
    const char* stop;              ///< 停止（ボタン）
    const char* recording;         ///< 録音中
    const char* seconds;           ///< 秒の単位（前に数字）
    const char* elapsedFormat;     ///< 録音中の経過と残り（printf 形式。%.1f が 2 つ）
    const char* voiceIdle;         ///< 録音していないときのメーターの場所の説明
    const char* noClips;           ///< 履歴が空
    const char* withNs;            ///< ＋ノイズ除去（履歴の設定の表示）
    const char* unknownSetting;    ///< 録ったときの設定が分からない
    const char* rowLanguage;       ///< 言語
    const char* rowAutostart;      ///< SteamVR と一緒に起動
    const char* autostartMissing;  ///< ユニットファイルが無いときの説明
    const char* autostartUnknown;  ///< 状態を読めないときの説明
    const char* quit;              ///< 終了
    const char* quitConfirm;       ///< もう一度押すと終了
    const char* footer;            ///< 押すとすぐ反映される旨
    const char* errReadSettings;   ///< 設定を読めない（wpctl）
    const char* errNotInstalled;   ///< 切り替えの仕組みが入っていない
    const char* errReadLinks;      ///< つながりを読めない（pw-link）
    const char* errWriteSettings;  ///< 切り替えに失敗（wpctl）
    const char* errWriteAutostart; ///< 自動起動の切り替えに失敗（systemctl）
    const char* errRecord;         ///< 録音を始められない（PipeWire）
    const char* errPlay;           ///< 再生できない（PipeWire）
    const char* nsVad;             ///< 判定の厳しさ（VAD しきい値のバー）
    const char* nsVadHint;         ///< 判定の厳しさの説明
    const char* nsGrace;           ///< 余韻（VAD の猶予のバー）
    const char* nsGraceHint;       ///< 余韻の説明
    const char* nsInactive;        ///< ノイズ除去がオフの間の説明
    const char* nsReset;           ///< 標準に戻す
    const char* errWriteNsParams;  ///< ノイズ除去の強さを変えられない（pw-cli）
    const char* presetTitle;       ///< プリセットの見出し（どこで音を聞いてる？）
    const char* presetHint;        ///< プリセットの見出しの右の説明（選ぶとおすすめの設定になります）
    const char* earphoneCard;      ///< イヤホンのカードの名前（イヤホン・ヘッドホン）
    const char* speakerCard;       ///< スピーカーのカードの名前（Frame のスピーカー）
    const char* chipEchoOff;       ///< イヤホンのカードのチップ（エコー除去 オフ）
    const char* chipEchoOn;        ///< スピーカーのカードのチップ（エコー除去 オン）
    const char* fineTune;          ///< 個別の設定の区切りの見出し（細かく調整）
    const char* presetCustom;      ///< どちらのプリセットとも一致しないときの見出しの右の文
    const char* chipNsOff;         ///< 両方のカードのチップ（ノイズ除去 オフ）
    const char* errWriteEcho;      ///< プリセットのうち、エコー除去の切り替えに失敗（wpctl）
    const char* errWriteNs;        ///< プリセットのうち、ノイズ除去の切り替えに失敗（wpctl）
    const char* tabQuick;          ///< 「かんたん」のタブ（「細かく調整」のタブは fineTune）
    const char* goFine;            ///< かんたんのタブから細かく調整のタブへ移るボタン
    const char* nsDefaultNote;     ///< 「標準に戻す」の左の説明（標準の値）

    // ---- 更新（vendor/frame-updater/strings.md から。今の版の行と、更新の確認・実行） ----
    const char* rowUpdateCheck;         ///< 設定名「新しい版の確認」（今は設定を切り替える画面が無いので未使用）
    const char* hintUpdateCheck;        ///< その説明（今は未使用）
    const char* updateUpToDateFormat;   ///< 最新（%s は版）
    const char* updateChecking;         ///< 確認中
    const char* updateAvailableFormat;  ///< 新しい版あり（%s は版）
    const char* updateButton;           ///< 「更新する」ボタン
    const char* updateManual;           ///< ここからは入れられない版（手で更新してね）
    const char* updateReleasePage;      ///< 「リリースページ:」（手で更新するときの 2 行目で URL の前に出す）
    const char* updateConfirmFormat;    ///< 確認の文言（%s は版。帯の 1 行目）
    const char* updateConfirmHint;      ///< 確認の補足（帯の 2 行目。更新中の 2 行目にも出す。strings.md から変えている）
    const char* updateConfirmYes;       ///< 確認の実行（updateButton と同じ文言）
    const char* updateConfirmNo;        ///< 確認の「やめる」ボタン（ほかを押すか待っても取り消し）
    const char* updateInstallingFormat; ///< 更新中（%s は手順）
    const char* updateInstalledFormat;  ///< 入れ終わった（%s は版。strings.md から変えている: 終了して起動し直す）
    const char* updateInstallFailed;    ///< 更新失敗の見出し（あとに理由が続く）
    const char* updateCheckFailed;      ///< 確認失敗の見出し（あとに理由が続く）
    const char* updateCheckNow;         ///< 「確認」ボタン（いつも出る行のボタン）
    const char* updateRetry;            ///< 「もう一度」ボタン（更新失敗のあと）
    const char* updateDismiss;          ///< 「閉じる」ボタン
    const char* updateLogHint;          ///< 失敗の補足（今は未使用）
    const char* updateInstalledHint;    ///< 入れ終わりの 2 行目（WirePlumber が変わったらヘッドセットの再起動。このアプリだけ）
    // 更新中の手順（UpdateStatus::step）
    const char* updateStepStart;
    const char* updateStepDownload;
    const char* updateStepVerify;
    const char* updateStepExtract;
    const char* updateStepInstall;
    // 更新の失敗の理由（UpdateStatus::error）
    const char* updateErrNetwork;
    const char* updateErrRateLimited;
    const char* updateErrNotFound;
    const char* updateErrBadResponse;
    const char* updateErrBadVersion;
    const char* updateErrBadUrl;
    const char* updateErrMissingTool;
    const char* updateErrNoChecksums;
    const char* updateErrNoAsset;
    const char* updateErrChecksumMismatch;
    const char* updateErrUnsafeArchive;
    const char* updateErrNoInstaller;
    const char* updateErrInstallFailed;
    const char* updateErrBadArgs;
    const char* updateErrBusy;
    const char* updateErrNotNewer;
    const char* updateErrDetachFailed;
    const char* updateErrInterrupted;
    const char* updateErrIo;
    const char* updateErrOther;  ///< 知らない理由・usage・script-failed・spawn-failed もここに落ちる

    // ---- ミュート（既定のマイクがミュートされているとき） ----
    const char* micMuted;       ///< 見出しのバッジ（ミュート中）
    const char* mutedBanner;    ///< ミュートの帯の文
    const char* unmute;         ///< 「ミュートを解除」ボタン
    const char* errWriteMute;   ///< ミュートを解除できない（wpctl）
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
 * @return "ja" / "en"
 */
const char* languageCode(Language language);

/**
 * 設定ファイルの言語の名前を読む。
 * @param code "ja" / "en"
 * @param language 読めたときの書き込み先
 * @return 知っている名前なら true
 */
bool parseLanguage(const std::string& code, Language& language);
