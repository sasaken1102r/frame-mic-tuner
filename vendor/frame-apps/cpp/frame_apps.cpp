// SPDX-License-Identifier: MIT — part of frame-apps by sasaken1102r, shipped under the host app's MIT license
// See frame_apps.h. The rules for the fetched list, the JSON reader and SHA-256 are in frame_apps_data.cpp; the
// built-in list in frame_apps_catalog.cpp and the built-in icons in frame_apps_icons.cpp (both made by tools/catalog.py).
#include "frame_apps.h"

#include <dirent.h>
#include <fcntl.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <csignal>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <utility>

extern char** environ;

namespace frame_apps {

namespace {

/**
 * @param path a path
 * @return true if it is a regular file
 */
bool isFile(const std::string& path) {
    struct stat st {};
    return ::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

/**
 * @param name program name
 * @return its path from PATH (or the usual folders when PATH isn't set), or "" if it isn't there
 */
std::string findProgram(const std::string& name) {
    const char* pathEnv = std::getenv("PATH");
    const std::string path = pathEnv != nullptr && pathEnv[0] != '\0' ? pathEnv : "/usr/local/bin:/usr/bin:/bin";
    std::stringstream dirs(path);
    std::string dir;
    while (std::getline(dirs, dir, ':')) {
        if (dir.empty() || dir[0] != '/') continue;  // an empty or relative part depends on the current folder; skip it
        const std::string candidate = dir + "/" + name;
        if (::access(candidate.c_str(), X_OK) == 0 && isFile(candidate)) return candidate;
    }
    return "";
}

/**
 * Starts a program without a shell.
 * @param argv program and arguments
 * @param newSession run it in its own session (it outlives the caller)
 * @param stdoutFd where its stdout goes (-1 = /dev/null)
 * @param pid written on success
 * @return true if it started
 */
bool spawn(const std::vector<std::string>& argv, bool newSession, int stdoutFd, pid_t& pid) {
    std::vector<char*> args;
    for (const std::string& a : argv) args.push_back(const_cast<char*>(a.c_str()));
    args.push_back(nullptr);
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, 0, "/dev/null", O_RDONLY, 0);
    if (stdoutFd >= 0) {
        posix_spawn_file_actions_adddup2(&actions, stdoutFd, 1);
    } else {
        posix_spawn_file_actions_addopen(&actions, 1, "/dev/null", O_WRONLY, 0);
    }
    posix_spawn_file_actions_addopen(&actions, 2, "/dev/null", O_WRONLY, 0);
    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);
#ifdef POSIX_SPAWN_SETSID
    if (newSession) posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSID);
#else
    (void)newSession;
#endif
    const int rc = posix_spawn(&pid, args[0], &actions, &attr, args.data(), environ);
    posix_spawnattr_destroy(&attr);
    posix_spawn_file_actions_destroy(&actions);
    return rc == 0;
}

/**
 * Reads a regular file (not through a symlink) of at most MAX bytes.
 * @return true if read and not larger than MAX
 */
bool readLimited(const std::string& path, size_t max, std::string& out) {
    out.clear();
    const int fd = ::open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return false;
    struct stat st {};
    if (::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
        ::close(fd);
        return false;
    }
    char buf[8192];
    for (;;) {
        const ssize_t n = ::read(fd, buf, sizeof(buf));
        if (n <= 0) break;
        out.append(buf, static_cast<size_t>(n));
        if (out.size() > max) break;
    }
    ::close(fd);
    return out.size() <= max;
}

/** Writes a file through a temporary file and rename (never follows a symlink). @return true on success */
bool writeAtomic(const std::string& path, const std::string& data) {
    const std::string tmp = path + ".tmp-" + std::to_string(::getpid());
    const int fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC, 0644);
    if (fd < 0) return false;
    size_t done = 0;
    while (done < data.size()) {
        const ssize_t n = ::write(fd, data.data() + done, data.size() - done);
        if (n <= 0) {
            ::close(fd);
            ::unlink(tmp.c_str());
            return false;
        }
        done += static_cast<size_t>(n);
    }
    if (::close(fd) != 0 || ::rename(tmp.c_str(), path.c_str()) != 0) {
        ::unlink(tmp.c_str());
        return false;
    }
    return true;
}

/**
 * Creates the folder (0700) if needed.
 * @return true if it is a real folder (not a symlink) of this user that nobody else may use (no group/other bits)
 */
