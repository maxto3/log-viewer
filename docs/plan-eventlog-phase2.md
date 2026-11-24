# Windows 事件日志二期计划：UAC 提权读取与实时监控

> 状态：**P1–P4/P6 已实施**（2026-11-24，见 `docs/design-doc.md` §16.13）；Phase C（Security 监控）与真实 UAC/新事件人工验收仍待完成。
> 范围：仅 Windows；不改变一期已交付的只读查看、分档继续加载与既有测试。
> 关联：`docs/spec.md`（FMT-7 / REQ-EVT-01…09）、`docs/design-doc.md` §16.12、`src/core/EventLogProvider`、`src/platform/EventLogBackend.cpp`、`src/platform/ElevatedFileReader`、`src/core/LogWatcher`。
>
> 本文件**不是**需求文档。按仓库约定，实施前必须先修订 `docs/spec.md`（含修订记录），再同步 `docs/design-doc.md` 与双 README。

---

## 0. 结论与推荐路线

| 能力 | 推荐方案 | 备选 |
| --- | --- | --- |
| UAC 提权读取 | **独立辅助进程 + 命名管道**：主程序创建私有管道，`ShellExecuteExW(runas)` 启动同一 exe 的 `--event-log-stream` 辅助模式，提权进程通过 `wevtapi` 读取并把记录流回非提权主进程 | ① 重启整个程序为管理员；② 提权进程写私有临时快照文件 |
| 实时监控 | **`QTimer` + `EventRecordID` 增量 `EvtQuery`（2 s 轮询）**：只取 `EventRecordID > last` 的新记录，最新在前，插入表格顶部 | `EvtSubscribe` 推送（线程/回调更复杂，二期后评估） |
| 优先级 | 先 UAC 一次性读取（Security 可用），再做普通通道监控，最后决定提权通道的监控 | — |

两个功能有一个交叉点：**Security 通道的监控**。一次性提权助手读完即退出，若每 2 s 轮询都重新弹 UAC 不可接受；要么让助手常驻（会话协议），要么只对非提权通道开放监控，要么改成“重启为管理员”。本计划建议把“提权通道监控”单列为 Phase C，先交付普通通道监控。

---

## 1. 现状与阻塞点

### 1.1 已实现（一期）

- 原生事件日志：`EventLogBackend`（`EvtQuery` / `EvtNext` / `EvtRender` / `EvtFormatMessage`，publisher 元数据跨打开缓存）。
- `EventLogProvider`：20,000 条上限、最新在前、UTC → 本地时间、`SystemTime` 保留绝对时刻。
- File ▸ Windows Event Log、全部通道对话框、`.evtx` 文件、分档继续加载。
- 只读；事件日志文档禁用 Refresh / Monitor；不写最近文件。

### 1.2 未实现（本计划目标）

- Security 通道只有错误提示，没有可点击的提权入口（`REQ-EVT-08` 目前只要求提示）。
- 事件日志没有实时监控（`REQ-EVT-05` 明确禁用 Monitor）。
- `ElevatedFileReader` 是 **Linux-only**（`pkexec`），Windows 上 `isSupported() == false`；`main.cpp` 的 `--elevated-stream` 也只服务文件。
- `LogWatcher` 基于 `QFileSystemWatcher` + 文件尾轮询，只支持**在末尾追加**。
- `LogTableModel` 只有 `appendRows(firstRow)`，没有向顶部插入的 `prependRows()`。
- 事件日志“最新在前”，新事件必须插到**第一行**，而现有监控 UI 是“跟随底部”（`isAtBottom` / `scrollToBottomNow` / “New lines: N ▼”）。
- `WindowsEventXmlFormat::parseEventXml()` 不解析 `<EventRecordID>`，无法做增量查询。
- `EventLogProvider` 构建后不可变，`IEventLogBackend` 只有一次性 `readChannel(channel, maxRecords)`。

### 1.3 约束（不得违反）

