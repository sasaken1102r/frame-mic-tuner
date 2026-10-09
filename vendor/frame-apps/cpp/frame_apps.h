// SPDX-License-Identifier: MIT — part of frame-apps by sasaken1102r, shipped under the host app's MIT license
// The list of sasaken1102r's Steam Frame apps, whether each one is installed or running, and opening the
// installer (curl -fsSL https://frame.sasaken1102s.net | sh) in Konsole so the user can add the others from a panel.
// Shared by the Steam Frame panels (copied from the frame-apps repository; see UPSTREAM next to the copy).
// Depends only on the C++17 standard library and POSIX (curl and systemctl are run as programs). The panel draws
// the list itself and maps the states to its own text (see strings.md in frame-apps).
//
// The list comes from https://frame.sasaken1102s.net/apps/apps.json when "check for updates" is on (at most once an
// hour, kept in ~/.cache/frame-apps/), else from the last fetched copy, else from the list built into the app.
//
// Use from the panel's main loop:
//   frame_apps::AppsManager apps({"frame-aux-shortcuts",
//                                 {{"frame-perf-overlay", "パフォーマンス表示（5 回押し）に使う", "Used for the perf overlay (5 presses)"}}});
//   every loop:      apps.setFetchEnabled(config.updateCheck);
//                    if (apps.tick()) redraw;                 // reaps Konsole, systemctl and curl
//   at start / when the list opens: apps.fetchIfDue();      // at most once an hour
//   when visible:    apps.refreshIfStale(5);                  // looks again every few seconds
//   on "Install":    apps.openInstaller("perf", lang);        // "" opens the menu
//   draw from:       apps.entries() (related ones first, the app itself left out), apps.missingCount()
//   icons:           frame_apps_cairo.h (drawIcon) for cairo panels
#pragma once

#include <sys/types.h>

#include <chrono>
#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <vector>

#define FRAME_APPS_VERSION "0.3.1"

namespace frame_apps {

/** Language of the texts the catalog carries. */
enum class Lang { Ja, En };

/** One app of the catalog. */
struct AppInfo {
    std::string name;        ///< repository and program name ("frame-perf-overlay")
    std::string key;         ///< the installer's short name ("perf")
    std::string mono;        ///< 1–4 letters drawn in place of an icon ("Perf")
    std::string descJa;      ///< one line, Japanese (the same as the installer's menu)
    std::string descEn;      ///< one line, English
    std::string unit;        ///< systemd user unit that runs it ("frame-perf-overlay.service")
    std::string mainFile;    ///< file that tells it is installed, relative to $HOME (".local/bin/frame-perf-overlay")
    std::string icon;        ///< icon file name on the site ("frame-perf-overlay-128.png")
    std::string iconSha256;  ///< its SHA-256 (hex); an icon that doesn't match isn't used