bool ensurePrivateDir(const std::string& dir) {
    ::mkdir(dir.c_str(), 0700);
    struct stat st {};
    return ::lstat(dir.c_str(), &st) == 0 && S_ISDIR(st.st_mode) && st.st_uid == ::getuid() && (st.st_mode & 077) == 0;
}

/** Removes what an interrupted download or write left behind (".download-*" and "*.tmp-*" regular files). */
void removeLeftovers(const std::string& dir) {
    DIR* d = ::opendir(dir.c_str());
    if (d == nullptr) return;
    while (const dirent* entry = ::readdir(d)) {
        const std::string name = entry->d_name;
        if (name.rfind(".download-", 0) != 0 && name.find(".tmp-") == std::string::npos) continue;
        const std::string path = dir + "/" + name;
        struct stat st {};
        if (::lstat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode)) ::unlink(path.c_str());
    }
    ::closedir(d);
}

}  // namespace

const AppInfo* findApp(const std::string& nameOrKey) {
    for (const AppInfo& app : bundledCatalog()) {
        if (app.name == nameOrKey || app.key == nameOrKey) return &app;
    }
    return nullptr;
}

Paths Paths::fromEnvironment() {
    Paths p;
    const char* home = std::getenv("HOME");
    p.home = home != nullptr ? home : "";
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    p.configHome = xdg != nullptr && xdg[0] == '/' ? xdg : p.home + "/.config";
    return p;
}

FetchConfig FetchConfig::fromEnvironment() {
    FetchConfig f;
    const char* url = std::getenv("FRAME_APPS_URL");
    if (url != nullptr && url[0] != '\0') f.siteUrl = url;
    const char* insecure = std::getenv("FRAME_APPS_ALLOW_INSECURE");
    f.allowInsecure = insecure != nullptr && std::string(insecure) == "1";
    const char* xdg = std::getenv("XDG_CACHE_HOME");
    const char* home = std::getenv("HOME");
    const std::string base = xdg != nullptr && xdg[0] == '/' ? std::string(xdg)
                             : home != nullptr && home[0] == '/' ? std::string(home) + "/.cache"
                                                                 : std::string();
    f.cacheDir = base.empty() ? std::string() : base + "/frame-apps";
    return f;
}

bool isInstalled(const AppInfo& app, const Paths& paths) {
    if (paths.home.empty() || !isValidUnit(app.unit) || !isValidMainFile(app.mainFile)) return false;
    if (isFile(paths.configHome + "/systemd/user/" + app.unit)) return true;
    return isFile(paths.home + "/" + app.mainFile);
}

std::string installerCommand(const std::string& key) {
    std::string command = std::string("curl -fsSL ") + kInstallerUrl + " | sh";
    if (isValidKey(key)) command += " -s -- install " + key;
    return command;
}

std::string konsoleScript(const std::string& key, Lang lang) {
    std::string command = installerCommand(key);
#ifdef FRAME_APPS_ENABLE_TEST_COMMAND
    // tests only: try the launch without installing (never in the apps' builds)
    const char* test = std::getenv("FRAME_APPS_TEST_COMMAND");
    if (test != nullptr && test[0] != '\0') command = test;
#endif
    // The closing line has no single quote, so it can sit in single quotes as it is.
    const char* done = lang == Lang::Ja ? "終わりました。Enter で閉じます" : "Done. Press Enter to close.";
    return command + "\necho\nprintf '%s' '" + done + "'\nread -r _\n";
}

std::vector<std::string> konsoleArgv(const std::string& konsole, const std::string& key, Lang lang) {
    // --separate: its own process, so the pid tells when the window closes (and it isn't merged into another Konsole)
    return {konsole, "--separate", "-e", "sh", "-c", konsoleScript(key, lang)};
}

AppsManager::AppsManager(ManagerConfig config) : config_(std::move(config)), catalog_(bundledCatalog()) {
    if (loadCache()) source_ = CatalogSource::Cached;
    buildEntries();
}

AppsManager::~AppsManager() {
    if (systemctlPid_ > 0) {
        ::kill(systemctlPid_, SIGTERM);
        ::waitpid(systemctlPid_, nullptr, 0);
    }
    if (systemctlFd_ >= 0) ::close(systemctlFd_);
    if (curlPid_ > 0) {
        ::kill(curlPid_, SIGTERM);
        ::waitpid(curlPid_, nullptr, 0);
        ::unlink(current_.tmpPath.c_str());
    }
}

