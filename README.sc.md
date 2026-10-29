# Frame Mic Tuner

这是 Steam Frame 上的一块 SteamVR 仪表盘面板。戴着头显时，可以用它切换头显麦克风的回声消除和噪声抑制。用耳机时关掉回声消除，口部杂音这类细小声音也能清楚地收进来；用 Frame 扬声器时再打开，避免扬声器的声音窜进麦克风。内置的声音检查会录下应用实际听到的几秒钟，方便当场对比设置。

[English](README.md) · [日本語](README.ja.md) · [Deutsch](README.de.md) · [Français](README.fr.md) · [Español](README.es.md) · [Italiano](README.it.md)

| 简单设置（预设） | 精细调整 |
|---|---|
| ![简单设置标签页](docs/v12-en-quick-speaker_2026-10-18_19-50-00.png) | ![精细调整标签页](docs/v12-en-fine_2026-10-18_19-50-00.png) |

### 演示视频（有声音）

Frame 扬声器播放暴风雪的声音，同时用声音检查录音，再把两段录音放出来听。第 1 段：噪声抑制关闭（扬声器预设）。第 2 段：回声消除也关闭（耳机预设）。关掉回声消除后，扬声器里的暴风雪会进入麦克风。视频里的面板是日文。

https://github.com/user-attachments/assets/14b5e180-f417-4d90-85f5-6ba1dc573546

## 它能做什么

- **按声音输出分别记住设置**：面板会为每个音频输出单独记住 Frame 麦克风的设置（回声消除、噪声抑制及其强度）：Frame 扬声器、USB-C 耳机或转接器、蓝牙耳机。输出一变（例如插上耳机并成为当前输出），即使面板关着，应用也会自己换上该输出保存的设置，并显示「已连接 %s，已切换为记住的设置」，同时给出 **撤销**。没见过的输出如果是 Frame 扬声器，就从 **Frame 扬声器** 预设开始，否则从 **耳机 / 头戴式耳机** 预设开始。
- **声音输出和麦克风**：上方的 **声音输出** 列出已连接的输出和以前用过的输出。在那里可以 **从这里输出**（设为默认输出，与 SteamVR 的音频设置相同），提前查看并修改某个输出的设置，或 **忘记** 不再使用的输出。列表较长时可用摇杆或拖动来滚动。**麦克风** 用来选择应用使用的麦克风。回声消除和噪声抑制只对 Frame 内置麦克风生效；用其他麦克风时，面板会说明这一点，并提供 **改回 Frame 内置麦克风**。
- **简单设置和精细调整**：**简单设置** 里是按收听方式准备的预设。**耳机 / 头戴式耳机** 关闭回声消除和噪声抑制。**Frame 扬声器** 打开回声消除、关闭噪声抑制。**精细调整** 里是单独的开/关，以及噪声抑制的两根滑块：**麦克风灵敏度**（声音要多像人声才会通过；越低越容易通过）和 **收音保持时间**（说完后还继续收音多久）。改动会立刻生效，麦克风正在使用时也不会把声音切断。面板会打开你上次看的标签页。
- **信号链路**：显示是否有应用正在使用麦克风，以及声音实际经过哪些处理（麦克风 → 音质补正 → 回声消除 → 噪声抑制 → 应用），数据来自当前的 PipeWire 连接。
- **静音**：标题栏显示「使用中」「未使用」或红色的「静音中」（从 Steam、`wpctl`、aux 按钮或其他地方静音都一样），旁边就是 **静音** / **取消静音**。
- **声音检查**：最多录 10 秒应用最终收到的声音，保留最近 5 条，并用当前输出播放。每条录音显示时间、长度、录音时的输出和设置（例如「AB13X · 回声消除关」）以及一小段波形。
- **应用与更新**（右上角按钮；粉色圆点表示有内容要看）：当前版本、**立即检查** 和 **更新**、**检查新版本** 开关，以及作者的其他 Steam Frame 应用。和本应用配合的 frame-aux-shortcuts（用 aux 按钮切换麦克风静音）排在最前，其他应用点一下就能打开。安装时会先给出确切命令，再在 Konsole 里打开安装程序。
- 界面语言有日语、英语、简体中文、德语、法语、西班牙语和意大利语。文字和控件满足 WCAG 2.x AA 对比度。

为什么需要它：应用使用麦克风时，SteamOS 会让声音经过音质补正、回声消除和噪声抑制。回声消除会大幅削弱细小声音，噪声抑制会把不像人声的声音变成无声。用耳机时没有需要消除的扬声器声音，关掉回声消除能让更多说话声通过。

