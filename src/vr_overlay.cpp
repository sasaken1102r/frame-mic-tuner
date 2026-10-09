// OpenVR との接続とオーバーレイの実装。
#include "vr_overlay.h"

#include "openvr.h"

#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

namespace {

constexpr const char* kDashboardKey = "sasaken.frame-mic-tuner";
constexpr const char* kDashboardName = "Mic";
// 1200px を 2.8m に（1px あたりの大きさは v2 の 1024px / 2.4m とほぼ同じ。高さは約 1.52m）
constexpr float kDashboardWidthM = 2.8f;
// 終了時、オーバーレイを消してから VR_Shutdown まで待つ時間（90Hz で約 36 フレーム）
constexpr int kShutdownWaitMs = 400;

/**
 * オーバーレイのエラー名を返す。
 * @param error エラー
 * @return 名前（例: VROverlayError_None）
 */
const char* overlayErrorName(vr::EVROverlayError error) {
    return vr::VROverlay()->GetOverlayErrorNameFromEnum(error);
}

/**
 * オーバーレイのエラーを、成功以外なら標準エラーに出す。
 * @param what 何をしたときか
 * @param error エラー
 * @return 成功なら true
 */
bool checkOverlay(const char* what, vr::EVROverlayError error) {
    if (error == vr::VROverlayError_None) return true;
    std::fprintf(stderr, "[VR] %s に失敗: %s\n", what, overlayErrorName(error));
    return false;
}

/**
 * 終了処理の 1 手順の結果を、成功でも失敗でもログに出す（あとで順番と結果を確かめるため）。
 * @param what 何をしたか
 * @param error 戻り値
 */
void logShutdownStep(const char* what, vr::EVROverlayError error) {
    std::fprintf(stderr, "[VR] 終了処理 %s -> %s\n", what, overlayErrorName(error));
}

}  // namespace

OverlayHealth judgeOverlay(bool findOk, uint64_t found, uint64_t own) {
    // 見つからない（UnknownOverlay など）か、自分がハンドルを持っていない = 消えている
    if (!findOk || own == 0) return OverlayHealth::Missing;
    return found == own ? OverlayHealth::Alive : OverlayHealth::Replaced;
}

VrOverlay::VrOverlay() = default;

VrOverlay::~VrOverlay() {
    shutdown();
}

int VrOverlay::findVrserverPid() {
    DIR* proc = ::opendir("/proc");
    if (proc == nullptr) return -1;
    int found = -1;
    while (const dirent* entry = ::readdir(proc)) {
        const char* name = entry->d_name;
        if (name[0] < '0' || name[0] > '9') continue;
        const std::string commPath = std::string("/proc/") + name + "/comm";
        const int fd = ::open(commPath.c_str(), O_RDONLY | O_CLOEXEC);
        if (fd < 0) continue;
        char comm[64] = {};
        const ssize_t n = ::read(fd, comm, sizeof(comm) - 1);
        ::close(fd);
        if (n > 0 && std::strncmp(comm, "vrserver\n", 9) == 0) {
            found = std::atoi(name);
            break;
        }
    }
    ::closedir(proc);
    return found;
}