- `REQ-PLAT-01` / `CON-9`：平台条件编译只能出现在 `src/platform/`；UI 通过 `isSupported()` 决定入口。
- `REQ-PLAT-08`：不调用外部进程，不依赖 shell。Linux 的 `pkexec` 是已记录的例外；Windows 的 `ShellExecuteEx(runas)` 启动**本程序自身辅助模式**也需要在 spec 中新增同类例外条款。
- `CON-4`：不引入第三方运行时依赖；Windows 系统 DLL（`wevtapi`、`shell32`、`advapi32`）允许。
- 只读：不写事件日志、不清空日志。
- 所有可见字符串 `tr()` + 手工补 `src/i18n/logviewer_zh_CN.ts`。
- 测试仍要求跨平台可跑（Windows 专属步骤用 `QSKIP` 守卫）。

---

## 2. 实施前必须完成的需求修订（spec 先行）

### 2.1 新增需求

| 编号 | 需求 | 优先级 |
| --- | --- | --- |
| `REQ-EVT-10` | **Windows UAC 提权读取**：对无权限的事件日志（典型为 Security，或用户无权访问的通道/文件），错误提示旁提供“以管理员权限打开…”；经 UAC 授权后由本程序的高权限辅助进程只读目标并把记录流回、显示为非提权主进程中的只读文档；取消/失败保留当前文档；提权文档有明确标记；提权读取不写任何用户路径（管道或私有快照） | 必须 |
| `REQ-EVT-11` | **事件日志实时监控**：对**实时通道**（不含 `.evtx` 静态文件）提供 Monitor；按 `EventRecordID` 增量轮询，新事件出现在**第一行**；用户停留在顶部时自动跟随，滚离顶部时显示“New events: N ▲”按钮；新事件同样参与过滤/查找/级别计数；不重复、不丢行；检测到通道被清空/重置时重建文档；`.evtx` 文件保持禁用 Monitor | 必须 |

### 2.2 需要修订的既有条目

- `REQ-EVT-05`：由“事件日志视图不支持 Monitor 与 Refresh”改为“只读；Refresh 仍禁用；Monitor 仅对实时通道开放（`.evtx` 除外）”。
- `REQ-PLAT-08`：增加 Windows 例外——允许 `ShellExecuteExW(runas)` 启动**本程序自身**的内部辅助模式（类似 `REQ-CLI-11` 的 `--elevated-stream`），但不得调用 `wevtutil.exe` / PowerShell / cmd。
- `REQ-CLI-11` 附近：新增内部辅助模式 `--event-log-stream`（Windows）或 `--event-log-session`（若实现会话助手），固定参数、GUI 初始化前执行。
- `§7 范围外`：移除“Windows 事件日志实时订阅与 tail 式监控”“Windows 事件日志提权读取（UAC）”两行，改为指向本计划或移入已实现。
- `§8 验收标准`：新增 `AC-22`（UAC 提权打开 Security）与 `AC-23`（通道实时监控）。
- `docs/design-doc.md`：§4.1（提权/监控数据流）、§4.9（平台表新增 Windows 提权行）、§6.3（菜单/状态栏）、§10（测试）、§13（风险）、新增 §16.13 实施记录。
- 双 README：功能、权限、UAC、监控限制。

---

## 3. UAC 提权读取设计

### 3.1 方案对比

| 方案 | 做法 | 优点 | 缺点 | 工作量 |
| --- | --- | --- | --- | --- |
| **A. 重启为管理员（备选）** | `ShellExecuteEx(runas)` 启动一个新的本程序实例，带上当前通道/文件参数；原实例关闭或保留 | 无需进程间协议；提权实例里“加载更多”和监控都自然可用 | 重新打开窗口；丢失当前文档/过滤/滚动；设置/最近文件共享；多实例语义变化 | 1–2 d |
| **B. 辅助进程 + 命名管道（推荐）** | 主进程创建 `\\.\pipe\log-viewer-…`，`runas` 启动 `--event-log-stream`；辅助进程用 `wevtapi` 读取，按帧写回；主进程组装 `EventLogProvider` | 主进程保持非提权；只读目标；与 Linux `pkexec` 只读快照语义一致；可扩展为会话协议 | 需要设计帧格式、管道 ACL、超时/取消、等待光标/线程 | 3–5 d |
| **C. 辅助进程 + 临时文件** | 辅助进程把记录写成私有临时快照，主进程读取后删除 | 实现简单，易调试 | 事件日志可能很大；落盘/清理；提权进程写用户目录有安全风险；崩溃残留 | 2–3 d |
| **D. 常驻提权服务** | Windows 服务/计划任务长期运行 | 无 UAC 重复提示 | 安装/权限/安全面过大，超出本项目范围 | 不做 |