## 需要准备

- 一台打开了开发者模式并能 SSH 登录的 Steam Frame（设置 > 系统 > 开发者模式，然后在开发者项里设置密码）。请用不易猜到的密码：打开 SSH 后，同一网络里知道密码的人都能登录头显。
- 头显上的 SteamVR。从源码构建（见下文）需要 cmake、ninja、g++、pkg-config，以及 cairo、FreeType、PipeWire 和 Vulkan 的开发文件，SteamOS 里已经带了。下载的发行版一个都不需要。

## 安装

### 最简单：只在 Frame 里安装（推荐）

不需要电脑。在 Frame 的 Konsole 里（底部栏的 ＋ → 程序列表 → Konsole）输入下面的命令，按 Enter，在菜单里选 **3**（frame-mic-tuner）。

```sh
curl -fsSL https://frame.sasaken1102s.net | sh
```

- 第一次请先到 Steam 设置 → 系统 → 打开「Enable Developer Mode」（关掉时，＋ 列表里不会出现 Konsole）。
- 其他应用（frameeyeosc、frame-jp-keyboard、frame-perf-overlay、frame-aux-shortcuts）也可以从同一个菜单安装。
- 可以先看脚本内容：`curl -fsSL https://frame.sasaken1102s.net/i | less`。
- 更新时再运行同一条命令并选同一个编号。卸载用菜单里的 `u`。
- 分步说明和视频：https://frame.sasaken1102s.net
- 不想回答提问：`curl -fsSL https://frame.sasaken1102s.net | sh -s -- install mic`

装上的内容和选项与下面的「从电脑安装」相同（它会替你运行 `install.sh`）。

### 从电脑安装

