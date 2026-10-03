# 路径探索记录：系统日志直接查看（Linux / Windows Event Log）

> 临时记录文件，汇总本项目会话中两次可行性分析。
> 目的：为后续是否立项、如何分期提供决策依据。
> 状态：仅分析，未修改任何代码。

---

## 0. 背景与共同前提

项目定位：Qt 6 Widgets + C++20 桌面日志查看器（单文档合并视图，文本日志为主）。

参考对象：KDE Plasma 的 **KSystemLog** —— 通过菜单/启动页直接列出并打开系统日志。

两次探索分别对应 KSystemLog 的两类数据源：

| 主题 | 数据源本质 | 结论概览 |
| --- | --- | --- |
| Linux 系统日志 | `/var/log/` 下的**纯文本文件** | 可行、低成本（≈2.5 人日），但受规范边界约束 |
| Windows Event Log | `winevt\Logs\*.evtx` **二进制**，运行时被服务占用 | 可行、中高成本（≈6–8.5 人日），需接原生 API |

共同约束（来自 `docs/spec.md` / `AGENTS.md`）：

- `REQ-PLAT-08`（spec.md:271）：**不调用平台专有外部进程**（如 `tasklist`/`ps`）、不依赖 shell。
- `REQ-PLAT-01`（spec.md:264）：平台条件编译**只允许**出现在 `src/platform/`。
- `CON-4`：不引入第三方运行时依赖（系统 DLL 不受限）。
- spec.md:330（范围外）：远程/网络日志源（syslog 监听、SSH、**journalctl 实时订阅**）不做，仅支持 journalctl 导出文件的离线查看。
- 需求/行为变更必须先改 `docs/spec.md`（含修订记录），再同步 `docs/design-doc.md` 与双 README。

现有可复用资产：

- 唯一中央打开 API：`MainWindow::openPaths()`（`src/ui/MainWindow.cpp:1434`）。
- Provider 抽象：`IEntryProvider`（`src/core/IEntryProvider.h:29-44`），仅 5 个方法，对数据来源无假设；
  `DemoProvider`、`TestProvider` 已证明非文件实现可行。
- 显示/过滤/查找/详情面板/状态栏均只依赖 `IEntryProvider` + `DocumentInfo`。
- 已有的 Windows 事件导出解析：`wevt_text` / `wevt_tsv` / `wevt_xml`（`src/core/formats/WindowsFormats.cpp`）。
- 平台层先例：`FileAssociation`（Win32 API + `QSKIP` 测试模式）、`EncodingBackend`。

---

## 1. Linux 系统日志（KSystemLog 风格快捷入口）

### 1.1 结论

**纯"直接打开 `/var/log/*` 文件"完全可行，复杂度中低（≈2.5 人日）。**
但要做的是"文件列表快捷入口"，**不是** KSystemLog 完整形态——KSystemLog 在现代 systemd 发行版上主要靠 `journalctl` 动态生成内容，而本项目规范禁止这条路。

### 1.2 规范红线

- journalctl 实时订阅：明确不做（spec.md:330），只能看离线导出文件。
- `REQ-PLAT-08`：不能用 `QProcess` 跑 `journalctl` / `dmesg` / `last`。
- 因此排除：`*.journal`（二进制）、`wtmp`/`btmp`/`lastlog`（二进制，原本要靠命令）。

### 1.3 方案对比

| 方案 | 内容 | 规范冲突 | 复杂度 | 建议 |
| --- | --- | --- | --- | --- |
| **A. 静态候选子菜单** | 平台层内置发行版候选路径，按存在性/可读性过滤后进 File 菜单 | 无 | 低 | ✅ MVP 首选 |
| B. A + 用户自定义路径 | 设置项 `files/systemLogs` 存自定义列表 | 无 | 中 | 二期 |
| C. journalctl 集成 | 动态执行并解析 journalctl | **违反 REQ-PLAT-08 + spec.md:330** | 高 | 不建议（需先改 spec） |

### 1.4 方案 A 工作拆解