**推荐 B**。同时建议协议从第一天就按**请求/响应**设计（`OPEN` / `READ_SINCE` / `CLOSE`），即使 Phase A 只用到 `OPEN`，这样 Phase C 的 Security 监控不需要重写一套通道。

### 3.2 进程与数据流（方案 B）

```
主进程（普通权限）
  ├─ CreateNamedPipeW("\\.\pipe\log-viewer-<pid>-<uuid>",
  │     PIPE_ACCESS_INBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE,
  │     PIPE_TYPE_BYTE | PIPE_REJECT_REMOTE_CLIENTS, ACL: 当前用户/BA/SY)
  ├─ ShellExecuteExW(verb="runas",
  │     file=applicationFilePath(),
  │     parameters="--event-log-stream <pipe> <channel|file> <maxRecords>",
  │     SEE_MASK_NOCLOSEPROCESS)
  ├─ 工作线程 ReadFile/解析帧（UI 线程只收结果）
  └─ 完成后 CloseHandle(process/pipe)，删除临时状态
辅助进程（高权限，同一 exe）
  ├─ main() 在 QApplication 之前识别 --event-log-stream
  ├─ 只接受固定 4 个参数；拒绝 shell 元字符/额外参数
  ├─ wevtapi：EvtQuery → EvtRender → EvtFormatMessage（复用 EventLogBackend 的渲染逻辑）
  └─ 写帧：MAGIC/VERSION、HEADER(通道名, 条数)、逐条 XML+Message、DONE/ERROR
```

要点：

- **管道名随机 + 先创建**：防止本地进程抢注；`PIPE_REJECT_REMOTE_CLIENTS`；ACL 只允许当前用户 SID + `BUILTIN\Administrators` + `NT AUTHORITY\SYSTEM`（提权后的同一用户 SID 不变，仍可连接）。
- **参数固定、不经 shell**：`ShellExecuteEx` 的 `lpParameters` 逐个加引号；辅助进程严格校验参数个数与类型；不接受任意命令。
- **只读**：辅助进程仅调用 `EvtQuery`/`EvtRender`/`EvtFormatMessage`，不写日志、不写用户路径。
- **帧格式**：字节模式 + 长度前缀（UTF-8），例如
  `uint32 magic`、`uint16 version`、`uint8 type`、`uint32 length`、`bytes payload`；
  类型：`HEADER`、`RECORD`、`DONE`、`ERROR`。不要用换行分隔 XML/消息（消息可含换行）。
- **错误传递**：`ERROR` 帧携带 Win32 错误码 + 可读文本（`ERROR_ACCESS_DENIED`、`15007` 等），UI 复用现有错误映射。
- **取消/超时**：UAC 取消时 `ShellExecuteEx` 失败（`ERROR_CANCELLED`），直接返回；读取阶段用工作线程 + `PeekNamedPipe`/超时；用户取消时关闭管道，辅助进程读到断管后自行退出；最后才考虑 `TerminateProcess`。
- **生命周期**：文档关闭/替换/程序退出时停止读取、关闭管道；用 Job Object（`JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`）确保主进程异常退出时辅助进程不残留。
- **测试 seam**：像 Linux `setProgramOverrideForTesting()` 一样，允许测试用普通子进程/脚本模拟辅助进程；真实 UAC 仅手工验证。

### 3.3 UI 接入