    /**
     * @param lang language
     * @return the description in that language
     */
    const std::string& desc(Lang lang) const { return lang == Lang::Ja ? descJa : descEn; }
};

/**
 * The list built into the app (cpp/frame_apps_catalog.cpp, made by tools/catalog.py from catalog.json). Used when
 * the site can't be reached and nothing was fetched before.
 * @return the list, in the installer's order
 */
const std::vector<AppInfo>& bundledCatalog();

/**
 * @param nameOrKey a name ("frame-perf-overlay") or a short name ("perf")
 * @return the app of the built-in list, or nullptr
 */
const AppInfo* findApp(const std::string& nameOrKey);

// ---- rules for the fetched list (anything that breaks one is dropped) ----

/** @return true if KEY looks like an installer short name: ^[a-z]{1,16}$ (only such names go into a command) */
bool isValidKey(const std::string& key);
/** @return true for ^[a-z0-9][a-z0-9-]{0,39}$ */
bool isValidName(const std::string& name);
/** @return true for ^[A-Za-z0-9]{1,4}$ */
bool isValidMono(const std::string& mono);
/** @return true for ^[a-z0-9][a-z0-9-]{0,39}\.service$ (never starts with -, so systemctl can't take it as an option) */
bool isValidUnit(const std::string& unit);
/** @return true for a relative path under $HOME: [A-Za-z0-9._-] parts joined by /, no . or .. part, 120 bytes at most */
bool isValidMainFile(const std::string& path);
/** @return true for valid UTF-8 of 1–120 characters without control characters */
bool isValidDesc(const std::string& text);
/** @return true for ^[a-z0-9][a-z0-9-]{0,39}-128\.png$ */
bool isValidIconName(const std::string& icon);
/** @return true for 64 lowercase hex digits */
bool isValidSha256(const std::string& hex);
/** @return true if every field of APP follows the rules above */
bool isValidApp(const AppInfo& app);

/** Result of reading apps.json. */
struct CatalogParse {
    bool ok = false;            ///< the file as a whole is usable (otherwise use the fallback)
    std::vector<AppInfo> apps;  ///< the apps that passed every rule, in the file's order
    int dropped = 0;            ///< apps left out (a bad field, or a name or key used twice)
    std::string error;          ///< why the whole file was refused (English, for logs)
};

/** Size limits for what is fetched. */
constexpr size_t kMaxCatalogBytes = 64 * 1024;
constexpr size_t kMaxIconBytes = 64 * 1024;
constexpr size_t kMaxApps = 64;

/**
 * Reads apps.json: {"version": 1, "apps": [{"name", "key", "mono", "desc_ja", "desc_en", "unit", "main_file", "icon",
 * "icon_sha256"}, ...]}. Strict JSON, at most kMaxCatalogBytes and kMaxApps; unknown fields are ignored.
 * @param text the file
 * @return the apps that can be used
 */
CatalogParse parseCatalogJson(const std::string& text);

/**
 * @param bytes data
 * @return its SHA-256 as 64 lowercase hex digits
 */
std::string sha256Hex(const std::string& bytes);

/**
 * @param bytes a file
 * @return true if it is a PNG of 128×128 and at most kMaxIconBytes
 */
bool isPng128(const std::string& bytes);

/** Where to look for installed apps (normally from the environment; tests use a fake home). */
struct Paths {
    std::string home;        ///< $HOME
    std::string configHome;  ///< $XDG_CONFIG_HOME, or $HOME/.config

    /** @return the paths of the current user */
    static Paths fromEnvironment();
};

/**
 * Installed if the app's user unit or its main file is there (the same test as the installer).
 * @param app the app
 * @param paths where to look
 * @return true if installed
 */
bool isInstalled(const AppInfo& app, const Paths& paths);

/** What the panel shows for an app. */
enum class AppState {
    Unknown,    ///< not looked at yet
    Missing,    ///< not installed
    Installed,  ///< installed, not running now
    Running,    ///< its unit is active
};

/** An app the host app uses, and why (shown under the name). */
struct RelatedApp {
    std::string name;      ///< app name from the catalog
    std::string reasonJa;  ///< "パフォーマンス表示（5 回押し）に使う"
    std::string reasonEn;  ///< "Used for the perf overlay (5 presses)"
};

/** One line of the list. */
struct Entry {
    AppInfo app;
    AppState state = AppState::Unknown;
    bool busy = false;     ///< a Konsole opened for this app is still open
    bool related = false;  ///< the host app uses it
    std::string reasonJa;
    std::string reasonEn;
    std::string iconPath;  ///< a fetched icon whose SHA-256 matched ("" = use the built-in one, or the letters)