bool AppsManager::loadCache() {
    if (config_.fetch.cacheDir.empty()) return false;
    std::string text;
    if (!readLimited(config_.fetch.cacheDir + "/apps.json", kMaxCatalogBytes, text)) return false;
    CatalogParse parsed = parseCatalogJson(text);
    if (!parsed.ok) return false;
    catalog_ = std::move(parsed.apps);
    return true;
}

std::string AppsManager::cachedIcon(const AppInfo& app) const {
    if (config_.fetch.cacheDir.empty() || !isValidIconName(app.icon) || !isValidSha256(app.iconSha256)) return "";
    const std::string path = config_.fetch.cacheDir + "/icons/" + app.icon;
    std::string bytes;
    if (!readLimited(path, kMaxIconBytes, bytes) || !isPng128(bytes) || sha256Hex(bytes) != app.iconSha256) return "";
    return path;
}

void AppsManager::buildEntries() {
    // keep what is known about apps that stay in the list
    std::map<std::string, Entry> old;
    for (const Entry& e : entries_) old[e.app.name] = e;
    entries_.clear();
    const auto add = [&](const AppInfo& app, const RelatedApp* related) {
        Entry e;
        e.app = app;
        const auto it = old.find(app.name);
        if (it != old.end()) {
            e.state = it->second.state;
            e.busy = it->second.busy;
        }
        if (related != nullptr) {
            e.related = true;
            e.reasonJa = related->reasonJa;
            e.reasonEn = related->reasonEn;
        }
        e.iconPath = cachedIcon(app);
        entries_.push_back(e);
    };
    // the related apps first, in the host app's order (only those in the list); then the list's order
    for (const RelatedApp& related : config_.related) {
        for (const AppInfo& app : catalog_) {
            if (app.name == related.name && app.name != config_.self) add(app, &related);
        }
    }
    for (const AppInfo& app : catalog_) {
        if (app.name == config_.self) continue;
        bool already = false;
        for (const Entry& e : entries_) already = already || e.app.name == app.name;
        if (!already) add(app, nullptr);
    }
    syncBusy();
}

int AppsManager::missingCount() const {
    int n = 0;
    for (const Entry& e : entries_) n += e.state == AppState::Missing ? 1 : 0;
    return n;
}

void AppsManager::refreshNow() {
    lastRefresh_ = std::chrono::steady_clock::now();
    everRefreshed_ = true;
    bool changed = false;
    std::vector<std::string> units;
    for (Entry& e : entries_) {
        const bool installed = isInstalled(e.app, config_.paths);
        // keep "Running" until systemctl answers, so the chip doesn't blink
        const AppState next = !installed ? AppState::Missing
                              : e.state == AppState::Running ? AppState::Running
                                                             : AppState::Installed;
        if (next != e.state) {
            e.state = next;
            changed = true;
        }
        if (installed) units.push_back(e.app.unit);
    }
    if (changed) ++revision_;
    if (units.empty() || systemctlPid_ > 0) return;
    const std::string systemctl = findProgram("systemctl");
    if (systemctl.empty()) return;
    int fds[2];
    if (::pipe2(fds, O_CLOEXEC) != 0) return;
    // "--": the unit names (from the list) are never read as options
    std::vector<std::string> argv = {systemctl, "--user", "is-active", "--"};
    argv.insert(argv.end(), units.begin(), units.end());
    pid_t pid = -1;
    if (!spawn(argv, false, fds[1], pid)) {
        ::close(fds[0]);
        ::close(fds[1]);
        return;
    }
    ::close(fds[1]);
    ::fcntl(fds[0], F_SETFL, ::fcntl(fds[0], F_GETFL) | O_NONBLOCK);
    systemctlPid_ = pid;
    systemctlFd_ = fds[0];
    systemctlOut_.clear();
    askedUnits_ = units;
}

void AppsManager::refreshIfStale(double seconds) {
    const double age = std::chrono::duration<double>(std::chrono::steady_clock::now() - lastRefresh_).count();
    if (!everRefreshed_ || age >= seconds) refreshNow();
}

bool AppsManager::applySystemctl(const std::string& output) {
    std::stringstream lines(output);
    std::string line;
    bool changed = false;
    for (const std::string& unit : askedUnits_) {
        if (!std::getline(lines, line)) break;
        for (Entry& e : entries_) {
            if (e.app.unit != unit || e.state == AppState::Missing) continue;
            const AppState next = line == "active" || line == "reloading" ? AppState::Running : AppState::Installed;
            if (next != e.state) {
                e.state = next;
                changed = true;
            }
        }
    }
    return changed;
}