从[发布页](https://github.com/sasaken1102r/frame-mic-tuner/releases)下载 `frame-mic-tuner-<version>.tar.gz`，复制到头显，例如从电脑：

```sh
scp frame-mic-tuner-*.tar.gz steamos@<头显IP>:
```

然后在头显上（`ssh steamos@<头显IP>`）：

```sh
tar xzf frame-mic-tuner-*.tar.gz
cd frame-mic-tuner
./install.sh
```

也可以在头显上从源码构建：

```sh
git clone https://github.com/sasaken1102r/frame-mic-tuner.git
cd frame-mic-tuner
./install.sh
```

然后 **把头显重启一次**，让 WirePlumber 加载切换脚本。游玩时不要手动重启 PipeWire 或 WirePlumber：SteamVR 和 Steam Link 会没有声音。如果已经这样了，在头显上重启 SteamVR（或重新连接 Steam Link），声音就会回来。

不需要 sudo。`install.sh` 把所有东西放进主文件夹（`~/.local/bin`、`~/.local/share`、`~/.config`），所以 SteamOS 更新不会把它删掉。手动更新：下载新的发布包，再运行一次它的 `install.sh`（源码目录里则是 `git pull && ./install.sh`）。也可以在面板的 **应用与更新** 里，出现新版本时按 **更新**。0.1.0 没有更新按钮，所以从 0.1.0 到 0.2.0 这一次必须手动更新。

会装上这些：

- 应用本体、仪表盘 **＋**（启动程序）列表里的一项，以及图标
- 一个随 SteamVR 一起启动的 systemd 用户单元（你打开之前不会启用；见下文）
- WirePlumber 脚本和配置（`contrib/wireplumber/`），用来替换 SteamOS 的麦克风跟踪，使回声消除和噪声抑制可以切换。除此之外的行为与原来相同。
- 共用的更新脚本（`~/.local/share/frame-mic-tuner/frame-update.sh`，来自 `vendor/frame-updater/`），供 **应用与更新** 使用

移除：运行 `./install.sh --uninstall`，然后重启头显，即可恢复 SteamOS 原来的麦克风行为。加上 `--purge` 还会删除应用的设置和保存的切换值。

## 用法

1. 打开 SteamVR 仪表盘，按 **＋**（启动程序），选择 **Frame Mic Tuner**。仪表盘底部一排会出现 **Mic** 图标，选中它打开面板。已经在运行时再启动一次，只会打开面板。
2. **声音输出** 按钮显示你正在收听的输出。在 **简单设置** 的「使用 <输出> 收听时」下，选择 **耳机 / 头戴式耳机** 或 **Frame 扬声器**。这是预设：它们把回声消除和噪声抑制设成建议值（每张卡片上的标签会写出来）；噪声抑制的强度保持不变。应用会记住该输出的选择，下次回到这个输出时会自动切换。想细调就换到 **精细调整**。如果这时的设置和两个预设都不一样，简单设置会显示「当前使用“精细调整”的设置」。噪声抑制打开时会切掉细小声音（口部杂音等）。
   - 在 **精细调整** 里拖动 **麦克风灵敏度** 和 **收音保持时间** 来调节噪声抑制。SteamOS 的默认是 23% 和 500 毫秒；**恢复默认** 可以改回去。这两根滑块只在噪声抑制打开时起作用。
   - SteamOS 的音频每次重新启动（例如重启之后）都会把这两个值恢复成默认。应用按输出保存你的值，并在启动时和输出变化时再设上去，所以只在应用运行期间有效。如果希望一直有效，请打开 **随 SteamVR 启动**。应用没在运行时，用的是 SteamOS 的默认值。
   - 应用自己切换之后，简单设置会显示「已连接 <输出>，已切换为记住的设置」。**撤销** 会恢复切换前的设置，并把那个值重新记成这个输出的设置。
3. **声音输出**（左上）：打开「选择声音输出」。按一行即可查看该输出的设置（现在没连接的输出也可以改；下次它成为输出时会用上）。**从这里输出** 与 SteamVR 的音频设置一样，把该输出设为默认。**忘记** 会删除未连接输出里记住的设置。用摇杆或拖动来滚动。
4. **麦克风**（声音输出旁边）：**使用这个麦克风** 选择应用录音用的麦克风。只要不是 Frame 内置麦克风，回声消除和噪声抑制就不会生效，设置卡片会提供 **改回 Frame 内置麦克风**。
5. **静音** / **取消静音**（上方，状态旁边）：立刻静音或取消默认麦克风的静音，不再确认。
6. **声音检查**：按 **录音**，说几句，再按 **停止**（10 秒后会自己停）。换一个设置再录一次，用每一行的 ▶ 对比。关掉面板，录音会马上停止。
7. **随 SteamVR 启动**（底部一行）：打开后，以后会随 SteamVR 自动启动。也可以用 `./install.sh --autostart` 启用。
8. **语言**：面板一开始跟随 Steam Frame 的语言（Steam 设为日语、简体中文、德语、法语、西班牙语或意大利语时用该语言，否则用英语）。用左下角的菜单更改（日本語、English、简体中文、Deutsch、Français、Español、Italiano），选择会被保存。
9. **退出**：按两次，或者把指针放在仪表盘的 Mic 图标上选择关闭。
10. **应用与更新**（右上；**麦克风设置** 返回）。**本应用** 显示正在运行的版本，例如「已是最新（0.3.4）」。**立即检查** 会马上查找更新（**检查新版本** 关掉时也能用）。
    - 有新版本时，方框会出现强调色边框，并写着「有新版本 X」。按 **更新**，它会问「要更新到 X 吗？」，并给出 **取消** 和 **更新**（3 秒后问题会自己消失）。再按一次 **更新** 开始。方框随后显示进度（「更新中：正在下载」等）。
    - 安装过程中面板保持打开，继续运行旧版本。出现「已安装 X。退出并重新启动即可使用」后，按两次 **退出**，再从 **＋** 重新启动（重启 SteamVR 也一样）。如果那条提示还在，用 **关闭** 去掉。新版本如果改了 WirePlumber 脚本，也要重启头显（方框会提醒，更新日志会写明有没有改）。
    - 如果显示「这个版本不能从这里安装」（发布里没有 `SHA256SUMS` 或没有安装包），请按「安装」一节手动更新；方框会给出发布页。
    - 更新失败时，方框变成红框，说明原因，并写明没有改动任何东西，同时提供 **重试** 和 **关闭**。如果只是检查失败（例如没有网络），只有文字变红，**立即检查** 仍然可以按。
    - **检查新版本** 开 / 关：关掉后，每天一次的检查和应用列表的获取都会停止（见「隐私」）。
    - **配合使用的应用** 会显示 frame-aux-shortcuts（用 aux 按钮切换麦克风静音），没装时可以按 **安装**。「sasaken@ 的 Frame 应用」打开完整列表。**安装** 和 **打开安装程序** 会先显示确切命令；**在 Konsole 中打开** 再在 Konsole 里运行，进度显示在 Konsole 中。不使用 sudo，只安装到主文件夹。

也可以用 SSH 改设置：

```sh
wpctl settings --save frame-mic.echo-cancel false        # 耳机：关闭回声消除
wpctl settings --save frame-mic.echo-cancel true         # 扬声器：打开回声消除（默认）
wpctl settings --save frame-mic.noise-suppression true   # 打开噪声抑制（默认是关）
~/.local/bin/frame-mic-tuner --print                     # 当前值、麦克风是否在用、信号链路、输出及其保存的设置
```

这样改的值（或用 aux 按钮改的）会在面板打开时被注意到，最晚在输出变化时注意到，并记成当前输出的设置。

## 出问题时

- **面板显示「切换组件尚未安装」**：WirePlumber 脚本还没加载。运行 `./install.sh`，然后重启头显。
- **自启动按钮是灰色的**：systemd 单元还没安装。运行 `./install.sh`。
- **录音没有声音，或者对方完全听不到你**：检查头显混音器里的麦克风音量（只读，是安全的）：
  ```sh
  amixer -c 0 cget name='VA_DEC0 Volume'
  amixer -c 0 cget name='VA_DEC1 Volume'
  ```
  两者都应该是 `values=96`。如果是 `0`，麦克风实际上等于静音。本应用从不改混音器，但这些值曾经自己掉到 0。要改回去（请自行承担风险；先记下当前值和 `cget` 显示的 `max=`，再用你的头显平时的值，作者的头显是 96）：
  ```sh
  amixer -c 0 cset name='VA_DEC0 Volume' 96
  amixer -c 0 cset name='VA_DEC1 Volume' 96
  ```
- **PipeWire 或 WirePlumber 重启后，SteamVR 或 Steam Link 完全没有声音**：在头显上重启 SteamVR（或重新连接 Steam Link）。声音会回来。
- **SteamOS 更新后完全没有声音**：切换脚本是作为 WirePlumber 的必要部分加载的。如果更新让脚本读不进去，WirePlumber 可能一起停掉，所有声音都没有。通过 SSH 删掉配置并重启头显（运行 `./install.sh --uninstall` 后再重启也一样）：
  ```sh
  rm ~/.config/wireplumber/wireplumber.conf.d/90-frame-mic.conf
  ```
- **哪个应用里都没说话，却显示「使用中」**：任何在录音的应用都算，包括屏幕或声音录制。SteamOS 会对它们全部打开这些处理。
- **＋ 列表里没有 Frame Mic Tuner**：安装后把头显重启一次。
- **应用在运行，仪表盘上却没有 Mic 图标**：SteamVR 有时会丢掉应用的仪表盘面板（例如仪表盘自己重启时）。应用每 3 秒检查一次并自己重建面板，等几秒再打开仪表盘。从 **＋** 再启动一次也会先检查并重建，然后打开面板。如果还是不出现，日志会说明原因（作为服务运行时用 `journalctl --user -u frame-mic-tuner -f`）。日志是日文，以「自己修復」开头的行表示「自我修复」。退出应用再启动也能修好。
- 日志：作为服务运行时用 `journalctl --user -u frame-mic-tuner -f`。要看手动启动的日志，先退出应用，再从 SSH 运行 `~/.local/bin/frame-mic-tuner`。
- **检查新版本失败，或者按「更新」没有反应**：`~/.cache/frame-mic-tuner/update.log` 里有详细过程，`~/.cache/frame-mic-tuner/update-check.json` / `update-state.json` 里是最后一次的原始回答。关掉 **检查新版本**（或在设置文件里写 `update_check: false`）只会停掉每天一次的自动检查，**立即检查** / **更新** 仍然可用。
- **插上耳机后设置没有切换**：应用只跟随 *默认* 声音输出。如果 SteamVR 仍从 Frame 扬声器出声，请在 **声音输出** 里对耳机按 **从这里输出**（在 SteamVR 的音频设置里选也一样）。不是真实设备的输出（滤波器、虚拟输出）会被忽略。
- **安装其他应用时出现「无法打开 Konsole」**：Konsole 需要开发者模式（Steam 设置 → 系统 → Enable Developer Mode）。

## 隐私

- 声音检查的录音只放在应用的内存里。不会写到磁盘或日志，也不会发送到任何地方。应用退出后就消失。
- 录音时面板显示「● 录音中」。关掉面板，录音会马上停止。
- 麦克风可能会录到周围人的声音。录音前请注意。
- 为了决定初始语言，启动时会读一次 Steam 的 `~/.steam/registry.vdf` 里的 `language` 行（只读）。
- 没有遥测。会用到网络的只有检查新版本和获取应用列表（如下）：
  - **何时**：应用运行期间，如果启动时上次的回答已经超过 24 小时，会向 GitHub 询问，之后大约每天一次（失败后再过一小时重试）。按 **立即检查** 会马上询问。在 **应用与更新** 里关掉 **检查新版本**（或在设置文件里写 `update_check: false`）后，自动检查停止，只有按 **立即检查** 或 **更新** 时才会连接。
  - **发送什么**：只向 `api.github.com` 发一条 HTTPS 请求，询问本项目的最新发布。不含账号、标识或使用数据。和普通网页请求一样，GitHub 能看到你的 IP 地址和 `curl` 的 User-Agent。录音、设置和日志都不会发送。
  - **收到什么**：最新发布的版本号、发布页，以及文件名和链接。回答保存在 `~/.cache/frame-mic-tuner/update-check.json`。
  - **只有在按下更新并确认之后**，才会从 `github.com` / `*.githubusercontent.com` 下载该发布的安装包和 `SHA256SUMS`。
- **检查新版本** 打开时，还会从 `frame.sasaken1102s.net` 获取作者的其他 Steam Frame 应用列表和图标（最多每小时一次，与作者的其他应用共用，保存在 `~/.cache/frame-apps/`）。这是一条不附加任何内容的 HTTPS GET（`curl -q --proto =https --max-redirs 0`）：和 GitHub 一样，`frame.sasaken1102s.net` 能看到你的 IP 地址和 `curl` 的 User-Agent，除此之外什么都不发送。关掉 **检查新版本** 后这也停止，面板会显示上次取到的列表或内置列表。从 **应用与更新** 安装其他应用时，会打开 Konsole 并运行确认画面上的安装命令，在 Konsole 里连接 `frame.sasaken1102s.net` 和 GitHub。
- 它写入的文件只有：`~/.config/frame-mic-tuner/config.json`（界面语言、上次打开的标签页、检查新版本的设置，以及每个用过的输出的名称、Frame 麦克风设置和最后使用日期）、`~/.config/frame-mic-tuner/install-args`（上一次 `./install.sh` 的选项，以便更新时按同样选项重装）、`~/.cache/frame-mic-tuner/`（检查结果的缓存、更新日志和状态、更新进行中的锁目录 `update.lock/`，以及每次更新后会删掉下载内容的工作目录）、`~/.cache/frame-apps/`（取来的应用列表和图标，与作者的其他应用共用），以及 `$XDG_RUNTIME_DIR` 里的一个锁文件（内容是进程号，用来防止启动第二份）。更新时还会像运行 `./install.sh` 一样改写已安装的文件。

## 免责声明

- 请自行承担使用风险。本项目使用 AI 模型 Claude Opus 5.5 制作。我在自己的 Steam Frame 上测试过，但不能对你的设备上发生的事情负责。运行之前请自己阅读并检查代码。本软件不提供任何保证（见 [LICENSE](LICENSE)）。
- 它会改动：WirePlumber 的两项设置 `frame-mic.echo-cancel` 和 `frame-mic.noise-suppression`（通过 `wpctl settings`），噪声抑制正在使用的两个参数「VAD Threshold (%)」和「VAD Grace Period (ms)」（通过 `pw-cli set-param`；SteamOS 的音频重新启动时会恢复默认），以及主文件夹里的 WirePlumber 配置（用自己的脚本替换 SteamOS 的麦克风跟踪）。按下 **从这里输出**、**使用这个麦克风** / **改回 Frame 内置麦克风** 或 **静音** / **取消静音** 时，会更改默认输出、默认麦克风或其静音（通过 `wpctl set-default` / `wpctl set-mute`，与 SteamVR 的音频设置相同）。除此之外，它只会在你要求时启用或停用自己的 systemd 用户单元，在你确认安装其他应用时用 Konsole 打开安装程序，以及在从面板更新时，用一个短时的 systemd 用户单元（`frame-mic-tuner-update`）运行新发布的 `install.sh`。
- 它从不重启 PipeWire 或 WirePlumber，从不写入 ALSA 混音器（amixer），从不改 `/etc`，也从不需要 root。
- **更新**：通过 HTTPS 从 GitHub 下载最新发布的 `.tar.gz` 和 `SHA256SUMS`（只连接 GitHub 自己的主机），核对哈希之后才解压到 `~/.cache/frame-mic-tuner/update/`，并用与上次安装相同的选项运行其中的 `install.sh`。如果没有 `SHA256SUMS`、哈希不符，或压缩包里有绝对路径、`..`、链接或特殊文件，它会拒绝并什么都不改。下载的文件结束后会删除。`SHA256SUMS` 是同一发布里的校验和，不是签名：能发现损坏或未下完的文件，但不能防止 GitHub 上的发布本身被替换。正在运行的应用不会重启：要使用新版本，请退出后再启动。具体步骤见 [frame-update.sh 的说明](vendor/frame-updater/frame-update.sh)。
- SteamOS 更新可能改变麦克风滤波器的构成。那时在本项目跟进之前，切换可能失效。因为切换脚本是 WirePlumber 的必要部分，脚本无法加载时也可能让头显完全没有声音（见「出问题时」）。`./install.sh --uninstall` 之后重启，可以恢复 SteamOS 原来的行为。
- 这是非官方项目，与 Valve Corporation 没有关系，也未获其认可。Steam、Steam Frame、SteamOS、SteamVR 和 Steam Link 是 Valve Corporation 在美国和/或其他国家的商标和/或注册商标。这里使用这些名称只是为了说明本软件配合什么使用。

## 开发

在头显上构建：

```sh
cmake -G Ninja -S . -B build && ninja -C build
./build/frame-mic-tuner --help
scripts/package.sh   # 发布构建：dist/frame-mic-tuner-<version>.tar.gz、dist/SHA256SUMS
```

不需要 SteamVR 的常用选项：`--print`、`--set-ns-vad N` / `--set-ns-grace N`（立刻应用噪声抑制强度，不保存）、`--dump-png 路径`（把面板画成 PNG，可用 `--fake-output`、`--fake-switched`、`--fake-external-mic`、`--fake-update STATE` 等 `--fake-*` 做截图状态，并用 `--view` / `--overlay` 画其他画面）、`--test-record 3`（录 3 秒再播放）、`--contrast-report`（每一对颜色的 WCAG 对比度）和 `--version`。设计笔记、切换脚本的原理和测试结果在 [docs/DEVELOPMENT.ja.md](docs/DEVELOPMENT.ja.md)（日文）。

更新机制（`vendor/frame-updater/`）、共用的界面部件（`vendor/frame-ui/`）和作者其他应用的列表（`vendor/frame-apps/`）是从私有的共用仓库复制来的。不要手动修改这些副本。各自的 `verify.sh`（由 `scripts/package.sh` 运行）会检查它们有没有被改过。

Release 构建保留编译器的速度优化，并在支持时启用链接时优化（LTO）和删除未使用的代码与数据。构建后，`ctest --test-dir build --output-on-failure` 会在不需要 SteamVR 和音频硬件的情况下运行原有的 `--self-test` 和穷举所有组合的像素转换测试。`./build/rgba-test --benchmark` 还会把转换耗时和以前的算法做比较。测试不会放进发布包；配置时加上 `-DBUILD_TESTING=OFF` 就不会构建它们。

### 发布

`scripts/package.sh` 会运行 `vendor/` 下三份副本的 `verify.sh`，以 Release 构建，并写出 `dist/frame-mic-tuner-<version>.tar.gz` 和 `dist/SHA256SUMS`（面板的 **更新** 要接受这个发布，这两样是必需的）。最后会打印一条命令，用来创建带这两个文件的 GitHub 发布（及其标签）。请先把发布说明写到仓库外面的文件里：

```sh
gh release create v<version> dist/frame-mic-tuner-<version>.tar.gz dist/SHA256SUMS --title v<version> --notes-file <发布说明文件>
```

不要做成草稿或预发布：面板看的是 GitHub 的最新发布，这两种都不会被看到。

## 许可证

MIT。见 [LICENSE](LICENSE)。附带的 OpenVR 头文件（`third_party/openvr/`）是 Valve Corporation 的 BSD-3-Clause。语言菜单的图标（`assets/heroicons-language-16-solid.svg`，也编进了程序）来自 Tailwind Labs 的 [Heroicons](https://github.com/tailwindlabs/heroicons)，MIT。`vendor/frame-updater/`、`vendor/frame-ui/` 和 `vendor/frame-apps/`（包括编进程序的其他应用图标）是作者自己代码的副本，与本仓库其余部分一样是 MIT。附带的头文件及其使用的库的许可证列在 [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md)。变更记录在 [CHANGELOG.md](CHANGELOG.md)。