1. **平台层新增候选定位器**（`src/platform/SystemLogLocator.{h,cpp}` 或扩 `PlatformInfo`）
   - `#if defined(Q_OS_LINUX)` 分支，按 `exists() && isReadable() && isFile()` 过滤；
   - 排除 `.gz`、`.1`、`.2` 轮转文件与二进制；
   - 发行版差异必须覆盖：
     | 类别 | Debian/Ubuntu | RHEL/Fedora/SUSE |
     | --- | --- | --- |
     | 系统 | `/var/log/syslog` | `/var/log/messages` |
     | 认证 | `/var/log/auth.log` | `/var/log/secure` |
     | 内核 | `/var/log/kern.log` | `/var/log/dmesg`（仅开机） |
     | 包管理 | `/var/log/dpkg.log` | `/var/log/dnf.log`、`/var/log/zypper.log` |
   - Arch / 纯 journald 系统可能一个都没有 → 必须优雅降级（菜单禁用）。
2. **UI**：仿 `m_recentMenu`（`MainWindow.cpp:298-299`）加 `m_systemLogMenu` 子菜单；
   - 建议 `aboutToShow` 时重建（模板：`m_fileAssociationMenu`，`MainWindow.cpp:338-340`），实时反映文件出现/消失与权限；
   - 点击 lambda 调 `openPaths({path})`（`MainWindow.cpp:1539-1551` 是现成模板）。
3. **错误处理**：完全复用 `reportError()`（`MainWindow.cpp:1529`），无需新通道。
4. **可选设置（方案 B）**：仿 `recentFiles()`（`SettingsStore.cpp:235-255`）加 `files/systemLogs` 键。
5. **i18n**：`tr()` + 在 `logviewer_zh_CN.ts` 的 `lv::MainWindow` context 手工补条目（构建只跑 lrelease，漏了静默回退英文）。
6. **测试**：`tst_mainwindow_behavior` 加菜单存在性用例（仿 `associationMenuEntryExists`）；
   新增定位器纯逻辑测试（临时目录注入，**不依赖真实 `/var/log`**）；`tests/CMakeLists.txt:17` 显式注册。

### 1.5 复杂度

| 工作项 | 量级 |
| --- | --- |
| 平台定位器 | 0.5 d |
| 菜单接线 + 重建 | 0.5 d |
| 空/无权限降级 | 0.25 d |
| i18n | 0.25 d |
| 测试 | 0.5 d |
| 文档（spec + design + README×2） | 0.5 d |
| **合计** | **≈2.5 人日** |

### 1.6 风险与注意点

1. 权限：`auth.log` / `secure` 默认 `root:adm` `640`，普通用户打不开 → 可读性检查为假时禁用并给 tooltip。
2. 二进制混淆：务必排除 `.journal`、`wtmp`、`btmp`、`.gz`。
3. 发行版差异：不能只硬编码 Debian 路径。
4. 空文件（journald 接管后 `/var/log/syslog` 常为空）属正常。
5. 不做"实时刷新系统日志"，避免撞 monitor 语义与 spec 边界。

### 1.7 待决策

1. 是否接受 journalctl 只做"离线导出文件"入口（首期不碰命令）？
2. 候选路径固定内置，还是一开始就允许用户增删（决定是否做方案 B）？

---

## 2. Windows Event Log（原生通道查看）

### 2.1 结论

**可行，价值高，但比 Linux 场景复杂一个量级（≈6–8.5 人日）。**
根因：Windows 事件日志不是文本文件，`C:\Windows\System32\winevt\Logs\*.evtx` 是二进制且被 EventLog 服务占用，**必须走 `wevtapi.dll` 原生 API**。

有利条件：

1. 不违反 `REQ-PLAT-08` —— 禁止的是 `wevtutil.exe`/PowerShell 等**外部进程**，系统 API 调用不在其列；
2. 已有 `wevt_text` / `wevt_tsv` / `wevt_xml` 解析器与级别映射，显示层就绪；
3. `IEntryProvider` 能容纳非文件源；`setProvider(provider, QString())` 会自动禁用 Refresh 与 Monitor（`MainWindow.cpp:1399`、`:1404-1405`），MVP 可不动 watcher。