- 错误路径：`EventLogProvider::openChannel()` 失败且错误为 `ERROR_ACCESS_DENIED` 时，`MainWindow` 复用状态栏“以管理员权限打开…”按钮（现有 `m_elevateButton`），点击走事件日志提权分支；不能复用 Linux 的 `ElevatedFileReader::createSnapshot()` 文件路径。
- 提权模式标记：文档标签追加 `(elevated)`（与 Linux 的 `(elevated snapshot)` 区分）。
- 提权文档的 Refresh：保持禁用（事件日志本身已禁用 Refresh）；“加载更多”需要重新提权（见下）。
- 加载更多：
  - 一次性助手：再弹一次 UAC；在首次提权对话框中提示“可一次加载全部以免重复授权”。
  - 会话助手（Phase C）：同一管道发送 `READ_SINCE`，不再弹 UAC。
- 提权读取成功后再 `setProvider()`；失败保留当前文档（与现有 `loadEventLog()` 语义一致）。
- 非 Windows：UI 不显示任何提权入口（`isSupported()` 为 false）。

### 3.4 关键文件/接口

| 文件 | 计划改动 |
| --- | --- |
| `src/platform/ElevatedFileReader.{h,cpp}` 或新增 `src/platform/ElevatedEventLog.{h,cpp}` | Windows 提权实现；建议新增独立文件，避免把文件快照与事件日志协议混在一起；非 Windows 桩 |
| `src/main.cpp` | 在 `QApplication` 之前识别 `--event-log-stream`（与 `--elevated-stream` 同层） |
| `src/platform/EventLogBackend.cpp` | 抽出“渲染一条记录”的可复用函数，供一次性读取与辅助进程使用；避免两套字段映射 |
| `src/core/EventLogProvider.cpp` | 接收辅助进程返回的记录（与现有 `load()` 复用） |
| `src/ui/MainWindow.{h,cpp}` | 错误 → 提权按钮 → 提权加载；`(elevated)` 标记；加载更多重新授权 |
| `src/CMakeLists.txt` | 新增文件；Windows 已链接 shell32/advapi32/wevtapi，可能补 `ole32`/`uuid` |
| `src/i18n/logviewer_zh_CN.ts` | 新增文案 |

### 3.5 测试

- 协议单测（无 UAC）：父进程创建管道，启动**非提权**的辅助模式读 Application 通道，校验帧、条数、错误帧、断管退出。
- 提权 UI 用例：用测试替身（脚本/子进程）模拟成功/取消/错误；真实 `runas` 手工验证（会弹 UAC）。
- Security 手工验收：普通用户点击提权 → UAC → 最近 20k 条可见；取消后保留原文档；关闭后辅助进程不残留。

---

## 4. 事件日志实时监控设计

### 4.1 方案对比

| 方案 | 做法 | 优点 | 缺点 | 工作量 |
| --- | --- | --- | --- | --- |
| **A. `QTimer` + `EventRecordID` 增量查询（推荐）** | 每 2 s 查询 `*[System[EventRecordID > N]]`，只取新记录并插到顶部 | 复用现有 `wevtapi` 渲染/消息缓存；实现可控；无回调线程 | 轮询有延迟；需要处理清空/重置 | 1–1.5 d 后端 + 2–3 d UI/模型 |
| B. `EvtSubscribe` 推送 | 注册回调，事件到达即通知 | 实时性最好 | 回调线程、生命周期、错误恢复复杂；与现有同步架构差异大 | 3–5 d |
| C. 定时重查最新 N 条并重建 provider | 每 2 s 重新读 20k | 实现最简单 | 重复渲染 20k 条消息，卡顿；滚动/过滤重置 | 0.5 d（但不推荐） |
| D. 改造 `LogWatcher` | 文件监视器复用到通道 | 表面复用 | 数据源语义完全不同，耦合更差 | 不建议 |

**推荐 A**，并把 `EvtSubscribe` 留作后续优化。

### 4.2 数据与增量协议