VrOverlay::ConnectResult VrOverlay::connect(int width, int height, std::string& message) {
    if (connected_) return ConnectResult::Ok;

    // 1) Background 型で「SteamVR が動いているか」だけ確かめる（動いていなければ起動させない）
    vr::EVRInitError error = vr::VRInitError_None;
    vr::VR_Init(&error, vr::VRApplication_Background);
    if (error != vr::VRInitError_None) {
        message = vr::VR_GetVRInitErrorAsEnglishDescription(error);
        return error == vr::VRInitError_Init_NoServerForBackgroundApp ? ConnectResult::NotRunning
                                                                        : ConnectResult::Error;
    }
    vr::VR_Shutdown();

    // 2) オーバーレイ型でつなぎ直す
    vr::VR_Init(&error, vr::VRApplication_Overlay);
    if (error != vr::VRInitError_None) {
        message = vr::VR_GetVRInitErrorAsEnglishDescription(error);
        return ConnectResult::Error;
    }
    connected_ = true;

    // 3) Vulkan（OpenVR が要求する拡張と、HMD の GPU で作る）
    std::string vkMessage;
    if (!vulkan_.init(vkMessage)) {
        message = "Vulkan の準備に失敗: " + vkMessage;
        shutdown();
        return ConnectResult::Error;
    }

    // 4) ダッシュボードのパネルとサムネイル
    panelWidth_ = width;
    panelHeight_ = height;
    if (!createDashboardOverlay(message)) {
        shutdown();
        return ConnectResult::Error;
    }

    if (!panelTexture_.create(vulkan_, width, height, vkMessage)) {
        message = "パネルのテクスチャを作れません: " + vkMessage;
        shutdown();
        return ConnectResult::Error;
    }

    vrserverPid_ = findVrserverPid();
    lastPanelError_.clear();
    return ConnectResult::Ok;
}

bool VrOverlay::createDashboardOverlay(std::string& message) {
    vr::IVROverlay* overlay = vr::VROverlay();
    vr::VROverlayHandle_t main = vr::k_ulOverlayHandleInvalid;
    vr::VROverlayHandle_t thumbnail = vr::k_ulOverlayHandleInvalid;
    const vr::EVROverlayError createError =
        overlay->CreateDashboardOverlay(kDashboardKey, kDashboardName, &main, &thumbnail);
    std::fprintf(stderr, "[VR] CreateDashboardOverlay(%s) -> %s\n", kDashboardKey, overlayErrorName(createError));
    if (createError != vr::VROverlayError_None) {
        message = std::string("CreateDashboardOverlay: ") + overlayErrorName(createError);
        return false;
    }
    dashboardHandle_ = main;
    thumbnailHandle_ = thumbnail;

    checkOverlay("SetOverlayWidthInMeters", overlay->SetOverlayWidthInMeters(main, kDashboardWidthM));
    checkOverlay("SetOverlayInputMethod", overlay->SetOverlayInputMethod(main, vr::VROverlayInputMethod_Mouse));
    // マウス座標を画像の px にそろえる
    const vr::HmdVector2_t scale = {{static_cast<float>(panelWidth_), static_cast<float>(panelHeight_)}};
    checkOverlay("SetOverlayMouseScale", overlay->SetOverlayMouseScale(main, &scale));
    // ダッシュボードの下のアイコンにホバーしたとき「閉じる」を出す。押されると VREvent_OverlayClosed が届く
    const vr::EVROverlayError closeError = overlay->SetOverlayFlag(main, vr::VROverlayFlags_EnableControlBarClose, true);
    bool closeEnabled = false;
    const vr::EVROverlayError readError =
        overlay->GetOverlayFlag(main, vr::VROverlayFlags_EnableControlBarClose, &closeEnabled);
    std::fprintf(stderr, "[VR] SetOverlayFlag(EnableControlBarClose) -> %s（読み返し: %s, %s）\n",
                 overlayErrorName(closeError), overlayErrorName(readError), closeEnabled ? "true" : "false");
    // コントローラーのスティックで、重ねた画面の一覧をスクロールする（VREvent_ScrollSmooth）
    checkOverlay("SetOverlayFlag(SendVRSmoothScrollEvents)",
                 overlay->SetOverlayFlag(main, vr::VROverlayFlags_SendVRSmoothScrollEvents, true));
    return true;
}