缺口：没有"非文件源"接入路径——`openPaths()` / `LogSource` / `LogWatcher` 均以真实文件路径为前提。

### 2.2 技术路线对比

| 路线 | 做法 | 评价 |
| --- | --- | --- |
| **A. 原生 API（推荐）** | `EvtQuery` → `EvtNext`（每次 ≤256）→ `EvtRender` 转 XML → `EvtFormatMessage` 取消息 | 无新依赖；实时、字段完整；可读 Security（有权限时）；也可打开归档 `.evtx` |
| B. 直接解析 `.evtx` | 自研 BinXML/模板/CRC 解析 | 跨平台理论可行但需数周，实时文件被服务占用，不值得 |
| C. `wevtutil.exe` / PowerShell | 子进程导出文本再解析 | **违反 REQ-PLAT-08**，慢且脆弱，否决 |

### 2.3 工作拆解（路线 A）

1. **平台后端 `EventLogBackend`**（`src/platform/`，核心工作量）
   - API 流程：
     ```
     EvtQuery(NULL, L"Application", NULL, EvtQueryChannelPath | EvtQueryReverseDirection)
       → EvtNext(hResults, 256, ...)
       → 每事件：EvtRender(NULL, hEvent, EvtRenderEventXml, ...)
                  EvtFormatMessage(hProviderMetadata, hEvent, ...)
       → EvtClose
     ```
   - **最大坑：Message 格式化**。`EvtRender` 只给原始 XML（`System` + `EventData`），**不含** `RenderingInfo/Message`；可读消息须调 `EvtFormatMessage`（依赖 publisher 元数据），部分 provider 无元数据（转发事件、部分第三方）→ 回退 `EventData` 拼接（`parseEventXml` 已有回退）。
   - publisher 元数据句柄必须**按 provider 缓存**，否则每事件加载资源 DLL，性能崩溃。
   - 顺序：通道默认新→旧，表格期望旧→新，需反转。
   - 错误码：`ERROR_ACCESS_DENIED`（Security）、`ERROR_EVT_CHANNEL_NOT_FOUND`(15007) 映射为人话。
   - CMake：在 `src/CMakeLists.txt:74-76` 的 `if(WIN32)` 块追加 `wevtapi`。
   - 非 Windows 提供 `isSupported()==false` 桩（仿 `FileAssociation`）。
2. **Provider `EventLogProvider`**（建议 `src/core/`）
   - 实现 `IEntryProvider`；`entryAt()` O(1)；`sourceName()` = 通道名；
   - `DocumentInfo.truncated` 表示"只载入最近 N 条"；
   - 后端抽象注入 + mock，使逻辑可在 Linux 上测。
3. **复用 XML 解析**（小改动，高收益）
   - `WindowsEventXmlFormat::parseEventXml` 目前是 **private static**（`WindowsFormats.h:84`）；
     提升为公开静态方法（或抽共享 helper），避免字段映射出现两份实现。
4. **UI 入口**（工作量比预想小）
   - `File ▸ Open Event Log ▸ Application / System / Setup / Security`；
   - 新增 `MainWindow::openEventLogChannel()`，内部 `setProvider(provider, QString())`；
   - 现有逻辑自动禁用 Refresh/Monitor、不污染最近文件；
   - 标题/状态栏/详情面板只读 `DocumentInfo`，天然兼容；
   - Security 建议显示但置灰或给明确权限错误提示。
5. **可选 CLI**：`--event-log Application` 或 `--channel Application`（按 `CliParser` 流程 + `tst_cli`）；建议二期。
6. **i18n + 文档 + 测试**
   - spec.md FMT-5（:43）目前只承诺"事件日志**导出**"解析，新增原生通道属需求变更 → 先加 REQ + 修订记录，再同步 design-doc（§4.1、§4.9、§16）与双 README；
   - 测试：mock 后端跨平台单测 + Windows 专项集成测试（真查 Application 通道，仿 `tst_association` 的 `QSKIP` 模式）。