1. **提取 `EventRecordID`**
   - 在 `WindowsEventXmlFormat::parseEventXml()` 中解析 `<EventRecordID>`，或由 `EventLogBackend` 在渲染后提取。
   - 建议给 `LogEntry` 增加 `qint64 eventRecordId = 0`（0 = 无/不适用），`EventLogRecord` 同步增加；避免塞进 `extra` 导致多出一列。
2. **后端增量查询**
   - 新增 `IEventLogBackend::readChannelSince(channel, afterRecordId, maxRecords)`：
     - 查询 `*[System[EventRecordID > <after>]]`；
     - 方向：仍用 `EvtQueryReverseDirection`，返回“新记录中的最新在前”，直接 prepend 即保持表格顺序；
     - 复用 `EvtRender` / `EvtFormatMessage` / 元数据缓存；
     - 返回 `records`、`newestRecordId`、`truncated`、`error`。
   - 清空/重置检测：
     - 每次轮询额外取“当前最新 1 条”（反向、无过滤）；若其 `EventRecordID < lastRecordId`，说明日志被清空/重建 → 触发 `documentRebuilt`。
     - 若通道被禁用/删除 → `watchingFailed`。
   - 轮询间隔：默认 2 s（可后续做成设置）；单次最多取 N 条（如 500），超过则分页或显示“新事件过多”。
3. **Provider 可变化**
   - `EventLogProvider` 增加 `prepend(const QVector<EventLogRecord> &newestFirst)`，更新 `m_entries`、`m_info.lineCount`、级别计数缓存（增量或失效重算）。
   - 记录 `m_newestRecordId` / `m_oldestRecordId`。
   - `requestedLimit` 与继续加载语义保持不变；监控期间“加载更多”先停止监控、替换文档、再恢复。
4. **监控对象**
   - 新增 `src/core/EventLogWatcher.{h,cpp}`（QObject + QTimer），持有 `IEventLogBackend`、通道名、`lastRecordId`；信号：
     - `recordsPrepended(int count)`
     - `documentRebuilt()`
     - `watchingFailed(const QString &)`
   - 只支持实时通道；`.evtx` 文件禁用 Monitor（静态文件，且可能被占用）。
   - 提权文档：Phase C 前禁用 Monitor（或提示“提权通道监控需要重启为管理员/会话助手”）。

### 4.3 模型与视图：顶部插入 + 顶部跟随

这是本计划的主要 UI 工作量。

1. **`LogTableModel::prependRows(int count)`**
   - 无过滤：`beginInsertRows(QModelIndex(), 0, count - 1)` + `endInsertRows()`；清空/失效行缓存与 find 行缓存。
   - 有过滤：现有 `m_visibleRows` 存的是 provider 行号；provider 前插后旧行号整体 +count。需要：
     - 先把 `m_visibleRows` 中所有值 +count；
     - 对新行 `0..count-1` 逐条 `entryMatches()`，把匹配行插到可见序列前端；
     - `beginInsertRows` 的区间 = 新增的可见行数。
   - 行高/展开缓存：当前视图用“源序号 + 源内行号”作键；事件日志的 `firstLine` 若继续用 1..N 序号，前插后全体变化。建议：
     - 方案一：事件日志行键改用 `EventRecordID`（稳定、唯一），表格 Line 列仍显示序号或改为 Record ID（需 spec 决策）；
     - 方案二：每次前插后清空行高/展开缓存（实现简单，但 2 s 一次可能抖动）。
   - 推荐方案一，并顺带解决“前插后序号重排”的性能问题。
2. **`LogTableView` 顶部跟随**
   - 新增 `isAtTop()` / `scrollToTopNow()`。
   - “New events” 按钮改为顶部显示（`New events: N ▲`），点击跳到顶部并清零；滚离顶部时计数，回到顶部自动清零。
   - 现有底部按钮/`isAtBottom` 保留给文件监控；按文档类型选择方向。