    /**
     * @param lang language
     * @return why the host app uses it ("" if it doesn't)
     */
    const std::string& reason(Lang lang) const { return lang == Lang::Ja ? reasonJa : reasonEn; }
};

/** An app's icon as PNG bytes inside the program (cpp/frame_apps_icons.cpp, made by tools/catalog.py). */
struct EmbeddedIcon {
    const char* name;           ///< app name
    const unsigned char* data;  ///< the PNG file (128×128)
    size_t size;                ///< its length in bytes
    const char* sha256;         ///< SHA-256 of the PNG (hex)
};

/**
 * @param name app name
 * @return its built-in icon, or nullptr
 */
const EmbeddedIcon* embeddedIcon(const std::string& name);

/** Address of the installer, and of the list (site + "/apps/apps.json") and icons (site + "/apps/<icon>"). */
constexpr const char* kInstallerUrl = "https://frame.sasaken1102s.net";

/**
 * The command the user would type: "curl -fsSL https://frame.sasaken1102s.net | sh" for the menu, or with
 * " -s -- install <key>" for one app. Shown on the confirmation and run in Konsole.
 * @param key short name, or "" for the menu (a key that isn't valid also gives the menu)
 * @return the command line
 */
std::string installerCommand(const std::string& key);

/**
 * The script Konsole runs: installerCommand(key), then "Done. Press Enter to close." so the result stays readable.
 * Only in a build with FRAME_APPS_ENABLE_TEST_COMMAND defined (the tests), FRAME_APPS_TEST_COMMAND replaces the
 * installer command; in the apps the command is always the one the confirmation shows.
 * @param key short name, or "" for the menu
 * @param lang language of the closing line
 * @return the sh -c script
 */
std::string konsoleScript(const std::string& key, Lang lang);

/**
 * @param konsole path of the konsole program
 * @param key short name, or "" for the menu
 * @param lang language of the closing line
 * @return argv: konsole --separate -e sh -c <script>
 */
std::vector<std::string> konsoleArgv(const std::string& konsole, const std::string& key, Lang lang);

/** Result of openInstaller(). */
enum class LaunchResult {
    Started,     ///< Konsole was started
    Busy,        ///< a Konsole for the same app (or the menu) is still open
    UnknownApp,  ///< the key isn't in the list
    NoDisplay,   ///< DISPLAY isn't set (no screen to open a window on)
    NoKonsole,   ///< konsole isn't installed
    Failed,      ///< it couldn't be started
};

/** Where the list is fetched from and kept. */
struct FetchConfig {
    std::string siteUrl = kInstallerUrl;  ///< FRAME_APPS_URL (tests); without FRAME_APPS_ALLOW_INSECURE only this host
    bool allowInsecure = false;           ///< FRAME_APPS_ALLOW_INSECURE=1 (tests): http:// and any host
    std::string cacheDir;                 ///< ${XDG_CACHE_HOME:-~/.cache}/frame-apps
    int intervalSeconds = 3600;           ///< at most one try per this many seconds (shared by every app)

    /** @return the settings of the current user */
    static FetchConfig fromEnvironment();
};

/** Where the list on show came from. */
enum class CatalogSource {
    Bundled,  ///< built into the app
    Cached,   ///< fetched before (from ~/.cache/frame-apps/)
    Fetched,  ///< fetched by this process
};

/** What the manager needs to know about the host app. */
struct ManagerConfig {
    std::string self;                  ///< the host app's name (left out of the list)
    std::vector<RelatedApp> related;   ///< apps the host app uses, in the order to show them
    Paths paths = Paths::fromEnvironment();
    FetchConfig fetch = FetchConfig::fromEnvironment();
};

/**
 * Keeps the list with each app's state, the Konsole windows it opened, and fetches the list. Not thread-safe: call
 * every method from the same thread (the panel's main loop). Child processes are reaped in tick().
 */
class AppsManager {
public:
    /** @param config the host app and the apps it uses. Loads the cached list if there is a usable one */
    explicit AppsManager(ManagerConfig config);
    /** Konsole windows stay open (they run in their own session); a running systemctl or curl is stopped. */
    ~AppsManager();
    AppsManager(const AppsManager&) = delete;
    AppsManager& operator=(const AppsManager&) = delete;

    /**
     * Reaps finished children: reads systemctl's answer, takes curl's downloads, and looks again when a Konsole closes.
     * @return true if entries() or busy() changed (redraw)
     */
    bool tick();

    /** Looks at the files now and asks systemctl which units run (the answer arrives in a later tick()). */
    void refreshNow();