bool AppsManager::syncBusy() {
    bool changed = false;
    for (Entry& e : entries_) {
        const bool b = konsoles_.count(e.app.key) != 0;
        if (b != e.busy) {
            e.busy = b;
            changed = true;
        }
    }
    return changed;
}

std::string AppsManager::siteFile(const std::string& name) const {
    std::string base = config_.fetch.siteUrl;
    while (!base.empty() && base.back() == '/') base.pop_back();
    if (config_.fetch.allowInsecure) {
        if (base.rfind("http://", 0) != 0 && base.rfind("https://", 0) != 0) return "";
    } else if (base != kInstallerUrl) {
        return "";  // only the installer's site, over https
    }
    return base + "/apps/" + name;
}

bool AppsManager::fetchIfDue() {
    if (!fetchEnabled_ || curlPid_ > 0 || config_.fetch.cacheDir.empty()) return false;
    const std::string url = siteFile("apps.json");
    if (url.empty()) {
        lastError_ = "the list's address isn't allowed";
        return false;
    }
    // ~/.cache may not exist yet; frame-apps itself must be this user's real folder
    const std::string& dir = config_.fetch.cacheDir;
    ::mkdir(dir.substr(0, dir.find_last_of('/')).c_str(), 0700);
    if (!ensurePrivateDir(dir)) {
        lastError_ = "can't use " + dir + " (not a private folder of this user)";
        return false;
    }
    removeLeftovers(dir);
    removeLeftovers(dir + "/icons");
    // at most one try per interval, shared by every app (the time of the last try is the file's mtime)
    const std::string stamp = dir + "/last-fetch";
    struct stat st {};
    if (::lstat(stamp.c_str(), &st) == 0 && std::time(nullptr) - st.st_mtime < config_.fetch.intervalSeconds &&
        std::time(nullptr) >= st.st_mtime) {
        return false;
    }
    writeAtomic(stamp, std::to_string(std::time(nullptr)) + "\n");
    queue_.clear();
    queue_.push_back({url, "", "", ""});
    startNextDownload();
    return curlPid_ > 0;
}

void AppsManager::startNextDownload() {
    while (curlPid_ <= 0 && !queue_.empty()) {
        current_ = queue_.front();
        queue_.pop_front();
        const std::string curl = findProgram("curl");
        if (curl.empty()) {
            lastError_ = "curl isn't installed";
            queue_.clear();
            return;
        }
        static unsigned counter = 0;
        current_.tmpPath = config_.fetch.cacheDir + "/.download-" + std::to_string(::getpid()) + "-" + std::to_string(++counter);
        const size_t limit = current_.icon.empty() ? kMaxCatalogBytes : kMaxIconBytes;
        // the same options as frame-updater's curl: fail on HTTP errors, time limits, a size limit, https only;
        // no redirects at all (the site is fixed)
        // -q first: don't read ~/.curlrc (its options could change where or how it fetches)
        std::vector<std::string> argv = {curl, "-q", "--silent", "--show-error", "--fail", "--max-redirs", "0",
                                         "--connect-timeout", "15", "--max-time", "20",
                                         "--max-filesize", std::to_string(limit)};
        if (!config_.fetch.allowInsecure) {
            argv.insert(argv.end(), {"--proto", "=https"});
        }
        argv.insert(argv.end(), {"-o", current_.tmpPath, current_.url});
        pid_t pid = -1;
        if (spawn(argv, false, -1, pid)) {
            curlPid_ = pid;
        } else {
            lastError_ = "couldn't start curl";
        }
    }
}