3. **`MainWindow` 监控分支**
   - `updateDocumentUi()` 的 `canMonitor` 当前要求 `m_source`（`LogSource`）；改为：
     - 文件文档：现有 `LogWatcher`；
     - 事件日志通道：`EventLogWatcher`；
     - `.evtx`/demo/提权：禁用。
   - `startMonitor()` / `stopMonitor()` 按类型分派；`onRowsAppended` 之外新增 `onEventLogRowsPrepended`，更新级别计数、顶部按钮、监控标签。
   - `Monitor` 菜单文案/状态栏：顶部跟随时显示 `Monitor: on`，暂停时显示 `Monitor: on (N new events)`。
   - `closeEvent` / 文档切换：停止事件日志轮询，断开管道/定时器。
4. **过滤、查找、级别计数**
   - 新事件照常进入 `levelCounts()`；`FilterPanel::updateLevelCounts()` 刷新。
   - Find 的缓存按行号存储，前插后需失效（`m_findRowsValid = false`、清缓存）。
   - 若过滤隐藏了新事件，顶部按钮计数只统计“可见新增行”还是“全部新增行”？建议统计全部新事件，并在状态栏标注“（过滤中）”。

### 4.4 边界与限制

- **新事件堆积**：初始 20k + 监控新增。建议设硬上限（如 200k）或滚动窗口；超过后暂停监控并提示，避免内存无限增长。该上限应做成设置项（`eventLog.monitorLimit`）或先写死并文档说明。
- **通道清空**：`EventRecordID` 回退/通道重建 → 重建文档并提示。
- **事件突发**：一次轮询超过 500 条时，分页/截断并显示“新事件过多，已暂停/仅显示最新 N 条”。
- **`.evtx` 文件**：不监控；Windows 对归档文件通常不追加。
- **提权通道**：Phase C 决策；Phase A/B 期间监控禁用并提示。
- **性能**：2 s 轮询 + 增量消息格式化（只格式化新记录）；`EventLogBackend` 的 provider 元数据缓存跨轮询复用，已具备。

---

## 5. 实施顺序与估算

| 阶段 | 内容 | 估算 |
| --- | --- | --- |
| P0 | spec/design/README 修订（REQ-EVT-10/11、REQ-EVT-05、REQ-PLAT-08 例外、AC-22/23） | 0.5–1 d |
| P1 | UAC 辅助进程：管道协议、`runas`、参数校验、错误/取消、Job Object；非 Windows 桩 | 3–4 d |
| P2 | UI 接入：错误 → 提权按钮 → 提权加载、`(elevated)` 标记、加载更多重新授权；测试替身 | 1 d |
| P3 | `EventRecordID` 提取、`readChannelSince`、清空检测、`EventLogWatcher` + mock 测试 | 1.5 d |
| P4 | `prependRows`、顶部跟随/按钮、行键稳定化、过滤/查找缓存处理、MainWindow 分派 | 2–3 d |
| P5 | 提权通道监控（Phase C）：会话助手或“重启为管理员”二选一；不做则文档标注 | 2–3 d（可延后） |
| P6 | i18n、双 README、Windows 手工验收（UAC + 实际新事件）、Linux 构建回归 | 1–1.5 d |
| **合计** | 不含 P5 | **约 9–12 人日** |
| **合计** | 含 P5 | **约 11–15 人日** |

---

## 6. 验收标准（建议写入 spec §8）

- **AC-22（UAC）**：普通用户打开 Security 通道 → 出现“以管理员权限打开…” → UAC 确认后显示最近 20,000 条（时间本地、最新在前）；取消授权后保留原文档、无残留辅助进程；关闭文档/退出程序后管道与进程清理干净。
- **AC-23（监控）**：对 Application/System 勾选 Monitor 后，用外部工具（手工）产生新事件，2 s 内出现在第一行；滚离顶部时显示 `New events: N ▲`，点击回顶部；过滤/查找/级别计数同步；连续运行 10 分钟无重复、无丢行、无持续 CPU 飙升；清空通道后文档自动重建或明确提示；`.evtx` 文件不显示 Monitor 可用。
- 既有 AC-01…AC-21 与 `ctest` 全部保持通过（Windows 18/18，Linux 桩编译/测试不受影响）。