    /**
     * Calls refreshNow() if the last look is older than the given time.
     * @param seconds how old the last look may be
     */
    void refreshIfStale(double seconds);

    /**
     * Whether the list may be fetched (the host app's "check for updates"). Off: nothing goes to the network.
     * @param enabled on or off
     */
    void setFetchEnabled(bool enabled) { fetchEnabled_ = enabled; }

    /**
     * Starts fetching the list if allowed and the last try (by any app) is older than the interval.
     * @return true if a fetch started
     */
    bool fetchIfDue();

    /** @return true while curl is fetching the list or an icon */
    bool fetching() const { return curlPid_ > 0; }

    /** @return where the list on show came from */
    CatalogSource source() const { return source_; }

    /** @return the last fetch problem (English, for logs; "" if none) */
    const std::string& lastError() const { return lastError_; }

    /** @return the list: the related apps first (in their order), then the others; the host app left out */
    const std::vector<Entry>& entries() const { return entries_; }

    /** @return how many apps of the list are not installed */
    int missingCount() const;

    /**
     * Opens Konsole with the installer.
     * @param key short name of an app in the list, or "" for the menu
     * @param lang language of the closing line
     * @return what happened
     */
    LaunchResult openInstaller(const std::string& key, Lang lang);

    /**
     * @param key short name, or "" for the menu
     * @return true if the Konsole opened for it is still open
     */
    bool busy(const std::string& key) const { return konsoles_.count(key) != 0; }

    /** @return true if any Konsole opened here is still open */
    bool anyBusy() const { return !konsoles_.empty(); }

    /** @return a number that changes whenever entries() or busy() changes */
    std::uint64_t revision() const { return revision_; }

    /**
     * For previews and tests: replaces the states without looking.
     * @param states name → state (apps not given stay as they are)
     * @param busyKeys short names to show as busy
     */
    void setForPreview(const std::map<std::string, AppState>& states, const std::vector<std::string>& busyKeys);

private:
    /** What the running curl is downloading. */
    struct Download {
        std::string url;
        std::string tmpPath;
        std::string icon;      ///< "" for apps.json, else the icon file name
        std::string sha256;    ///< the icon's expected SHA-256
    };

    ManagerConfig config_;
    std::vector<AppInfo> catalog_;  ///< the list in use (validated)
    std::vector<Entry> entries_;
    std::map<std::string, pid_t> konsoles_;  ///< key ("" = menu) → Konsole's pid (-1 for previews)
    pid_t systemctlPid_ = -1;
    int systemctlFd_ = -1;
    std::string systemctlOut_;
    std::vector<std::string> askedUnits_;  ///< units in the order systemctl was asked
    std::chrono::steady_clock::time_point lastRefresh_{};
    bool everRefreshed_ = false;
    std::uint64_t revision_ = 0;
    bool fetchEnabled_ = false;
    CatalogSource source_ = CatalogSource::Bundled;
    std::string lastError_;
    pid_t curlPid_ = -1;
    Download current_;
    std::deque<Download> queue_;  ///< icons still to download

    /** Builds entries_ from catalog_ and the related apps, keeping states and busy flags of known apps. */
    void buildEntries();
    /** Reads systemctl's output into the states. @return true if a state changed */
    bool applySystemctl(const std::string& output);
    /** Marks busy flags from konsoles_. @return true if a flag changed */
    bool syncBusy();
    /** Loads ~/.cache/frame-apps/apps.json if it is usable. @return true if loaded */
    bool loadCache();
    /** @return the URL of a file under the site's apps/ folder, or "" if it isn't allowed */
    std::string siteFile(const std::string& name) const;
    /** Starts curl for the first queued download. */
    void startNextDownload();
    /** Handles a finished download. @param ok curl succeeded @return true if the list changed */
    bool finishDownload(bool ok);
    /** @return the cached icon's path if its SHA-256 and size match, else "" */
    std::string cachedIcon(const AppInfo& app) const;
};

}  // namespace frame_apps