OverlayRepair VrOverlay::ensureOverlay() {
    if (!connected_) return OverlayRepair::Ok;
    vr::IVROverlay* overlay = vr::VROverlay();
    vr::VROverlayHandle_t found = vr::k_ulOverlayHandleInvalid;
    const vr::EVROverlayError findError = overlay->FindOverlay(kDashboardKey, &found);
    const OverlayHealth health = judgeOverlay(findError == vr::VROverlayError_None, found, dashboardHandle_);
    if (health == OverlayHealth::Alive) {
        if (repairFailing_) std::fprintf(stderr, "[VR] 自己修復: オーバーレイが戻っています\n");
        repairFailing_ = false;
        return OverlayRepair::Ok;
    }

    // 消えている（または別のハンドルになっている）。理由をログに出して作り直す
    char reason[160];
    if (health == OverlayHealth::Missing) {
        std::snprintf(reason, sizeof(reason), "FindOverlay(%s) -> %s", kDashboardKey, overlayErrorName(findError));
    } else {
        std::snprintf(reason, sizeof(reason), "FindOverlay(%s) のハンドルが違う（自分 %llu、今 %llu）", kDashboardKey,
                      static_cast<unsigned long long>(dashboardHandle_), static_cast<unsigned long long>(found));
    }
    if (!repairFailing_) {
        std::fprintf(stderr, "[VR] 自己修復: ダッシュボードのオーバーレイが SteamVR から消えています（%s）。作り直します\n",
                     reason);
    }
    // 古いハンドル（自分のもの）を片付ける。消えている前提なので、エラーは無視する。
    // 別のハンドルがキーを持っていても、それには触らない（自分のハンドルだけを消す）
    if (dashboardHandle_ != 0) {
        overlay->ClearOverlayTexture(dashboardHandle_);
        if (thumbnailHandle_ != 0) overlay->ClearOverlayTexture(thumbnailHandle_);
        overlay->DestroyOverlay(dashboardHandle_);
    }
    dashboardHandle_ = 0;
    thumbnailHandle_ = 0;

    std::string message;
    if (!createDashboardOverlay(message)) {
        // KeyInUse（別のプロセスが同じキーを持っている）などは、相手を消さずに次の確かめで再試行する
        if (!repairFailing_ || message != lastRepairMessage_) {
            std::fprintf(stderr, "[VR] 自己修復: 作り直せません（%s）。次の確かめで再試行します\n", message.c_str());
        }
        repairFailing_ = true;
        lastRepairMessage_ = message;
        return OverlayRepair::Failed;
    }
    repairFailing_ = false;
    lastRepairMessage_.clear();
    lastPanelError_.clear();
    std::fprintf(stderr, "[VR] 自己修復: ダッシュボードのオーバーレイを作り直しました\n");
    return OverlayRepair::Repaired;
}

void VrOverlay::shutdown() {
    if (!connected_) return;
    vr::IVROverlay* overlay = vr::VROverlay();
    std::fprintf(stderr, "[VR] 終了処理を始めます\n");

    // 1) テクスチャを外す（コンポジタがこちらの画像を参照しないようにする）
    if (dashboardHandle_ != 0) {
        logShutdownStep("ClearOverlayTexture(パネル)", overlay->ClearOverlayTexture(dashboardHandle_));
        logShutdownStep("ClearOverlayTexture(サムネイル)", overlay->ClearOverlayTexture(thumbnailHandle_));
    }
    // 2) オーバーレイを消す（サムネイルはパネルと一緒に消える）
    if (dashboardHandle_ != 0) logShutdownStep("DestroyOverlay(パネル)", overlay->DestroyOverlay(dashboardHandle_));
    dashboardHandle_ = 0;
    thumbnailHandle_ = 0;

    // 3) コンポジタが数フレーム回って、外したテクスチャを手放すのを待つ
    std::this_thread::sleep_for(std::chrono::milliseconds(kShutdownWaitMs));
    std::fprintf(stderr, "[VR] 終了処理 %dms 待ちました\n", kShutdownWaitMs);

    // 4) OpenVR を閉じる（Vulkan の画像を壊すのはこの後、という OpenVR の決まり）
    vr::VR_Shutdown();
    connected_ = false;
    std::fprintf(stderr, "[VR] 終了処理 VR_Shutdown 済み\n");

    // 5) Vulkan の画像とデバイスを壊す
    thumbnailTexture_.destroy();
    panelTexture_.destroy();
    vulkan_.destroy();
    std::fprintf(stderr, "[VR] 終了処理 Vulkan を片付けました\n");
}