bool AppsManager::finishDownload(bool ok) {
    const bool isList = current_.icon.empty();
    std::string bytes;
    const bool read = ok && readLimited(current_.tmpPath, isList ? kMaxCatalogBytes : kMaxIconBytes, bytes);
    ::unlink(current_.tmpPath.c_str());
    if (isList) {
        if (!read) {
            lastError_ = ok ? "the list is too large" : "couldn't fetch " + current_.url;
            queue_.clear();
            return false;
        }
        CatalogParse parsed = parseCatalogJson(bytes);
        if (!parsed.ok) {
            lastError_ = parsed.error;  // keep the list in use (the last fetched one, or the built-in one)
            queue_.clear();
            return false;
        }
        lastError_ = parsed.dropped > 0 ? std::to_string(parsed.dropped) + " app(s) of the list were dropped" : "";
        writeAtomic(config_.fetch.cacheDir + "/apps.json", bytes);
        catalog_ = std::move(parsed.apps);
        source_ = CatalogSource::Fetched;
        // icons that aren't in the cache yet (or changed)
        for (const AppInfo& app : catalog_) {
            if (app.name != config_.self && cachedIcon(app).empty()) {
                const std::string url = siteFile(app.icon);
                if (!url.empty()) queue_.push_back({url, "", app.icon, app.iconSha256});
            }
        }
        buildEntries();
        ++revision_;
        return true;
    }
    if (!read || !isPng128(bytes) || sha256Hex(bytes) != current_.sha256) {
        lastError_ = "the icon " + current_.icon + " didn't match";  // the built-in icon or the letters are used
        return false;
    }
    if (!ensurePrivateDir(config_.fetch.cacheDir + "/icons") ||
        !writeAtomic(config_.fetch.cacheDir + "/icons/" + current_.icon, bytes)) {
        lastError_ = "couldn't save the icon " + current_.icon;
        return false;
    }
    buildEntries();
    ++revision_;
    return true;
}

bool AppsManager::tick() {
    bool changed = false;
    // Konsole windows: when one closes, the install may have changed something
    bool closed = false;
    for (auto it = konsoles_.begin(); it != konsoles_.end();) {
        if (it->second > 0 && ::waitpid(it->second, nullptr, WNOHANG) == it->second) {
            it = konsoles_.erase(it);
            closed = true;
        } else {
            ++it;
        }
    }
    if (closed) {
        changed = true;
        changed |= syncBusy();
        refreshNow();
    }
    // systemctl's answer
    if (systemctlFd_ >= 0) {
        char buf[512];
        for (;;) {
            const ssize_t n = ::read(systemctlFd_, buf, sizeof(buf));
            if (n > 0) {
                systemctlOut_.append(buf, static_cast<size_t>(n));
                continue;
            }
            break;
        }
        if (systemctlPid_ > 0 && ::waitpid(systemctlPid_, nullptr, WNOHANG) == systemctlPid_) {
            ssize_t n;
            while ((n = ::read(systemctlFd_, buf, sizeof(buf))) > 0) systemctlOut_.append(buf, static_cast<size_t>(n));
            ::close(systemctlFd_);
            systemctlFd_ = -1;
            systemctlPid_ = -1;
            changed |= applySystemctl(systemctlOut_);
        }
    }
    // curl: the list, then the icons one by one
    int status = 0;
    if (curlPid_ > 0 && ::waitpid(curlPid_, &status, WNOHANG) == curlPid_) {
        curlPid_ = -1;
        if (finishDownload(WIFEXITED(status) && WEXITSTATUS(status) == 0)) {
            changed = true;
            refreshNow();  // new apps need their states
        }
        startNextDownload();
    }
    if (changed) ++revision_;
    return changed;
}

LaunchResult AppsManager::openInstaller(const std::string& key, Lang lang) {
    if (!key.empty()) {
        // only a short name that follows the rule and belongs to an app of the list on show goes into the command
        bool known = false;
        for (const Entry& e : entries_) known = known || e.app.key == key;
        if (!isValidKey(key) || !known) return LaunchResult::UnknownApp;
    }
    if (konsoles_.count(key) != 0) return LaunchResult::Busy;
    const char* display = std::getenv("DISPLAY");
    if (display == nullptr || display[0] == '\0') return LaunchResult::NoDisplay;
    const std::string konsole = findProgram("konsole");
    if (konsole.empty()) return LaunchResult::NoKonsole;
    pid_t pid = -1;
    if (!spawn(konsoleArgv(konsole, key, lang), true, -1, pid)) return LaunchResult::Failed;
    konsoles_[key] = pid;
    syncBusy();
    ++revision_;
    return LaunchResult::Started;
}

void AppsManager::setForPreview(const std::map<std::string, AppState>& states, const std::vector<std::string>& busyKeys) {
    for (Entry& e : entries_) {
        const auto it = states.find(e.app.name);
        if (it != states.end()) e.state = it->second;
    }
    for (const std::string& key : busyKeys) konsoles_[key] = -1;
    syncBusy();
    ++revision_;
}

}  // namespace frame_apps