### 2.4 复杂度

| 工作项 | 位置 | 估计 |
| --- | --- | --- |
| P0 技术验证（API 流程 + Message 格式化率/性能实测量） | 临时 | 0.5–1 d |
| 平台后端（查询/分页/渲染/消息/元数据缓存/错误） | `src/platform/` | 2–3 d |
| Provider + mock 注入 | `src/core/` | 1–1.5 d |
| XML 解析复用 | `src/core/formats/` | 0.25 d |
| UI 菜单 + 打开分支 + 权限提示 | `src/ui/` | 0.5–1 d |
| CMake / i18n | — | 0.25 d |
| 测试 | `tests/` | 1 d |
| 文档 | `docs/` | 0.5–1 d |
| **MVP 合计** | | **6–8.5 人日** |

### 2.5 风险与缓解

1. **消息格式化成功率（最大不确定性）** → 先做不入库的 P0 spike：实测 System 通道 1 万条消息的失败率与耗时，再决定继续与否。
2. **大通道卡 UI**：System 通道轻松十万条，当前加载是**同步**的（design-doc §16.6 既有缺口）。
   缓解：事件通道默认上限 2–5 万（低于文件的 200 万行），等待光标 + 截断标记；二期懒渲染。
3. **权限**：Security 通道普通用户必失败，错误文案提示以管理员运行。
4. **消息语言**：`EvtFormatMessage` 输出跟随系统 locale，不受应用语言影响，需文档说明。
5. **监控不适配**：`LogWatcher` 是 `QFileSystemWatcher` + 轮询文件尾，通道源不适用。
   MVP 直接禁用（现有 `m_source == nullptr` 逻辑已天然做到）；二期用 `EventRecordID > N` 的 XPath 轮询或 `EvtSubscribe`。

### 2.6 分期建议

- **一期（MVP，约 1–1.5 周）**：Application / System / Setup 只读打开 + Security 清晰权限错误；不做监控、不做通道选择器、不做 CLI。
- **二期**：监控（按 `EventRecordID` 增量轮询更贴合现有架构）、`EvtOpenChannelEnum` 全通道选择对话框、XPath 过滤、CLI `--event-log`。

---

## 3. 两者对比与本项目内的共性

| 维度 | Linux 系统日志 | Windows Event Log |
| --- | --- | --- |
| 数据源 | 文本文件 | 二进制 + 系统 API |
| 规范冲突 | 无（避开 journalctl 即可） | 无（避开外部进程即可） |
| 主要新代码 | 平台定位器 + 菜单 | 平台后端 + Provider + 菜单 |
| 监控 | 现有 `LogWatcher` 可直接复用 | 需二期改造 |
| 估算 | ≈2.5 人日 | 6–8.5 人日 |
| 风险 | 低 | 中高（消息格式化、性能、权限） |

共性结论：

1. 两者都**不需要改显示层**，`IEntryProvider` 是关键复用点。
2. 两者都必须把平台差异收敛在 `src/platform/`。
3. 两者都属于需求新增，按 AGENTS.md 必须先修订 `docs/spec.md` 再动代码。
4. 建议实施顺序：Linux（快速见效、零风险）→ Windows P0 spike → Windows MVP。

---

## 4. 下一步待办（若立项）

- [ ] 确认 Linux 方案 A/B 范围与待决策两问
- [ ] Windows：写不入库 spike，实测 `EvtQuery/EvtNext/EvtRender/EvtFormatMessage`
      10k 条成功率与耗时
- [ ] spec.md 新增需求条目 + 修订记录
- [ ] design-doc.md 同步（§4.1 / §4.9 / §16）
- [ ] README.md / README.zh_CN.md 同步

---

*记录生成时间：2026-10-01*
*说明：本文件为临时分析记录，内容基于当前工作区代码，未做任何修改。*