VrEvents VrOverlay::pollEvents() {
    VrEvents result;
    if (!connected_) return result;
    vr::VREvent_t event {};
    while (vr::VRSystem()->PollNextEvent(&event, sizeof(event))) {
        if (event.eventType == vr::VREvent_Quit) result.quit = true;
    }
    if (dashboardHandle_ != 0) {
        while (vr::VROverlay()->PollNextOverlayEvent(dashboardHandle_, &event, sizeof(event))) {
            // マウス座標は左下が原点なので、上が原点になるよう反転する
            const double x = event.data.mouse.x;
            const double y = panelHeight_ - event.data.mouse.y;
            switch (event.eventType) {
                case vr::VREvent_MouseMove: result.pointer.push_back({PointerInput::Type::Move, x, y}); break;
                case vr::VREvent_MouseButtonDown:
                    if (event.data.mouse.button == vr::VRMouseButton_Left) {
                        result.pointer.push_back({PointerInput::Type::Down, x, y});
                    }
                    break;
                case vr::VREvent_MouseButtonUp:
                    if (event.data.mouse.button == vr::VRMouseButton_Left) {
                        result.pointer.push_back({PointerInput::Type::Up, x, y});
                    }
                    break;
                case vr::VREvent_FocusLeave: result.pointer.push_back({PointerInput::Type::Leave, 0, 0}); break;
                case vr::VREvent_ScrollSmooth:
                case vr::VREvent_ScrollDiscrete:
                    result.pointer.push_back({PointerInput::Type::Scroll, 0, event.data.scroll.ydelta});
                    break;
                // ダッシュボードのアイコンにホバーしたときの「閉じる」（VROverlayFlags_EnableControlBarClose）。
                // SteamVR 自体の終了（VRSystem 側の VREvent_Quit）とは別のイベント
                case vr::VREvent_OverlayClosed:
                    std::fprintf(stderr, "[VR] ダッシュボードの「閉じる」が押されました\n");
                    result.closeRequested = true;
                    break;
                case vr::VREvent_OverlayShown: std::fprintf(stderr, "[VR] パネルが開きました\n"); break;
                case vr::VREvent_OverlayHidden: std::fprintf(stderr, "[VR] パネルが閉じました\n"); break;
                default: break;
            }
        }
    }
    if (result.quit) {
        std::fprintf(stderr, "[VR] SteamVR から終了の知らせが来ました\n");
        vr::VRSystem()->AcknowledgeQuit_Exiting();
    }
    return result;
}

bool VrOverlay::steamVrAlive() const {
    if (vrserverPid_ <= 0) return true;  // 確かめられないときは生きている扱い
    return ::kill(vrserverPid_, 0) == 0 || errno == EPERM;
}

bool VrOverlay::panelVisible() const {
    return connected_ && dashboardHandle_ != 0 && vr::VROverlay()->IsOverlayVisible(dashboardHandle_);
}

void VrOverlay::showPanel() {
    if (!connected_ || dashboardHandle_ == 0) return;
    // 戻り値は無い。開けたかは、あとの IsOverlayVisible / VREvent_OverlayShown で分かる
    vr::VROverlay()->ShowDashboard(kDashboardKey);
    std::fprintf(stderr, "[VR] ShowDashboard(%s) を呼びました\n", kDashboardKey);
}

