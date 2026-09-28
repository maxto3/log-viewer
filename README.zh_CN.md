# Log Viewer（日志查看器）

一款跨平台（Windows / Linux）桌面日志查看器，基于 **Qt 6 Widgets + C++20** 构建。
以表格方式阅读文本日志，支持查找与过滤、内嵌 JSON/XML/YAML 片段的 VSCode 配色高亮，
以及类似 `tail -f` 的实时监控。

![详情框与过滤](screenshots/02-reallog-chinese-preview.png)

## 功能一览

| 方面 | 说明 |
| --- | --- |
| 阅读 | 表格列自动判定（行号、时间、级别、线程、目标…）；行号列默认显示源文件物理行号，Windows 事件日志 XML/TSV 导出显示条目序号；隔行交替底色 + 单元格分隔线；默认两行显示、超长以 `…` 截断；Enter 展开整行 |
| 日志格式 | Rust `tracing`、syslog（RFC 3164 / 5424）、`journalctl`（short / JSON）、JSON Lines、logfmt、CSV/TSV、Python logging、Serilog、log4j/Logback、Windows 事件日志导出（事件查看器文本/TAB、XML、`Format-List` 文本块）、IIS W3C，以及任意文本日志的通用兜底 |
| 查找 | 命中高亮（默认绿底黑字）且不隐藏任何行；支持全词 / 通配符（`*` `?`）/ 正则表达式与忽略大小写；`F3` / `Shift+F3` 导航并显示 `当前/总数` |
| 过滤 | 按级别、时间范围（绝对时刻比较）、关键词隐藏不匹配行（关键词过滤提供**反向过滤**勾选框，只保留不匹配的行）；级别复选列表按当前文件实际出现的级别动态生成，随日志切换而变化；条件之间 AND 组合；状态栏显示"显示行数 / 总行数" |
| 语法高亮 | 消息中的 JSON / XML / YAML 片段套用 VSCode Dark+ / Light+ 配色，详情面板同步 |
| 详情框 | 可选面板（右侧或底部），字段名加粗、完整展示超长消息；**隐藏时表格切换为完整内容模式**（每行按内容自适应、不截断）；双击任意单元格复制完整文本 |
| 实时监控 | `Ctrl+M` 对单个文件实现 tail -f：新行自动追加、智能跟随、轮转/截断自动重载 |
| 多文件 | 打开或拖入多个同格式文件按时间交错合并，并增加 File 列；格式不一致时明确拒绝并说明原因 |
| 编码 | UTF-8（含 BOM）、UTF-16 自动识别；非 UTF-8 自动检测 GB18030/GBK、Big5、Shift_JIS、CP1252 并转换为 UTF-8 显示 |
| 语言 | 完整英文与简体中文界面，运行时即时切换 |
| 主题 | 浅色 / 深色 / 跟随系统；语法高亮主题可独立设置 |

## 环境要求

* Windows 10/11 x64（首要支持）或 glibc ≥ 2.28 的 Linux x64
* Qt 6.8.x（LGPLv3）——Windows 下由脚本自动下载安装
* CMake ≥ 3.25、Ninja、C++20 编译器（Windows 用 MSVC 2022，Linux 用 GCC ≥ 13 / Clang ≥ 16）

## Windows 构建与运行

```powershell
cd log-viewer

# 1) 一次性：下载 Qt 6.8.3 LTS（约 2 GB，支持 HTTP 代理）
.\scripts\install-qt.ps1 -Proxy http://localhost:1081     # 无需代理时省略 -Proxy

# 2) 构建（自动定位 Visual Studio、导入 vcvars64、调用 CMake + Ninja）
.\scripts\build.ps1

# 3) 运行
.\scripts\run.ps1                                                    # 空窗口
.\scripts\run.ps1 --demo                                             # 演示数据预览界面
.\scripts\run.ps1 --lang zh_CN .\test-data\sslocal.2026-09-28.log    # 中文界面 + 真实日志
.\scripts\run.ps1 -AppArgs '--version'

# 4) 测试
.\scripts\test.ps1

# 5) 便携目录（可执行文件 + Qt 运行时 + 翻译）
.\scripts\package.ps1 -Config Release                    # 输出 dist\log-viewer\

# 6) 可选：把 .log 关联到本程序（仅当前用户，可撤销）
.\scripts\register-association.ps1
.\scripts\register-association.ps1 -Unregister
```

构建产物位于 `build\windows-msvc-qt6-release\bin\log-viewer.exe`；该目录已由
`windeployqt` 部署完整，`package.ps1` 产出的 `dist\log-viewer\` 是清理后的分发版本。

手动构建（任意已把 CMake/Ninja 加入 PATH 的终端）：

```powershell
cmake --preset windows-msvc-qt6-release
cmake --build --preset windows-msvc-qt6-release
ctest --preset windows-msvc-qt6-release --output-on-failure
```

## Linux 构建与运行

> **尚未验证**：Linux 代码路径按可移植性要求编写，但未在本开发机上实际构建
> （见 spec.md REQ-PLAT-10）。以下为预期步骤。

```bash
sudo apt install build-essential cmake ninja-build \
                 qt6-base-dev qt6-base-dev-tools qt6-l10n-tools libgl1-mesa-dev