---

## 7. 风险与对策

| 风险 | 影响 | 对策 |
| --- | --- | --- |
| 管道 ACL/命名被本地进程利用 | 权限提升安全风险 | 随机管道名 + `FILE_FLAG_FIRST_PIPE_INSTANCE` + 仅当前用户/BA/SY + `PIPE_REJECT_REMOTE_CLIENTS`；辅助进程校验参数；不写用户路径 |
| 提权助手残留 | 安全/资源泄漏 | Job Object `KILL_ON_JOB_CLOSE`；父进程断管/退出即终止；文档关闭显式关闭 |
| UAC 重复提示（加载更多/监控） | 体验差 | 首次提示“加载全部以免重复授权”；Phase C 会话助手/重启为管理员 |
| 监控前插导致行高/展开/序号错乱 | UI 错乱 | 行键改用 `EventRecordID`；前插后失效缓存；UI 测试覆盖过滤与前插组合 |
| 轮询期间 UI 卡顿 | 体验差 | 每次只取新增记录、批次上限；必要时移到工作线程；实测 2 s 间隔 CPU |
| 通道清空/重置 | 行号/记录错乱 | 每次比较最新 `EventRecordID`；回退即重建 |
| 新事件无限增长 | 内存膨胀 | `eventLog.monitorLimit` 硬上限 + 暂停提示 |
| Security 监控需常驻提权 | 安全面扩大 | 默认不支持；Phase C 单独立项，限制生命周期与权限范围 |
| Linux/非 Windows 构建回归 | 编译失败 | 所有 Windows 代码仅在 `platform/`，提供 `isSupported()==false` 桩；CI/测试保持 |

---

## 8. 待决策问题（实施前确认）

1. **提权模型**：辅助进程 + 管道（推荐）还是“重启为管理员”？
2. **Security 监控**：Phase C 做会话助手，还是只允许“重启为管理员”，或干脆不监控 Security？
3. **管道 vs 临时快照**：是否接受命名管道的实现复杂度（推荐管道）？
4. **监控新增上限**：默认 200k 硬上限 / 滚动窗口 / 不限制，哪个？
5. **Line 列语义**：事件日志的 Line 列继续用序号（前插后重排），还是改为 `EventRecordID`（稳定、唯一）？
6. **轮询间隔**：固定 2 s，还是写入设置（1/2/5 s）？
7. **监控与“加载更多”交互**：加载更多时停止监控并重建，还是保留监控状态？

---

## 9. 建议的最小可交付（若时间有限）

1. **UAC 一次性读取**（Security 可看 20k，支持“加载全部”减少重复授权）。
2. **普通通道监控**（Application/System/Setup），顶部跟随 + 新事件按钮 + 清空检测。
3. Security 监控与“加载更多不重新授权”留到后续；文档明确限制。

这样能先交付两个用户可见的核心价值，同时避免常驻提权进程和复杂的会话协议。

---

## 10. 实施结果（2026-11-24）

已按本计划完成 P1–P4/P6（不含 Phase C），关键决策与实现：

- 提权模型：**辅助进程 + 命名管道**（`ElevatedEventLog`，`--event-log-stream`，Job Object 防残留）。
- Security 监控：**本轮不做**；Security 仅支持 UAC 提权查看。
- 监控新增上限：**50,000 条后自动暂停**。
- Line 列：事件日志改为 **EventRecordID**（表头 `Record ID`）。
- 轮询间隔：**固定 2 秒**。
- 监控中“加载更多”：**停止监控、重建后自动恢复**。
- 验证：`ctest` 18/18；新增非提权管道助手测试与真实 `readChannelSince` XPath 断言；真实 UAC 与真实新事件注入待人工验证。

详见 `docs/design-doc.md` §16.13 与 `docs/spec.md` 1.45。

---

*计划生成：2026-11-24；实施结果更新：2026-11-24。*