bool VrOverlay::submitThumbnail(const uint8_t* rgba, int size) {
    if (!connected_ || thumbnailHandle_ == 0) return false;
    std::string message;
    if (!thumbnailTexture_.ready() && !thumbnailTexture_.create(vulkan_, size, size, message)) {
        std::fprintf(stderr, "[Vulkan] サムネイルのテクスチャを作れません: %s\n", message.c_str());
        return false;
    }
    if (!thumbnailTexture_.update(thumbnailHandle_, rgba, message)) {
        std::fprintf(stderr, "[VR] サムネイルを送れません: %s\n", message.c_str());
        return false;
    }
    return true;
}

bool VrOverlay::submitPanel(const uint8_t* rgba) {
    if (!connected_ || dashboardHandle_ == 0 || !panelTexture_.ready()) return false;
    std::string message;
    const bool ok = panelTexture_.update(dashboardHandle_, rgba, message);
    // 同じエラーを毎回出さない
    if (message != lastPanelError_) {
        if (!ok) std::fprintf(stderr, "[VR] パネルを送れません: %s\n", message.c_str());
        lastPanelError_ = message;
    }
    return ok;
}

void VrOverlay::logOverlayState(const char* when) const {
    if (!connected_) return;
    vr::IVROverlay* overlay = vr::VROverlay();
    vr::VROverlayHandle_t found = vr::k_ulOverlayHandleInvalid;
    const vr::EVROverlayError findError = overlay->FindOverlay(kDashboardKey, &found);
    uint32_t w = 0;
    uint32_t h = 0;
    const vr::EVROverlayError sizeError = overlay->GetOverlayTextureSize(dashboardHandle_, &w, &h);
    uint32_t tw = 0;
    uint32_t th = 0;
    const vr::EVROverlayError thumbError = overlay->GetOverlayTextureSize(thumbnailHandle_, &tw, &th);
    std::fprintf(stderr,
                 "[VR] 確認（%s）: FindOverlay(%s) -> %s（同じハンドル: %s） パネルの画像 %ux%u（%s） "
                 "サムネイルの画像 %ux%u（%s） パネル表示中: %s ダッシュボード: %s\n",
                 when, kDashboardKey, overlayErrorName(findError), found == dashboardHandle_ ? "はい" : "いいえ", w, h,
                 overlayErrorName(sizeError), tw, th, overlayErrorName(thumbError),
                 overlay->IsOverlayVisible(dashboardHandle_) ? "はい" : "いいえ",
                 overlay->IsDashboardVisible() ? "開" : "閉");
}

int VrOverlay::probe() {
    vr::EVRInitError error = vr::VRInitError_None;
    vr::VR_Init(&error, vr::VRApplication_Background);
    if (error != vr::VRInitError_None) {
        std::printf("SteamVR につながりません: %s\n", vr::VR_GetVRInitErrorAsEnglishDescription(error));
        return 1;
    }
    vr::IVROverlay* overlay = vr::VROverlay();
    int code = 1;
    vr::VROverlayHandle_t handle = vr::k_ulOverlayHandleInvalid;
    const vr::EVROverlayError findError = overlay ? overlay->FindOverlay(kDashboardKey, &handle)
                                                  : vr::VROverlayError_RequestFailed;
    std::printf("FindOverlay(%s) -> %s\n", kDashboardKey, overlay ? overlayErrorName(findError) : "IVROverlay なし");
    if (overlay != nullptr && findError == vr::VROverlayError_None) {
        code = 0;
        char name[128] = {};
        overlay->GetOverlayName(handle, name, sizeof(name), nullptr);
        bool closeFlag = false;
        overlay->GetOverlayFlag(handle, vr::VROverlayFlags_EnableControlBarClose, &closeFlag);
        float widthM = 0.0f;
        overlay->GetOverlayWidthInMeters(handle, &widthM);
        std::printf("  名前: %s  幅: %.2fm  閉じるボタン: %s  パネル表示中: %s  ダッシュボード: %s\n", name, widthM,
                    closeFlag ? "あり" : "なし", overlay->IsOverlayVisible(handle) ? "はい" : "いいえ",
                    overlay->IsDashboardVisible() ? "開" : "閉");
        uint32_t w = 0;
        uint32_t h = 0;
        const vr::EVROverlayError sizeError = overlay->GetOverlayTextureSize(handle, &w, &h);
        std::printf("  GetOverlayTextureSize -> %s（%ux%u）\n", overlayErrorName(sizeError), w, h);
        // GetOverlayImageData は使わない: 2026-09-27 に試すと、Vulkan のテクスチャが入ったオーバーレイに対して
        // 呼んだ側（このプロセス）が SIGSEGV で落ちた（SteamVR 側は無事）。画像が入ったことは大きさで確かめる
    }
    vr::VR_Shutdown();
    return code;
}