./scripts/build.sh              # cmake --preset linux-gcc-release 并构建
./scripts/test.sh               # ctest（无显示环境自动使用 offscreen）
./scripts/run.sh --demo

# Debian 包与桌面集成
./scripts/package-deb.sh        # 输出 build/linux-gcc-release/log-viewer_*_amd64.deb
sudo dpkg -i build/linux-gcc-release/log-viewer_*_amd64.deb
# 或本地安装：
./scripts/register-association.sh
```

## 命令行参数

```
log-viewer [选项] [日志文件...]

  -h, --help             显示帮助并退出
  -v, --version          显示版本与构建信息
      --lang <en|zh_CN>  覆盖界面语言（不写入设置）
      --format <id>      强制日志格式（auto 自动探测，list 列出可用 id）
      --monitor          打开单个文件后立即启用实时监控
      --demo             载入内置演示数据（仅用于界面预览）
```

退出码：`0` 正常、`1` 文件错误、`2` 参数错误。

## 快捷键与鼠标

| 操作 | 行为 |
| --- | --- |
| 单击行 | 显示/更新详情框（"显示详情框"开启时） |
| **双击单元格** | 复制该单元格完整文本到剪贴板 |
| `Enter` / `Space` | 展开 / 收起选中行 |
| `Ctrl+C` | 复制选中内容 |
| `F3` / `Shift+F3` | 下一个 / 上一个查找命中 |
| `Ctrl+F` / `Ctrl+G` | 聚焦查找 / 过滤输入框 |
| 在查找 / 过滤框内按 `Enter` | 提交并执行（查找跳到首个命中，过滤隐藏不匹配行）；输入过程中不刷新表格 |
| `Ctrl+O` / `F5` / `Ctrl+W` / `Ctrl+Q` | 打开 / 刷新 / 关闭 / 退出 |
| `Ctrl+M` | 切换实时监控 |
| `Ctrl+E` | 导出过滤后的行（CSV 或文本） |
| `Ctrl` + 滚轮 | 临时缩放字体 |
| 把日志文件拖到窗口 | 打开（同格式自动合并）；文件夹会被忽略并提示 |
| 右键 | 复制单元格 / 整行 / 消息，列宽自适应 |

## 菜单结构

* **文件(File)** — 打开、刷新、关闭、监控（勾选项）、导出过滤结果、最近打开的文件、退出
* **设置(Settings)**
  * **字体** — 界面字体、表格字体、表头字体、恢复默认
  * **语言** — English / 简体中文（即时生效）
  * **详情框** — *显示详情框*：勾选=始终显示（打开日志自动选中第一条），取消=始终不显示；
    **布局** 子菜单选择详情框位置（右侧或底部）
  * **外观** — 主题（浅色/深色/跟随系统）、语法高亮主题（跟随主题 / VSCode Dark+ / Light+）、高亮颜色、重置全部设置
* **列(Columns)** — 位于设置与关于之间的顶级菜单：按当前文档的列动态生成勾选项，取消勾选即隐藏该列（选择按文档记忆）；行号列始终显示；*显示全部列* 一键恢复
* **关于(About)** — 与文件、设置平级的顶级菜单项：版本、构建信息、许可证

## 设置存储位置

| 平台 | 路径 |
| --- | --- |
| Windows | `%APPDATA%\LogViewer\settings.ini` |
| Linux | `$XDG_CONFIG_HOME/LogViewer/settings.ini`（通常为 `~/.config/LogViewer/`） |

设置项包括字体、语言、布局、主题、高亮颜色、单文件行数上限、续行合并、最近文件与
最近文档的列宽。`设置 ▸ 外观 ▸ 重置全部设置` 可恢复默认值。

## 文档

* `spec.md` — 需求规格说明书（唯一权威来源）
* `design-doc.md` — 技术设计与实施记录
* `screenshots/` — 精选界面截图（索引见 `screenshots/README.md`）

## 目录结构

```
src/app/        设置、主题、翻译、命令行
src/core/       行索引、数据源、日志格式解析器、表格模型、匹配器、监控
src/highlight/  VSCode 配色表、JSON/XML/YAML tokenizer、消息高亮
src/platform/   平台相关实现（编码、字体、路径）
src/ui/         主窗口、过滤面板、表格视图、详情框、对话框
tests/          Qt Test 测试（13 个目标）与冻结日志样本（tests/data/）
test-data/      本地手工测试用的样本日志（已加入 .gitignore，不入库）
scripts/        构建、测试、运行、打包与文件关联脚本
packaging/      Linux 桌面项、AppStream 元数据、图标、CPack DEB
```

## 许可证

MIT。Qt 6 依据 GNU 宽通用公共许可证第 3 版使用。