int VrOverlay::switchAway(double seconds) {
    vr::EVRInitError error = vr::VRInitError_None;
    vr::VR_Init(&error, vr::VRApplication_Background);
    if (error != vr::VRInitError_None) {
        std::printf("SteamVR につながりません: %s\n", vr::VR_GetVRInitErrorAsEnglishDescription(error));
        return 1;
    }
    vr::VR_Shutdown();
    vr::VR_Init(&error, vr::VRApplication_Overlay);
    if (error != vr::VRInitError_None) {
        std::printf("オーバーレイ型でつなげません: %s\n", vr::VR_GetVRInitErrorAsEnglishDescription(error));
        return 1;
    }
    vr::IVROverlay* overlay = vr::VROverlay();
    constexpr const char* kAwayKey = "sasaken.frame-mic-tuner.probe-away";
    vr::VROverlayHandle_t main = vr::k_ulOverlayHandleInvalid;
    vr::VROverlayHandle_t thumbnail = vr::k_ulOverlayHandleInvalid;
    const vr::EVROverlayError createError = overlay->CreateDashboardOverlay(kAwayKey, "Probe", &main, &thumbnail);
    std::printf("CreateDashboardOverlay(%s) -> %s\n", kAwayKey, overlayErrorName(createError));
    int code = 1;
    if (createError == vr::VROverlayError_None) {
        vr::VROverlayHandle_t mic = vr::k_ulOverlayHandleInvalid;
        overlay->FindOverlay(kDashboardKey, &mic);
        const auto micVisible = [&]() {
            return mic != vr::k_ulOverlayHandleInvalid && overlay->IsOverlayVisible(mic) ? "はい" : "いいえ";
        };
        std::printf("切り替える前: Mic のパネル表示中: %s\n", micVisible());
        // 画像の無いオーバーレイにはダッシュボードが切り替わらなかったので、アイコンの PNG を入れておく
        // （SetOverlayRaw の共有メモリは使わない。ファイルから読ませる）
        char exe[4096] = {};
        const ssize_t n = ::readlink("/proc/self/exe", exe, sizeof(exe) - 1);
        std::string icon = n > 0 ? std::string(exe, static_cast<size_t>(n)) : std::string();
        icon = icon.substr(0, icon.find_last_of('/')) + "/../contrib/icons/frame-mic-tuner-256.png";
        char resolved[4096] = {};
        if (::realpath(icon.c_str(), resolved) != nullptr) {
            std::printf("SetOverlayFromFile(%s) -> %s\n", resolved,
                        overlayErrorName(overlay->SetOverlayFromFile(main, resolved)));
        }
        overlay->SetOverlayWidthInMeters(main, 1.0f);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        overlay->ShowDashboard(kAwayKey);
        std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
        std::printf("切り替えた後: Mic のパネル表示中: %s  一時オーバーレイ表示中: %s\n", micVisible(),
                    overlay->IsOverlayVisible(main) ? "はい" : "いいえ");
        std::printf("DestroyOverlay -> %s\n", overlayErrorName(overlay->DestroyOverlay(main)));
        code = 0;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    vr::VR_Shutdown();
    return code;
}
