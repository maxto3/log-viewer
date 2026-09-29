# Log Viewer 技术设计文档

| 项目 | 内容 |
| --- | --- |
| 文档版本 | 1.1 |
| 日期 | 2026-09-29 |
| 关联需求 | `spec.md`（需求规格说明书 v1.1） |
| 状态 | 待用户确认后进入实施 |
| 目标平台 | **Windows 11 x64**（首要，v1 交付并验证） / **Linux x64**（源码级支持，v1 不做构建验证） |
| 设计参考 | KDE Plasma 的 KSystemLog（信息组织方式），视觉规范以 `spec.md` §5 为准 |

## 目录

1. [目标与技术选型](#1-目标与技术选型)
2. [总体架构](#2-总体架构)
3. [目录结构](#3-目录结构)
4. [核心模块设计](#4-核心模块设计)
5. [高亮引擎](#5-高亮引擎)
6. [界面布局与菜单结构](#6-界面布局与菜单结构)
7. [交互、快捷键与命令行接口](#7-交互快捷键与命令行接口)
8. [设置项与持久化](#8-设置项与持久化)
9. [构建、运行与打包（双平台）](#9-构建运行与打包双平台)
10. [测试计划](#10-测试计划)
11. [性能预算与优化手段](#11-性能预算与优化手段)
12. [里程碑与验收](#12-里程碑与验收)
13. [风险与对策](#13-风险与对策)
14. [开发约定](#14-开发约定)
15. [修订记录](#15-修订记录)

---

## 1. 目标与技术选型

构建跨平台桌面日志查看器，针对"大文件 + 高密度表格 + 实时监控"场景。

| 维度 | 选择 | 理由 |
| --- | --- | --- |
| Qt | **Qt 6.8.x LTS（LGPLv3）** | Windows：官方 6.8.3 `msvc2022_64` 预编译包；Linux：发行版 6.8.x 包（Debian 13 = 6.8.2）或 aqtinstall `gcc_64`（6.8.3）。Qt 5.15 已进入维护末期 |
| UI 技术 | **Qt Widgets** | 表格密集、需要精确列宽/行高与自绘代理，Widgets 比 QML 更成熟可控（KSystemLog 同路线） |
| 语言 | C++20 | `QStringView`、结构化绑定、`std::span` 便于解析器与渲染代码 |
| 构建 | CMake ≥ 3.25 + Ninja | Windows 复用 VS 2022 自带 CMake/Ninja；Linux 使用系统 CMake/Ninja |
| 编译器 | MSVC 2022 x64（Windows）/ GCC ≥ 13 或 Clang ≥ 16（Linux） | 两种编译器都必须无警告通过（`/W4` 与 `-Wall -Wextra`） |
| Qt 安装 | Windows：`aqtinstall` → `D:\Qt`；Linux：发行版软件包优先 | 免账号、可脚本化；Linux 侧避免额外下载 |
| 平台差异承载 | `src/platform/` 单一位置 | 满足 REQ-PLAT-01，避免平台宏散落 |
| 测试 | Qt Test（`QTest`）+ CTest | 跨平台一致 |
| i18n | Qt Linguist（`.ts` / `.qm`） | Qt 原生，支持运行时 `LanguageChange` 重译 |
| 第三方依赖 | **无**（Linux 编码回退使用 glibc 内置 `iconv`） | 避免 Scintilla/KSyntaxHighlighting/ICU 等重依赖的部署成本 |

## 2. 总体架构

```
┌───────────────────────── UI 层（Qt Widgets，可翻译） ──────────────────────────┐
│ MainWindow                                                                    │
│  ├─ FilterPanel（Search & Filter 分组框：FindPanel + SearchFilterPanel + …）  │
│  ├─ LogTableView + LogItemDelegate（表格 + 自绘渲染 + 双击复制）              │
│  ├─ DetailPane（字段表 + 完整消息，右侧/底部可切）                            │
│  ├─ AboutDialog（设置经 Settings 菜单直改，无需独立设置对话框）           │
│  └─ StatusBar                                                                 │
├────────────────────────── 模型 / 高亮层 ──────────────────────────────────────┤
│ LogTableModel   FilterProxyModel   Matcher(全词/通配/正则)   TimeRangeFilter  │
│ HighlightTheme(Light+/Dark+)   MessageHighlighter   SnippetDetector/Tokenizer │
├──────────────────────────── 核心层（无 UI 依赖） ─────────────────────────────┤
│ LogFormatRegistry → ILogFormat 实现族（tracing/syslog/…/generic）             │
│ LogSource（行索引 + 按需读取 + LRU 行缓存）                                    │
│ LogDocument（多文件合并：时间交错、列结构校验）                                │
│ TimestampParser   LevelParser   LogWatcher(tail -f)                           │
├─────────────────────────── 平台抽象层 src/platform/ ──────────────────────────┤
│ PlatformInfo(OS/字体/配置路径)   EncodingBackend(Win32 MB2WC | iconv)         │
│ FileAssociation(注册状态查询/引导)   ConsoleAttach(Windows --help/--version)  │
├───────────────────────────── 基础设施 ───────────────────────────────────────┤
│ SettingsStore(QSettings INI)   ThemeManager   TranslationManager   JobRunner  │
│ CliParser(QCommandLineParser，无 UI 依赖)                                     │
└──────────────────────────────────────────────────────────────────────────────┘
```

数据流：

```
磁盘字节 → EncodingDetector(平台回退后端) → 行分割/偏移索引 → LogFormatRegistry.probe()
        → ILogFormat.parse() → LogEntry{time,level,thread,target,extra,message}
        → LogTableModel → FilterProxyModel(级别/时间/关键词) → LogItemDelegate
        → 关键词/片段高亮渲染 + DetailPane(完整消息, QSyntaxHighlighter)
```

并发模型：全部重活（建索引、过滤、查找、导出、监控增量解析）跑在 `QThreadPool` 工作对象中，结果通过信号回主线程；每个作业带进度与取消标志。跨平台不使用任何平台专有线程/IO API。

## 3. 目录结构

```
log-viewer/
├─ CMakeLists.txt                 # 顶层：C++20、Qt6 组件、i18n、测试、install/CPack
├─ CMakePresets.json              # windows-msvc-qt6-release / linux-gcc-release
├─ .gitignore
├─ docs/
│   ├─ spec.md                   # 需求规格说明书（唯一权威来源）
│   └─ design-doc.md             # 技术设计与实施记录
├─ README.md  README.zh_CN.md     # 用户文档（含双平台构建与未验证声明）
├─ scripts/
│   ├─ install-qt.ps1             # Windows：aqtinstall 下载 Qt 6.8.3 msvc2022_64
│   ├─ build.ps1  test.ps1  run.ps1  package.ps1      # Windows（package.ps1→dist\）
│   ├─ register-association.ps1   # Windows：HKCU 文件关联（-Unregister 撤销）
│   ├─ build.sh   test.sh     run.sh  package-deb.sh  # Linux（未验证）
│   └─ register-association.sh    # Linux：xdg-mime 引导（未验证）
├─ packaging/
│   ├─ linux/
│   │   ├─ log-viewer.desktop.in          # Exec=log-viewer %F, MimeType=text/x-log;…
│   │   ├─ org.logviewer.LogViewer.metainfo.xml   # AppStream
│   │   ├─ icons/hicolor/*/apps/log-viewer.{svg,png}
│   │   └─ deb/CPackDeb.cmake             # 包名/依赖/安装路径
│   └─ windows/log-viewer.{ico,rc.in}     # 应用图标（编译进 exe，由 Linux 图标渲染）
├─ src/
│  ├─ main.cpp
│  ├─ app/
│  │   ├─ Application.{h,cpp}     # 启动流程、样式加载
│  │   ├─ CliParser.{h,cpp}       # 命令行解析（无 UI 依赖，可单测）
│  │   ├─ SettingsStore.{h,cpp}   # QSettings 包装（路径经 PlatformInfo）
│  │   ├─ ThemeManager.{h,cpp}    # Light/Dark/System + 调色板与样式表
│  │   ├─ TranslationManager.{h,cpp}
│  │   └─ JobRunner.{h,cpp}
│  ├─ platform/
│  │   ├─ PlatformInfo.{h,cpp}            # 平台名、默认等宽字体、配置目录
│  │   ├─ EncodingBackend.{h,cpp}         # 统一接口 + 两套实现（#if 隔离）
│  │   ├─ FileAssociation.{h,cpp}         # 注册状态查询 / 平台引导信息
│  │   └─ ConsoleAttach.{h,cpp}           # Windows 控制台附着（空实现于 Linux）
│  ├─ core/
│  │   ├─ LogLevel.{h,cpp}  LogEntry.h  LogFormat.{h,cpp}
│  │   ├─ LogFormatRegistry.{h,cpp}
│  │   ├─ formats/            # 12 个格式实现（见 §4.1）
│  │   ├─ EncodingDetector.{h,cpp}        # BOM/UTF-8 校验 → 委托 EncodingBackend
│  │   ├─ LineIndex.{h,cpp}  LogSource.{h,cpp}  LogDocument.{h,cpp}
│  │   ├─ TimestampParser.{h,cpp}  LogWatcher.{h,cpp}
│  ├─ model/     IEntryProvider(EntryProvider)  LogTableModel  FilterProxyModel  Matcher  TimeRangeFilter  FilterSpec
│  ├─ highlight/ HighlightTheme  LevelPalette  MessageHighlighter  SnippetDetector  Tokenizer
│  ├─ ui/        MainWindow  FilterPanel  FindPanel  SearchFilterPanel  LevelFilterBar
│  │             TimeRangePanel  LogTableView  LogItemDelegate  DetailPane
│  │             AboutDialog  StatusBarWidget  DemoData（--demo 自检数据）
│  └─ i18n/logviewer_zh_CN.ts
├─ resources/
│  ├─ resources.qrc  icons/log-viewer-*.png  themes/vscode_tokens.json
└─ tests/
   ├─ CMakeLists.txt  data/（附件真样本 + 各格式样本 + CRLF/LF/GBK/UTF-16 样本）
   ├─ tst_formats  tst_matcher  tst_timestamp  tst_snippets  tst_lineindex
   ├─ tst_watcher  tst_document  tst_highlight  tst_encoding  tst_cli
   └─ bench_1m_lines（可选构建目标）
```

## 4. 核心模块设计

### 4.1 日志格式注册表与解析器

接口（核心层无 UI 依赖）：

```cpp
struct ColumnSchema {                       // 列定义
    enum Id { Time, Level, Thread, Target, Pid, Host, File, Line, Message, Extra };
    struct Column { Id id; QString extraKey; QString titleKey; int minWidth; };
    QVector<Column> columns;
};

struct ParseContext {                       // 逐源的解析状态
    int year, month, day;                   // 推断日期（用于无年份/无日期格式）
    QDateTime lastTime;                     // 单调性校验与时间填补
    bool dateInferred = false;
};

struct ParsedEntry {
    QDateTime time;                         // 可能无效（无时间戳格式）
    LogLevel level = LogLevel::Other;
    QString rawLevel;                       // 原始级别文本
    QString thread, target, host, pid;
    QHash<QString, QString> extra;          // 动态列
    QString message;                        // 可含 '\n'（续行合并后）
    int physicalLineStart = 0, physicalLineCount = 1;
};

class ILogFormat {
public:
    virtual ~ILogFormat() = default;
    virtual QString id() const = 0;                 // "tracing", "syslog5424", ...
    virtual QString displayNameKey() const = 0;     // 翻译键
    virtual int probe(const QStringList &head, ParseContext &ctx) const = 0;   // 0..100
    virtual bool parse(const QStringList &lines, int &i, ParseContext &ctx,
                       ParsedEntry &out) const = 0; // 允许块解析（多行条目）
    virtual ColumnSchema schema() const = 0;
};
```

内置实现（`src/core/formats/`）：

| 文件 | id | 锚点要点 | 产出字段 |
| --- | --- | --- | --- |
| `format_tracing.cpp` | `tracing` | `ISO8601(7位小数,±HH:MM)  LEVEL  [线程名 ]ThreadId(N)  target: message` | time, level, thread, target, message |
| `format_syslog.cpp` | `syslog3164` / `syslog5424` / `journalctl_short` | RFC3164 `Mmm dd HH:MM:SS host tag[pid]: msg`；RFC5424 `<PRI>VER ts host app proc msgid [SD] msg` | time, level(由 PRI 映射), host, pid, target, message |
| `format_journal_json.cpp` | `journal_json` | 每行 JSON，键映射 `MESSAGE/__REALTIME_TIMESTAMP/PRIORITY/_HOSTNAME/SYSLOG_IDENTIFIER` | time, level, host, pid, target, message |
| `format_jsonlines.cpp` | `json_lines` | 每行 JSON，键集 {time,timestamp,ts,@timestamp} / {level,severity,lvl,log_level} / {msg,message,text}；其余键 → extra | time, level, message, extra |
| `format_logfmt.cpp` | `logfmt` | `k=v` 或 `k="v"`，同样键映射 | 同上 |
| `format_csv.cpp` | `csv_tsv` | 首行表头 + 分隔符自动判定（`,` `;` `\t` `\|`）；列名大小写不敏感映射 | 按表头 + 标准映射 |
| `format_python.cpp` | `python_logging` | `2026-09-28 22:15:43,303 - LEVEL - logger - msg`（分隔符 `-`/`–`/`:` 容忍） | time, level, logger→target, message |
| `format_serilog.cpp` | `serilog` | `2026-09-28 22:15:43.303 +08:00 [INF] msg` 及 `[22:15:43 INF] msg`（后者日期推断） | time, level(INF/WRN/ERR/DBG/VRB/FTL 映射), message |
| `format_log4j.cpp` | `log4j_logback` | `2026-09-28 22:15:43,303 [thread] LEVEL logger - msg`；容忍无毫秒 | time, level, thread, target, message |
| `format_windows_event.cpp` | `wevt_text` | 块格式：`TimeCreated : …` / `LevelDisplayName : …` / `Message : …`，空行或下一 `TimeCreated` 结束块 | time, level, extra(ProviderName, EventId…), message |
| `format_iis.cpp` | `iis_w3c` | `#Fields:` 表头 + 空格分隔数据行 | time(date+time 合并), level(由 sc-status 推断，可选), extra, message |
| `format_generic.cpp` | `generic` | 可选时间戳 + 可选 `[LEVEL]`/`LEVEL:` + 其余为消息；无时间戳时单列消息 | time?, level?, message |

**探测算法**

1. 取文件前 200 个物理行做探针样本（读 64 KB 足矣）。
2. 每个格式计算 `score = 命中行数 / 样本行数 × 100 - 惩罚项`（惩罚：块格式命中半截块、命中率 > 95% 但关键字段全空等）。
3. 取最高分且 `≥ 60` 者为该文件格式；全部低于阈值则用 `generic`。
4. 多文件合并要求各源探测结果的 `id` 相同，否则拒绝（REQ-FILE-04）。
5. 支持手动指定（`File ▸ Open` 对话框 "Log format: Auto / <列表>" 或命令行 `--format <id>`）作为逃生舱；v1 不做自定义正则编辑器。

**列生成规则**：**`Line` 恒为第一列**（左起对齐，便于与原始文件对照）；多文件时 `File` 次之；`Time, Level` 紧随其后（格式无时间/级别则隐藏该列）；`Thread, Target, Pid, Host` 按"至少 50% 条目非空"判定是否显示；`Message` 常显且为最后一列（吃掉剩余宽度）；`extra` 列追加在 Message 之前。

**`Line` 取值**（REQ-PARSE-03）：默认为条目的首个物理行号；格式可声明 `ILogFormat::numbersEntriesSequentially()` 改用**源内条目序号 1..N**——记录式导出需要它：`wevt_xml` 整个文档常写在一个物理行上（所有事件同在第 2 行）、`wevt_tsv` 首行是表头（数据从第 2 行开始），物理行既无法区分行、也不能从 1 开始；编号由 `LogSource` 统一赋值（行式条目在 `parseEntryAt()`，文档式条目的在 `buildEntryIndex()` 重编号）。

**列宽自适应**（`LogTableView::autoFitColumns()`，REQ-TABLE-05，右键菜单 Auto-fit Columns）：宽度 = **内容宽度 + 2 px 取整余量**（`contentWidthHint()` → 逐单元格调用 `LogItemDelegate::sizeHint()`；表头标题更宽时以 `headerTitleWidth()` 为准，避免标题被截断），下限 36 px、上限 600 px——**不为凑最小宽度留白**。内容宽度采样**整篇文档**：前 `kAutoFitHeadRows`(500) 行连续 + 其后 `kAutoFitSpreadRows`(3500) 行均匀抽样；`sizeHintForColumn()` 被覆盖为该实现（Qt 自带只看向前部窗口，曾漏掉更宽的级别值）。分配优先级：① **时间列与级别列先预留**（时间戳不换行、级别 chip 不被裁切，两列都不参与缩放）→ ② 消息列（= `stretchColumn()`，消息列隐藏时为最后一个可见列）保留剩余宽度且不少于 `max(240, 视口宽度 × 2/5)` → ③ 其余可见列在 `预算 = 视口宽度 − 预留列 − 消息列份额` 内按内容长度等比分配：预算充裕时各取内容宽度，不足时按 `预算 × 内容ᵢ / Σ内容` 缩放（下限 24 px，末列吃余数）。回归提醒：紧空间分支必须跳过预留列（曾把时间戳二次缩小）；级别列也必须预留并全文档测量（曾因采样漏掉 DEBUG 而按 INFO 拟合）。

**续行合并**：解析返回后，若下一物理行不匹配任何锚点且 `general.continuationMerge == true`，则并入 `message`（`\n` 连接）并计入 `physicalLineCount`；块格式（`wevt_text`）自行消费整块行。

### 4.2 时间解析

| 输入形态 | 处理 |
| --- | --- |
| `2026-09-28T22:15:43.3035871+08:00` | `Qt::ISODateWithMs`，失败则自定义解析（7 位小数截断到 3 位 + 偏移解析） |
| `2026-09-28 22:15:43,303` | `,` → `.`，本地时区 |
| `Mmm dd HH:MM:SS`（syslog） | 年份取文件修改时间所在年；若推断日期比 mtime 晚 24h 以上，年份减 1（跨年轮转） |
| 仅 `HH:MM:SS` | 日期取自文件名中的 `YYYY-MM-DD`，否则取文件 mtime 日期，标记 `dateInferred = true` |
| 无时间戳 | `time` 无效；默认不参与时间过滤（REQ-FILTER-08）；详情面板显示 `—` |

> Linux 注意：syslog 月份缩写按英文（`Jan`…`Dec`）解析，不使用系统 locale 的月份名，保证跨语言环境一致。

### 4.3 编码检测与回退（平台化）

优先级（REQ-PARSE-04）：

1. BOM 检测：UTF-8 / UTF-16 LE/BE / UTF-32 LE/BE → 对应解码器。
2. 无 BOM：`QStringDecoder` 严格模式对前 256 KB 做 UTF-8 全量校验，通过即 UTF-8（快路径：直接按字节切行，交付 `QString` 时才解码）。
3. UTF-16 启发式：出现大量 `\x00` 交替字节则按 UTF-16LE 解码。
4. 平台回退（`EncodingBackend`，唯一允许平台宏的位置）：

```cpp
class EncodingBackend {                       // src/platform/EncodingBackend.h
public:
    struct Result { QString text; int invalidBytes = 0; };
    // 将一行原始字节按平台回退编码解码为 UTF-8 文本
    virtual Result decodeFallback(const QByteArray &raw) const = 0;
    virtual QString fallbackEncodingName() const = 0;
};
// Windows 实现：MultiByteToWideChar(CP_ACP 或显式 CP936)（无 ICU）
// Linux  实现：iconv("GB18030" → "UTF-8"，失败依次尝试 GBK/ISO-8859-1)
```

5. **内置兜底解码器（跨平台）**：随程序附带一份紧凑的 GB18030/GBK 解码表（由公开映射数据生成，约数十 KB），当平台后端不可用（musl / 无 iconv / 平台 API 失败）或平台后端返回非法序列时启用，保证 Windows / glibc / musl 行为一致。
6. **候选编码判定**：严格 UTF-8 校验失败后，依次用 GB18030 → Big5 → Shift_JIS → CP1252 做*有效性校验*，取第一个"全程可解码且无非法字节"的候选；全部失败时使用平台后端并把非法字节记为 U+FFFD。
7. 解码失败字节 → U+FFFD 并累计 `invalidBytes`，状态栏提示（REQ-PARSE-05）。

非 UTF-8 文件使用"逐行解码"路径（索引仍指向原始字节偏移），不生成临时转码文件，因此监控追加天然可用；检测结果只影响显示层，不写回磁盘。

> 备选方案评估（OPEN-09）：ICU（体积与部署成本高 → 否决）；Qt5Compat `QTextCodec`（需额外模块，且无 ICU 构建时能力受限 → 否决）；**内置映射表（体积小、行为确定、许可友好 → 采纳）**。glibc 的 `iconv` 仍作为首选后端以获得更好的覆盖与非 UTF-8 编码的处理一致性。

### 4.4 行索引与按需读取（LogSource）

- **建索引**：`QFile` 顺序读 + 64 KB 块，扫描 `\n`，记录块内行首的绝对字节偏移（`std::vector<qint64>`，8 B/行）；`\r\n` 与 `\n` 统一处理（`\r` 在解码阶段剥离）。1M 行 ≈ 8 MB。
- **行数上限**：`general.maxLinesPerFile`（默认 2,000,000）。达到上限时暂停索引并询问；Continue → 继续建索引；Cancel → 以已索引部分作为文档并提示（REQ-PARSE-09）。
- **读取**：`readEntry(row)` → 偏移定位 → 按需解码 → 当前格式 `parse()` → LRU 缓存 `ParsedEntry`（每源 100,000 条）。
- **快速预判位图**：索引阶段记录每行首字符类别（数字/`[`/`<`/`{`，1 bit/行），用于时间过滤与片段检测早期剪枝。
- **过滤结果缓存**：`FilterProxyModel` 收到 `FilterSpec` 后在工作线程生成"可见行位图 + 前缀和"，主线程按行号 O(1) 映射，避免 `QSortFilterProxyModel` 逐行回调开销。

### 4.5 LogDocument（多文件合并）

```cpp
struct SourceRef { int sourceId; qint64 line; };
class LogDocument {
    QVector<QSharedPointer<LogSource>> sources;   // 同格式
    ColumnSchema schema;                          // 合并后列结构
    std::vector<SourceRef> order;                  // 合并行序（时间交错或拼接）
    bool timeSorted;                              // 合并模式标记
public:
    static OpenResult open(const QStringList &paths, ...);  // 校验格式一致性
};
```

- 时间交错：各源按 `(time, sourceId, line)` 做 k 路归并；`time` 无效的源整体置于末尾，`timeSorted=false`，状态栏提示。
- 合并后附加 `File` 列（同名文件追加父目录名区分）。
- 排序稳定：相同时间戳按源顺序与物理行序，保证刷新/监控追加后不跳动。

### 4.6 表格模型与过滤

- `LogTableModel`（`QAbstractTableModel`）：`rowCount` = 可见行数，`data()` 仅做轻量转换；`headerData` 返回翻译后列名；`Qt::UserRole + FullTextRole` 返回**未截断**完整文本（供双击复制与详情面板，REQ-CLIP-01）。
- `FilterSpec = { levelMask, timeRangeOpt, filterMatcher }`，各条件 AND 组合。
- `Matcher`：
  - 全词：`(?<![A-Za-z0-9_])PATTERN(?![A-Za-z0-9_])`（仅当模式首/尾为单词字符时加对应边界），CJK 场景见 REQ-FIND-08；
  - 通配符：`QRegularExpression::wildcardToRegularExpression(p, UnanchoredWildcardConversion)`；
  - 正则：直接编译；模式长度上限 512（OPEN-06）；
  - 大小写：`CaseInsensitiveOption` 开关；
  - 匹配目标：所有可见列的 `QString`，短路返回首个命中列（供导航定位）。
- 过滤/查找在 worker 线程分批执行（每批 50,000 行检查取消标志），完成后以位图回主线程刷新模型。

### 4.7 剪贴板与单元格交互

- 双击：`LogTableView::doubleClicked(QModelIndex)` → `index.data(Qt::UserRole + FullTextRole)`（**完整未截断文本**，非 delegate 渲染字符串）→ `QGuiApplication::clipboard()->setText()` → 状态栏 `Copied %1 chars from "%2"`（REQ-CLIP-01/02）。
- `Ctrl+C`：复制选中区域，单元格制表符分隔、行换行分隔（REQ-CLIP-03）。
- 右键菜单（最小集，OPEN-01）：Copy Cell / Copy Row / Copy Message / Auto-fit Columns。
- 行内展开：视图维护 `QSet<EntryKey>`（`EntryKey = {源序号, 源内条目序号}`，行式与文档式条目均唯一，REQ-TABLE-06），`setRowHeight(row, contentHeight)`；`Enter`/`Space` 切换；过滤/排序变化后按 `EntryKey` 重新应用；展开高度上限 30 行防超长内容。

### 4.8 详情面板（DetailPane）与行高模式

行高与详情框由 Settings ▸ Details Pane ▸ **Show Details Pane** 控制（开关式，无第三态）：

| 勾选状态 | 详情面板 | 行高 |
| --- | --- | --- |
| ☑ 显示 | 打开日志时自动选中第一条并展开；点击行更新内容；面板不自动隐藏；Details Pane ▸ Layout 位置可选 | 两行 + 内边距；Enter/Space 可临时展开整行（上限 30 行） |
| ☐ 隐藏 | 始终不显示，点击行也不展开；Layout 子菜单置灰 | **完整内容模式**：行高按内容自适应、不截断（单条上限 200 行）——此时表格是唯一的阅读位置（REQ-TABLE-04 例外） |

> 完整内容模式由 `LogTableView::setFullContentMode()` / `LogItemDelegate::kFullContentLineLimit` 实现，并绑定到"详情框关闭"这一状态（`MainWindow::applyDetailsBehavior()`）。行高策略：文档 ≤ 50,000 行时一次性计算全部行高（等待光标 + 暂停重绘），超过则抽样估计默认行高、滚动时逐行精确化。
查找与过滤引擎（M3 已实现）：

| 组件 | 职责 |
| --- | --- |
| `core/Matcher` | 编译全词 / 通配符 / 正则三类模式 + 大小写开关；全词边界使用 [A-Za-z0-9_]（CJK 不视为单词字符，中文仍可匹配，REQ-FIND-08）；模式长度上限 512（OPEN-06）；`ranges()` 返回全部命中区间用于高亮 |
| `core/FilterSpec` | 级别掩码 + 时间范围（按绝对时刻比较；无有效时间戳的条目默认不匹配，REQ-FILTER-08）+ 关键词匹配器（`invertKeyword` 反向：只保留不匹配的行，REQ-FILTER-09），条件之间为 AND |
| `LogTableModel`（过滤） | 计算可见行映射 `m_visibleRows`，视图只看到过滤后的行；`sourceRow()` / `visibleRowOf()` 双向映射供详情面板、复制、展开使用；监控追加时只把通过过滤的新行并入映射；当所有实际出现的级别都被勾选时掩码归零，退回全速路径 |
| `LogTableModel`（列显隐） | 隐藏列的**唯一事实源**：`m_hiddenColumns` 保存列键（`columnKey()` = `columnKindKey()` 或 `extra:<键>`，与译文无关，REQ-TABLE-11）。视图经 `columnsVisibilityChanged` 信号把状态映射到表头 section；关键词过滤与 `ensureFindRows()` 跳过隐藏列；导出与（Ctrl+E）只写可见列；`setColumnHidden()` 拒绝隐藏 Line 列（REQ-PARSE-03） |
| `LogTableModel`（查找） | `setFindMatcher()` + `findRanges(row, column)`（按单元格缓存）+ `findMatchRowCount()` / `findRowAt()` / `findOrdinalOf()`，支持 F3 / Shift+F3 环形导航与 `3/128` 计数；查找只高亮、不隐藏行（REQ-FIND-01） |
| 级别过滤行 | `FilterPanel::rebuildLevelChecks()` 按文档的级别直方图动态创建/销毁复选框（只列计数 > 0 的级别，**不显示计数**，计数仅出现在悬停提示中）；取消勾选的级别记录在 `m_uncheckedLevels` 中并跨文档保留；实时监控时若可见级别集合不变只更新提示文本，出现新级别才重建（避免每 300 ms 抖动控件） |
| 关键词高亮 | 默认绿底黑字（Settings ▸ Appearance ▸ Highlight Color 可改），绘制在片段 token 之上（REQ-HL-07） |
| `highlight/SnippetTokenizer` | 检测并 token 化 JSON（括号配平、字符串转义、嵌套）、XML（标签配平、属性、注释、忽略 `<`/`>` 比较运算符）、YAML（≥ 2 行的键值/列表块）；最小长度 8、单条消息最多 4 段、单段 ≤ 2000 字符（REQ-HL-04） |
| `highlight/HighlightTheme` | VSCode Dark+ / Light+ token 配色常量表；默认跟随应用主题，可由 Settings ▸ Appearance ▸ Syntax Highlighting 强制指定（REQ-HL-03） |
| `highlight/MessageTextHighlighter` | 详情面板的 `QSyntaxHighlighter`：对整段消息 token 化后按文本块映射偏移，支持跨行的 YAML/JSON 片段（REQ-HL-06） |
| 导出 | File ▸ Export Filtered Results（Ctrl+E）：按当前可见行导出 CSV（含表头、RFC4180 引号转义）或制表符分隔文本，UTF-8 |
| 面板折叠与全屏 | `FilterPanel::setCollapsed()` 只切换输入行容器的可见性——已生效条件与输入内容不受影响（REQ-UI-13）；标题栏右上角的 `QToolButton` 由 `resizeEvent` 定位（`resizeEvent` + `move()`，随窗口尺寸变化保持在标题行内）。`MainWindow::applyFullScreenState()` 响应 `QEvent::WindowStateChange`：进入全屏时记录并折叠面板，退出时恢复（REQ-UI-14）；`Esc` 由仅在全屏期间启用的 `QShortcut` 处理，F11 勾选项与窗口状态双向同步（含窗口系统发起的全屏变化）。全屏切换使用 `showFullScreen()` / `showMaximized()` / `showNormal()` 而非 `setWindowState()`——后者在"最大化 → 全屏"时保留 `WindowMaximized` 标志，Windows 下窗口会向右偏移约 48 逻辑像素，右边缘的折叠按钮被裁到屏幕外；关闭窗口时若仍处于全屏，保存的是进入全屏前记录的几何（REQ-UI-14：全屏不持久化） |

打开耗时（REQ-UI-15）：`MainWindow::openPaths()` 用 `QElapsedTimer` 测量打开操作，并扣除「大文件是否继续加载」对话框的等待时间，然后交给 `core/DurationFormat::formatDuration()` 生成自适应文本，写入状态栏左下角的**普通指示标签**（`statusBar()->addWidget()`，临时消息显示期间暂时让位）。计时覆盖同一调用栈内的全部同步工作：解析/建索引、模型挂载，以及**完整内容模式（详情框关闭）下的一次性行高计算**——因此数值会随该模式显著变化（大文档尤为明显；实测约 1.8 万行样本：详情框开启 0.69 s，关闭时因整篇行高 pass 为 16 s）。Refresh 重新打开后更新；关闭文档与 `--demo` 演示数据清除；`retranslateUi()` 在语言切换时按保存的毫秒数重新生成文本（`%1 s` / `%1 min %2 s` / `%1 h %2 min %3 s` 等分量为 0 时省略高位分量）。

监控实现（`LogWatcher`，对应 REQ-MON）：

| 项 | 实现 |
| --- | --- |
| 触发机制 | 300 ms 定时轮询为主 + `QFileSystemWatcher`（文件与所在目录）即时事件；轮询保证在 inotify/ReadDirectoryChangesW 受限时仍可用 |
| 增量索引 | `LineIndex::refresh()` 只从上次结束偏移继续扫描，追加行；本次未写完的"半行"会被丢弃并在下一次刷新时重读（写完换行后才成行） |
| 轮转/截断 | 文件尺寸小于已索引偏移时判定为轮转/截断 → 重建索引并发 `documentRebuilt`，主窗口重置模型、刷新级别计数、按需重新跟随 |
| 句柄策略 | `LogSource` **不长期持有文件句柄**（每次读取即开即关），否则 Windows 上会阻止写方轮转/改名日志文件 |
| 智能跟随 | 追加前若视图在底部 → 追加后自动滚到底；否则暂停跟随、右下角浮出「新行：N ▼」按钮并计数，点击或手动滚到底后复位 |
| 计数同步 | 追加行会增量扩展级别直方图；过滤面板只刷新数字，不动用户的级别勾选 |
| 范围限制 | 仅文件型文档（`--demo` 不可用），行数上限沿用 `general.maxLinesPerFile` |

详情框内容区（`DetailPane`）：
- 字段表：`QFormLayout`，字段名加粗、值可选中等宽显示，值标签启用换行；
- 消息区：`QPlainTextEdit`（只读、等宽字体、`WidgetWidth` 折行 + `WrapAtWordBoundaryOrAnywhere`、`ScrollBarAlwaysOff`），**永远不会出现横向滚动条**，超长行按面板宽度折行后纵向滚动；
- 底部按钮：Copy Message / Copy All Fields。
多段消息的绘制（回归缺陷修复）：`LogItemDelegate::drawClampedText()` 按 `\n` 分段排版，段落原点由累加的 `y` 表示，但段落内行位置曾误用**全局行号**（`(drawn + i) * lineSpacing`）——于是每段文字被下移 "已绘制行数 × 行距"，多段消息只有第一段可见、其余被行边界裁掉。现改为**段内相对索引**（`lines.size() * lineSpacing`）。该缺陷在完整内容模式（行高够大）下才会被看见，因此在"不显示详情框 → 完整展示"时才暴露。

表格截断行的着色（回归缺陷修复）：`LogItemDelegate::drawClampedText()` 在需要省略末行时会重绘该行；早期版本用不带格式的 `drawText` 重绘，导致**被截断行的高亮全部丢失**。详情框打开会使消息列变窄、彩色内容大量落入截断行，因此表现为"开启详情框后表格高亮失效"。现在通过 `shiftFormatsToElidedLine()` 把段内格式区间映射到省略后的文本再绘制（`tst_mainwindow_behavior::snippetColorsSurviveTruncation` 覆盖）。同一处理也覆盖完整内容模式与行内展开的行。

打开时机（回归缺陷修复）：详情框只由两处逻辑打开——`LogTableView::rowActivatedByUser`（鼠标点击或键盘导航选中某行）与 `MainWindow::selectFirstRowIfRequested()`（Show at Startup 勾选时）。`QItemSelectionModel::currentRowChanged` 仅用于**更新已打开面板的内容**，不再触发打开动作，因此模型重置 / 刷新 / 重新打开文件 / 过滤引起的程序化选中不会弹出详情框。行为由 `tests/tst_mainwindow_behavior.cpp` 锁定（6 个用例）。

行键与缓存（回归缺陷修复）：行高缓存与行内展开状态都以**行键**为索引，键 = `(源序号 << 40) | 源内行号`（`LogTableView::rowKey()`）。早期实现用「起始物理行」作键，对 XML 类**文档格式**（一行内含多个事件）必然冲突——实测 `test-event-logs.xml` 的 3620 个事件全在同一物理行，导致所有行共用同一高度（513 行被截断）。过滤/排序后由 `LogTableModel::sourceRow()` 映射回源内行号，键保持稳定。

行高计算策略（`LogTableView`）：
- 高度按**所有可见列**中最高的单元格计算（`contentHeightForRow()`，回归缺陷修复：原先只按消息列，长目标 + 短消息的行会把目标文本画到分隔线上）。为控制开销，先算消息列（通常是最高单元格）作基准，其余列用 `lineCountForCell(index, width, lines + 1)` **探测**"是否超过基准"：探测在基准 +1 行处截断布局，只有真正超过基准的列才完整计算；单段且宽度足够的短列（行号/时间/级别/线程）走宽度快速路径、不做文本布局。实测 4664 行文档的完整内容模式行高 pass 开销增加约 30%。结果按 `(行键, 消息列宽)` 缓存；行键 = `(源序号, 源内行号)`，因此刷新/过滤后仍然稳定。
- 行高依赖所有列宽：任意列的宽度变化都会清空行高缓存，并合并（延迟一次）为一次重算——大文档（> 5000 行）拖动列宽时只更新可见行，其余行在滚动时逐行精确化（与既有策略一致）。
- 完整内容模式下：文档 ≤ 50,000 行时一次性计算全部行高（等待光标 + 暂停重绘）；超过上限时用抽样（最多 1000 行）估计默认行高，滚动时逐行精确化（避免 100 万行 × 文本排版的卡顿）。
- 表头拉伸列宽在首次布局时尚未生效，因此 `scheduleHeightRefresh()` 会在事件循环空闲时校验"上次行高计算所用的**列宽快照**"（`columnWidthSnapshot()`，所有可见列一条记录），不一致则重算（自愈，避免用过窄列宽算出的超高行）。
- 单次扫描在开始时固定一个列宽快照（`passWidths`）并贯穿整轮计算，结束时把该快照记为"本轮所用宽度"；扫描进行中到达的列宽变化会使快照比对发现差异并触发一次完整重算，因此不会残留旧列宽下的高度。

- 首次点击行时创建/显示（REQ-DETAIL-01）；`QSplitter` 承载：右侧布局 = 水平分割（左 Log / 右 Details），底部布局 = 垂直分割。
- 结构：`QFormLayout` 字段表（字段名 `QFont::Bold`）+ `QPlainTextEdit`（只读、等宽、`NoWrap` 可切、`QSyntaxHighlighter` 高亮）+ 复制按钮行。
- 文档对象常驻，`setEntry(const ParsedEntry&, const HighlightTheme&)` 更新；长内容由编辑器自身滚动（REQ-DETAIL-04）。

### 4.9 平台抽象层与平台差异

`src/platform/` 是**唯一**允许出现 `#if defined(Q_OS_WIN) / Q_OS_LINUX` 的目录（REQ-PLAT-01）。

| 差异点 | Windows | Linux | 承载位置 |
| --- | --- | --- | --- |
| 本地编码回退 | `MultiByteToWideChar`（CP_ACP，可显式 CP936） | `iconv`（GB18030 → UTF-8，回退 GBK/ISO-8859-1） | `EncodingBackend` |
| 配置目录 | `%APPDATA%\LogViewer\` | `$XDG_CONFIG_HOME/LogViewer/`（默认 `~/.config/LogViewer`） | `QStandardPaths::AppConfigLocation`（`PlatformInfo` 封装） |
| 默认等宽字体 | `Consolas` → `Cascadia Mono` → 系统等宽 | `QFontDatabase::systemFont(FixedFont)`（DejaVu Sans Mono / Noto Sans Mono） | `PlatformInfo::monospaceFont()` |
| 界面字体 | 系统 UI 字体（Segoe UI） | 系统 UI 字体（Noto Sans / Cantarell 等） | `QFontDatabase::systemFont(GeneralFont)` |
| 文件监视 | `QFileSystemWatcher`（ReadDirectoryChangesW）+ 轮询兜底 | `QFileSystemWatcher`（inotify）+ 轮询兜底（watch 数受限时仍可用） | `LogWatcher`（无平台宏，仅配置差异） |
| 控制台输出 | GUI 子系统需 `AttachConsole` 才能打印 `--help/--version` | 直接写 stdout/stderr | `ConsoleAttach` |
| 文件关联 | PowerShell 脚本写 `HKCU\Software\Classes`（可撤销） | `.desktop` + MIME XML + `xdg-mime`（随 install/.deb） | `FileAssociation` + `packaging/` |
| 打包 | `windeployqt` → `dist\` 便携目录 | CPack DEB + `install` 规则 | `scripts/`、`packaging/` |
| 行尾 | 导出默认 CRLF | 导出默认 LF | 导出模块按 `QOperatingSystemVersion` 或提供开关（可强制 LF） |
| 路径分隔符 | `\` | `/` | 一律 `QDir`/`QFileInfo`，禁止字面量（CON-9） |
| 文件名大小写 | 不敏感 | 敏感 | 格式探测与扩展名映射一律大小写不敏感 |
| DPI / 显示后端 | 系统缩放 | X11 / Wayland 自适应 | Qt 6 自动处理（不写平台代码） |

> 本节实施对应需求：REQ-PLAT-01…10（可移植性）、REQ-CLI-01…08（命令行）、REQ-ASSOC-01…05（文件关联）、CON-9…CON-12（约束）。

## 5. 高亮引擎

### 5.1 输出模型

`MessageHighlighter::spans(const ParsedEntry&, const HighlightContext&) -> QVector<Span>`，`Span = {int start; int length; TokenKind kind;}`（偏移针对 Message 文本）。渲染时按 span 依次绘制 `QTextLayout::FormatRange`。

TokenKind：`Plain, Timestamp, Level{PerLevel}, Keyword, Key, String, Number, KeywordToken, Comment, Type, Function, Variable, Punctuation, TagName, AttributeName, AttributeValue`。

### 5.2 级别与时间戳着色

| 级别 | Light（文字 / 底色 20%） | Dark（文字 / 底色 20%） |
| --- | --- | --- |
| TRACE | `#6B7280` / `#F1F3F4` | `#808080` / `#2D2D30` |
| DEBUG | `#3F6212` / `#F0F5E5` | `#6A9955` / `#26331F` |
| INFO | `#1565C0` / `#E8F1FB` | `#3794FF` / `#13273D` |
| NOTICE | `#00695C` / `#E0F2F1` | `#4EC9B0` / `#0F2A26` |
| WARN | `#B26A00` / `#FFF4E5` | `#CCA700` / `#332C14` |
| ERROR | `#C62828` / `#FDECEA` | `#F14C4C` / `#3A1D1D` |
| FATAL | `#8E0000` / `#F9DEDE` | `#FF6B6B` / `#40201F` |
| OTHER | `#455A64` / `#ECEFF1` | `#9E9E9E` / `#2D2D30` |

时间戳使用次要文字色（Light `#6A737D` / Dark `#808080`）；级别单元格以"色块 + 文字"呈现（底色 20% 透明度）。

### 5.3 关键词高亮

- 默认 **底 `#7CFC00` / 字 `#000000`**（REQ-FIND-04），Settings 可改；Find 与 Filter 命中可用不同色调以区分（默认同色）。
- 层级：关键词底色优先级最高，覆盖片段 token 前景色（REQ-HL-07）。

### 5.4 片段检测与 token 化

| 类型 | 检测 | Token 化 |
| --- | --- | --- |
| JSON | 从 `{` 或 `[` 起做括号配对（跳过字符串内括号与 `\"` 转义） | 键→Variable、字符串→String、数字→Number、`true/false/null`→KeywordToken、标点→Punctuation |
| XML | 从 `<Tag` 起配平开闭标签（容忍自闭合、属性、注释、CDATA；大小写敏感） | 标签名→TagName、属性名→AttributeName、属性值→AttributeValue、`<>/=`→Punctuation、注释→Comment |
| YAML | 连续 `key: value` / `- item` / `#comment` 行，至少 2 行或含嵌套缩进 | 键→Variable、标量→String/Number/KeywordToken、注释→Comment、`-`/`:`→Punctuation、锚点/tag→Type |

约束（REQ-HL-04）：片段长度 ≥ 8 字符；必须完整闭合；单条消息最多 4 个片段；单片段 token 化上限 2000 字符；结果按（消息哈希 + 主题 + 关键词版本）缓存。

### 5.5 VSCode token 配色表（`resources/themes/vscode_tokens.json`）

| Token | Dark+ | Light+ |
| --- | --- | --- |
| 正文 Plain | `#D4D4D4` | `#000000` |
| 字符串 String | `#CE9178` | `#A31515` |
| 数字 Number | `#B5CEA8` | `#098658` |
| 关键字/常量 Keyword | `#569CD6` | `#0000FF` |
| 控制关键字 | `#C586C0` | `#AF00DB` |
| 类型 Type | `#4EC9B0` | `#267F99` |
| 函数 Function | `#DCDCAA` | `#795E26` |
| 变量/键 Variable | `#9CDCFE` | `#001080` |
| 注释 Comment | `#6A9955` | `#008000` |
| XML 标签 TagName | `#569CD6` | `#800000` |
| XML 属性名 AttributeName | `#9CDCFE` | `#E50000` |
| XML 属性值 AttributeValue | `#CE9178` | `#0000FF` |
| 标点 Punctuation | `#808080` | `#383838` |
| 代码块底色 | `#1E1E1E` | `#FFFFFF` |

> 配色集中存放于 JSON 资源，便于微调。详情面板中片段以"代码块底色 + 内边距"呈现；表格单元格内仅用前景 token 色（避免浅色界面出现突兀暗块）。

### 5.6 渲染路径

- **表格**：`LogItemDelegate::paint()` 只处理可见单元格（以 `option.rect` 作 `IntersectClip`，单元格内容**永不越界**——段落自身的裁剪也用 `IntersectClip`，早期用默认的 `ReplaceClip` 会替换掉单元格裁剪，行高不足时文本会画到分隔线之外）；`QTextLayout`（两行换行 + 省略号裁剪，`WrapAtWordBoundaryOrAnywhere`）绘制，`FormatRange` 来自 `MessageHighlighter` 缓存；`sizeHint()` 的高度取 `defaultRowHeight()`（行高由视图统一管理），**宽度按单元格完整文本计算**（+左右内边距，级别列再加 chip 内边距并用粗体，上限 2000 px、长行只测前 512 字符）——`resizeColumnToContents()`（右键 Auto-fit Columns 与双击列边界）依赖该宽度，回显当前宽度会让所有列被压成同一最小值；行高缓存 key = (行键, 消息列宽)，字体/任意列宽变化或列显隐切换都会清空缓存（见 §4.8 行高策略）。单元格背景由 delegate 自绘，因此**斑马纹**（REQ-TABLE-09）由 `index.row() % 2` 选取 `Base` / `AlternateBase`（视图的 `alternatingRowColors` 被整格填充覆盖、不可能透出；按显示行序交替，过滤后仍逐行交错），选中行改用 `Highlight`；**单元格分隔线**（REQ-TABLE-10）在每格右缘与下缘各画一条 1px 线（深色 `#3A3A3E` / 浅色 `#D9D9DD`，与 `LogHeaderView` 的列分隔线同色，两处需同步修改）。
- **详情面板**：`MessageTextHighlighter : QSyntaxHighlighter` 处理整段文本（支持跨行片段），关键词高亮用 `QTextCharFormat` 覆盖。

## 6. 界面布局与菜单结构

### 6.1 主窗口线框图（1920×1080 设计基准；内容列随窗口宽度伸展，最大化时铺满）

```
┌────────────────────────────────────────────────────────────────────────────────────────────┐
│ File   Settings                                                              Log Viewer 1.1 │ ← 菜单栏（左起排列）
├────────────────────────────────────────────────────────────────────────────────────────────┤
│      ┌──────────────────────────────────────────────────────────────────────────────┐      │
│      │ ▾ Search & Filter                                                            │      │
│      │  Find   [___________________________] [Whole word ▾] [☐ Aa]   ◀ 3/128 ▶      │      │
│      │  Filter [___________________________] [Whole word ▾] [☐ Aa]  [Apply] [Clear] │      │
│      │  Levels [TRACE 0][DEBUG 9121][INFO 23][NOTICE 0][WARN 0][ERROR 43][OTHER 0]   │      │
│      │  Time   [☐] From [2026-09-28 22:15:43 ▾]  To [2026-09-28 23:54:56 ▾]        │      │
│      │         Presets: [Last 5 min] [Last 15 min] [Last 1 hour] [Today] [All]      │      │
│      └──────────────────────────────────────────────────────────────────────────────┘      │
│      ┌──────────────────────── Log ─────────────────────┐  ┌──── Details ─────────────┐   │
│      │ ┌───────────┬───────┬──────────────────────────┐ │  │ Time   2026-09-28T22:... │   │ ← 详情为右侧布局
│      │ │ Time      │ Level │ Message                  │ │  │ Level  ERROR             │   │
│      │ ├───────────┼───────┼──────────────────────────┤ │  │ Target shadowsocks_...   │   │
│      │ │ 22:15:43  │ INFO  │ shadowsocks local 1.25.0 │ │  │ ──────────────────────── │   │
│      │ │ 22:15:43  │ DEBUG │ started plugin "C:/...    │ │  │ Message:                 │   │
│      │ │ 22:16:17  │ ERROR │ copy bidirection ends... │ │  │ HTTP connection 127.0.0. │   │
│      │ └───────────┴───────┴──────────────────────────┘ │  │ 1:51788 handler failed…  │   │
│      └──────────────────────────────────────────────────┘  └──────────────────────────┘   │
├────────────────────────────────────────────────────────────────────────────────────────────┤
│ sslocal.2026-09-28.log · UTF-8 · tracing · 9187 lines (showing 43) · Monitor: off         │
└────────────────────────────────────────────────────────────────────────────────────────────┘
```

底部布局时：`Details` 分组框移至 `Log` 分组框下方（垂直分割），两者仍保持同宽对齐。

折叠态（REQ-UI-13）：分组框只保留标题一行，标题栏右侧的切换按钮由 `▾` 变为 `▸`，其下内容整体上移，`Log` 分组框获得释放的纵向空间；`Settings ▸ Full Screen`（REQ-UI-14）进入全屏时该分组框自动折叠。折叠只隐藏输入行——已生效的 Find / Filter / 级别 / 时间条件继续生效。

状态栏（REQ-UI-15）：**左下角**显示最近一次打开日志的耗时（普通指示区，例如 `Loaded in 0.35 s`；临时消息如复制提示显示期间暂时让位），右侧依次为文件名、格式/编码/行数、Monitor 状态（永久指示区）。

### 6.2 尺寸与对齐规范

| 元素 | 规范 |
| --- | --- |
| 内容列 | **随窗口宽度伸展，无最大宽度限制**；左右外边距 16 px 对称；所有分组框左右边界对齐（同宽）；最大化时铺满可用像素（REQ-VIS-02 已更新） |
| 窗口尺寸 | 默认 1600×950；若可用屏幕不足则取 `min(默认, 可用 × 0.92)`；最小 `min(1280×720, 可用)`；高 DPI 下按逻辑像素计算（REQ-PLAT-06） |
| 分组框间距 | 垂直 12 px；内部边距 8 px、控件间距 8 px（表单行 6 px） |
| Search & Filter 分组框 | 展开时固定高度（约 150 px）、不参与纵向拉伸；可折叠为标题单行（切换按钮位于标题栏右侧，REQ-UI-13） |
| Log 分组框 | 伸缩因子 1；右侧布局时与 Details 水平分割比默认 62:38，底部布局时 68:32 |
| Details 分组框 | 初始隐藏；出现后最小 320×180 px |
| 表格表头 | 高度 = 字体行高 × 1.6；加粗；渐变背景（Light `#FAFAFA→#E4E4E4`，Dark `#3C3C3C→#2D2D2D`）+ 底边 1 px `#C8C8C8`/`#1F1F1F`；列内文字左对齐、垂直居中 |
| 表格单元格 | 行高 = 两行文本高度 + 6 px 内边距；左对齐、垂直居中；超长以 `…` 收尾；列右边界 1 px 网格线（`#E8E8E8`/`#2A2A2A`） |
| 详情字段名 | 加粗；字段与值两列；值列可选中 |
| 状态栏 | 单行；**左下角**为最近一次打开日志的耗时标签（`Loaded in …`，REQ-UI-15；普通指示区，临时提示显示时暂时让位）；右侧为永久指示区：文件名 / 格式 / 编码 / 行数（`·` 分隔）、Monitor 状态（生效时显示绿色圆点） |

### 6.3 菜单结构

```
File
 ├─ Open…                        Ctrl+O
 ├─ Refresh                      F5
 ├─ Close                        Ctrl+W
 ├─ Monitor                      ☑（仅单文件可勾选）
 ├─ ────────────
 ├─ Export Filtered Results…     Ctrl+E（CSV / Plain text）
 ├─ ────────────
 ├─ Recent Files ▸               （最多 10 项 + Clear List）
 ├─ ────────────
 └─ Exit                         Ctrl+Q

Settings
 ├─ Font ▸
 │   ├─ Interface Font…
 │   ├─ Log Table Font…
 │   ├─ Log Header Font…
 │   ├─ ─────────
 │   └─ Reset to Defaults
 ├─ Language ▸
 │   ├─ English      (•)
 │   └─ 简体中文
 ├─ Details Pane ▸
 │   ├─ Show Details Pane (checkbox, default off)
 │   └─ Layout ▸                 （只影响详情框位置，关闭详情框时置灰）
 │       ├─ Details: Right   (•)
 │       └─ Details: Bottom
 ├─ Appearance ▸
 │   ├─ Theme ▸ Light (•) / Dark / Follow System
 │   ├─ Syntax Highlighting ▸ Follow Theme (•) / VSCode Dark+ / VSCode Light+
 │   ├─ Highlight Color…
 │   ├─ ─────────
 │   └─ Reset All Settings
 ├─ ────────────
 └─ Full Screen                   F11（勾选项；进入时自动折叠查询与过滤分组框，Esc 退出）

Columns              ← 菜单栏顶级项（位于 Settings 与 About 之间，REQ-UI-12）
 ├─ 行号                （常显，禁用项，悬停提示）
 ├─ 时间               ☑
 ├─ 级别               ☑
 ├─ …（按当前文档的列动态生成，含 extra 列；取消勾选 = 隐藏该列）
 ├─ ────────────
 └─ Show All Columns   （仅当存在隐藏列时可用）

About                ← 菜单栏顶级项（与 File / Settings 平级），点击直接打开关于对话框
```

## 7. 交互、快捷键与命令行接口

### 7.1 交互与快捷键

| 操作 | 行为 |
| --- | --- |
| 单击行 | 选中该行 → 首次点击时显示 Details 分组框并填充完整内容（REQ-DETAIL-01） |
| **双击任意单元格** | **复制该单元格完整原始文本到剪贴板**（REQ-CLIP-01/02） |
| **Enter / Space** | 切换当前行的整行高度展开（REQ-TABLE-06） |
| `Ctrl+C` | 复制选中单元格/行（制表符与换行分隔） |
| `F3` / `Shift+F3` | 下一条 / 上一条 Find 命中 |
| `Ctrl+F` / `Ctrl+G` | 聚焦 Find 框 / Filter 框 |
| `Enter`（Find / Filter 框内） | 提交模式并执行（Find 跳到首个命中；Filter 隐藏不匹配行） |
| 输入过程 | 不刷新表格：Find 的高亮与 Filter 的行过滤都只在提交后生效（清空输入框立即生效） |
| `Ctrl+O` / `F5` / `Ctrl+W` / `Ctrl+Q` | 打开 / 刷新 / 关闭文档 / 退出 |
| `Ctrl+M` | 切换 Monitor（tail -f，仅文件型文档可用） |
| `F11` / Settings ▸ Full Screen | 进入 / 退出全屏（REQ-UI-14）：进入时自动折叠查询与过滤分组框 |
| `Esc` | **全屏时退出全屏**并恢复进入前的折叠状态（REQ-UI-14；该快捷键仅在全屏期间启用）；非全屏时为设计保留的「优先取消行内展开；若无展开则清除 Find 高亮」 |
| 标题栏右侧 `▾` / `▸` | 折叠 / 展开查询与过滤分组框（REQ-UI-13）：只隐藏输入行，已生效条件继续生效 |
| `Ctrl` + 滚轮 | 表格字体临时缩放（10%～200%，会话内，不改设置） |
| 拖拽文件到窗口 | 按 REQ-FILE-02/04 处理（Windows 资源管理器 / Linux 文件管理器）：`MainWindow` 接受拖放，子控件（查找/过滤输入框、表格视图、详情消息区）关闭自身拖放以免吞掉事件；仅接受本地文件，文件夹忽略并提示 |

### 7.2 命令行接口（REQ-CLI）

```
log-viewer [options] [files...]

Options:
  -h, --help              显示帮助并退出（退出码 0）
  -v, --version           显示版本、Qt 版本、构建平台与编译器信息后退出
      --lang <en|zh_CN>   覆盖界面语言（不写入设置）
      --monitor           打开单个文件后立即启用 Monitor（多文件时忽略并提示）
      --format <id|auto>  强制日志格式（--format list 列出可用 id）
      --demo              载入内置演示数据（界面自检/截图用，非正式功能）
```

| 要求 | 实现要点 |
| --- | --- |
| 解析器无 UI 依赖 | `app/CliParser` 独立类，`QCommandLineParser` + 自研校验，可单元测试（`tst_cli`） |
| 退出码 | 正常 0；参数错误 2；文件不存在/不可读 1（输出到 stderr，不弹 GUI 对话框） |
| 控制台打印 | Windows（GUI 子系统）通过 `ConsoleAttach` 调用 `AttachConsole(ATTACH_PARENT_PROCESS)` 后写 stdout；无控制台时退化为不输出但仍返回正确退出码。Linux 直接写 stdout/stderr |
| 启动顺序 | 解析命令行 → 加载设置 → 应用 `--lang` 覆盖 → 创建主窗口 → 打开位置参数文件（`--monitor` / `--format` / `--demo` 生效） |
| 多实例 | **不做单实例限制**：允许同时运行多个实例（用户要求，便于并行查看不同日志）；不注册命名管道 / Unix socket，第二次启动即打开独立窗口 |

## 8. 设置项与持久化

存储（CON-11，通过 `QStandardPaths::AppConfigLocation`）：

| 平台 | 路径 |
| --- | --- |
| Windows | `%APPDATA%\LogViewer\settings.ini` |
| Linux | `$XDG_CONFIG_HOME/LogViewer/settings.ini`（默认 `~/.config/LogViewer/settings.ini`） |

| 键 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `font/interface` | QFont | 系统 UI 字体 | 菜单与界面字体 |
| `font/table` | QFont | 平台等宽字体（`PlatformInfo::monospaceFont()`）10 pt | 日志表格单元格字体 |
| `font/header` | QFont | 表格字体 + Bold | 表头字体 |
| `language` | string | `en` | `en` / `zh_CN` |
| `layout/detailsPosition` | string | `right` | `right` / `bottom` |
| `layout/splitterState` | QByteArray | 空 | 分隔条位置（按布局方向分别记忆） |
| `view/showDetailsPane` | bool | `false` | 详情框开关（REQ-UI-11）：true = 始终显示（打开日志选中第一条，点击行更新内容）；false = 始终不显示。旧键 `view/alwaysShowDetails` / `view/detailsOnStartup` / `view/detailsOnRowClick` 在首次启动时自动迁移并删除（SettingsStore::migrateLegacyKeys） |
| `appearance/theme` | string | `light` | `light` / `dark` / `system` |
| `appearance/syntaxTheme` | string | `follow` | `follow` / `dark+` / `light+` |
| `appearance/highlightBg` / `highlightFg` | QColor | `#7CFC00` / `#000000` | 关键词高亮颜色 |
| `find/mode` `find/caseSensitive` | int/bool | `wholeWord` / `false` | Find 框设置 |
| `filter/mode` `filter/caseSensitive` | int/bool | `wholeWord` / `false` | Filter 框设置 |
| `filter/levels` | string | `all` | 级别复选集合 |
| `filter/timeStrictInvalid` | bool | `true` | 无时间戳条目是否视为不匹配 |
| `general/maxLinesPerFile` | int | `2000000` | 单文件行数上限 |
| `general/continuationMerge` | bool | `true` | 续行合并 |
| `general/monitorHighlightNew` | bool | `true` | 新行短暂高亮 |
| `general/exportLineEnding` | string | `platform` | `platform` / `lf` |
| `view/rowHeightLines` | int | `2` | 行高默认显示行数 |
| `view/columnWidths` `view/sort` | QVariantMap | 空 | 按文档签名记忆列宽与排序 |
| `window/geometry` `window/state` | QByteArray | 空 | 主窗口状态 |
| `files/recent` | QStringList | 空 | 最近文件（≤ 10） |

## 9. 构建、运行与打包（双平台）

### 9.1 Windows（v1 验证路径）

```powershell
# 1) 安装 Qt 6.8.3（一次性，约 1.5–2 GB）
.\scripts\install-qt.ps1            
#    下载需 HTTP 代理（本机为 http://localhost:1081）：install-qt.ps1 会自动设置
#    HTTP_PROXY / HTTPS_PROXY / ALL_PROXY，也可用 -Proxy 参数覆盖
#    pip install aqtinstall + aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 -O D:\Qt

# 2) 配置 + 构建（脚本定位 VS 2022、导入 vcvars64、使用 Ninja）
.\scripts\build.ps1 -Config Release

# 3) 运行
.\scripts\run.ps1 -Config Release

# 4) 测试
.\scripts\test.ps1 -Config Release   # ctest --preset windows-msvc-qt6-release

# 5) 打包便携目录（windeployqt + 中文 .qm + 图标）
.\scripts\package.ps1 -Config Release   # → dist\log-viewer.exe

# 6) 文件关联（可选，HKCU，可撤销）
.\scripts\register-association.ps1                 # 注册 .log
.\scripts\register-association.ps1 -Unregister     # 完全撤销
.\scripts\register-association.ps1 -IncludeTxt     # 可选注册 .txt（OPEN-11）
```

注册表布局（HKCU，无需管理员）：

```
HKCU\Software\Classes\LogViewer.log\shell\open\command = "<abs>\log-viewer.exe" "%1"
HKCU\Software\Classes\.log                             = "LogViewer.log"   (仅当不存在用户既有值时)
HKCU\Software\Classes\Applications\log-viewer.exe\...  = 提供"打开方式"入口
```

### 9.2 Linux（源码级支持，v1 不验证）

```bash
# 依赖（预期清单，未验证 → OPEN-08）
sudo apt install build-essential cmake ninja-build \
                 qt6-base-dev qt6-base-dev-tools qt6-l10n-tools libgl1-mesa-dev

# 配置 / 构建 / 测试
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure -j"$(nproc)"

# 运行与打包
./scripts/run.sh build
./scripts/package-deb.sh build        # cpack -G DEB → log-viewer_<version>_amd64.deb
sudo cmake --install build --prefix /usr/local   # 或 dpkg -i 安装 .deb
./scripts/register-association.sh     # xdg-mime default log-viewer.desktop text/x-log
```

CPack DEB 关键配置（`packaging/linux/deb/CPackDeb.cmake`）：

| 项 | 值 |
| --- | --- |
| `CPACK_PACKAGE_NAME` / `CPACK_PACKAGE_VENDOR` | `log-viewer` / 项目作者 |
| `CPACK_DEBIAN_PACKAGE_MAINTAINER` | 项目作者（占位，可后续修改） |
| `CPACK_DEBIAN_PACKAGE_DEPENDS` | `libqt6widgets6, libqt6gui6, libqt6core6`（或 `CPACK_DEBIAN_PACKAGE_SHLIBDEPS=ON` 自动推导） |
| `CPACK_DEBIAN_PACKAGE_SECTION` | `utils` |
| 安装路径 | `/usr/bin/log-viewer`；`/usr/share/applications/log-viewer.desktop`；`/usr/share/icons/hicolor/*/apps/log-viewer.svg`；`/usr/share/metainfo/org.logviewer.LogViewer.metainfo.xml`；`/usr/share/log-viewer/translations/logviewer_zh_CN.qm` |
| 架构 | `CPACK_DEBIAN_PACKAGE_ARCHITECTURE=amd64` |
| 许可证 | 随包安装 `LICENSE`（GPL/LGPL 说明与 Qt LGPLv3 合规声明） |

### 9.3 CMake 关键点（双平台共用的条件逻辑）

- 组件：`Core Gui Widgets LinguistTools`（`Test` 仅在开关打开时查找）；`qt_add_translations()` 生成 `logviewer_zh_CN.qm`。
- `CMAKE_CXX_STANDARD 20`；Windows 追加 `/W4 /permissive- /utf-8` 与 `WIN32` 子系统；Linux 追加 `-Wall -Wextra -Wpedantic`。
- Qt 定位：`CMAKE_PREFIX_PATH` 由 preset 提供（Windows `D:/Qt/6.8.3/msvc2022_64`；Linux 留空使用系统 Qt，可用 `-DLOGVIEWER_QT_ROOT=` 覆盖）。
- 平台分支只允许：`if(WIN32)` → 链接 `Advapi32`（注册表查询）与资源文件；`if(UNIX AND NOT APPLE)` → 链接 `iconv` 所在 libc、安装 `.desktop`/MIME/AppStream。
- `install()` 规则在两种平台上均生效（Windows 生成 `dist` 布局可选用 `cmake --install`）。
- 单元测试注册到 CTest：`tst_*`；`bench_1m_lines` 由 `LOGVIEWER_BUILD_BENCH=ON` 控制。

## 10. 测试计划

| 测试文件 | 覆盖 | 关键断言 |
| --- | --- | --- |
| `tst_formats.cpp` | 12 个内置格式正/负例；探测优先级；列生成规则 | 附件样本：识别为 `tracing`；9187 条；DEBUG 9121 / INFO 23 / ERROR 43；最长消息 398 字符；目标模块 3 个；续行 0 |
| `tst_timestamp.cpp` | ISO8601（7 位小数、`+08:00`）、Java `,SSS`、syslog、仅时间、无时间；**英文月份缩写不依赖 locale** | 时区换算正确；跨年推断正确；无法解析返回无效值 |
| `tst_matcher.cpp` | 全词（ASCII/CJK 边界）、通配符、正则、大小写、模式长度上限 | 命中集合符合预期；非法正则返回错误而非崩溃 |
| `tst_lineindex.cpp` | LF/CRLF/混合、空行、无末尾换行、单行 > 1 MB、UTF-8 多字节跨块、UTF-16 | 行数与偏移正确；CRLF 与 LF 样本结果一致 |
| `tst_snippets.cpp` | JSON（嵌套/字符串内括号/数组）、XML（自闭合/属性/注释）、YAML（多行/嵌套）；误报负例 | 片段边界精确；反例不产生片段；截断规则生效 |
| `tst_highlight.cpp` | 级别色、时间戳色、关键词覆盖优先级、主题切换 | span 颜色与层级符合 §5.5 与 REQ-HL-07 |
| `tst_document.cpp` | 同格式合并时间交错、异构格式拒绝、无时间戳退化拼接 | 合并顺序稳定；拒绝时给出错误码与格式名 |
| `tst_watcher.cpp` | 追加、截断、rename 轮转、部分行（无换行）写入 | 新行完整出现且不重复；轮转后继续跟随（临时文件模拟，平台无关） |
| `tst_encoding.cpp` | UTF-8/UTF-8 BOM/UTF-16LE + **平台回退样本**（CP936 中文） | 转换文本与预期一致；非法字节产生 U+FFFD 并计数；Linux 上未启用时跳过并记为 `SKIP`（v1 不验证） |
| `tst_cli.cpp` | `--help/--version/--lang/--monitor/--format`、非法参数、退出码、位置参数解析 | 退出码 0/1/2 与输出内容符合 §7.2；`CliParser` 无 UI 依赖 |
| `bench_1m_lines` | 100 万行合成日志 | 索引 ≤ 2.5 s、过滤 ≤ 300 ms、内存 ≤ 250 MB（报告输出） |

手工冒烟清单（`tests/SMOKE.md`，M6 生成）：语言即时切换、三项字体、右/底布局切换、主题切换、监控跟随与暂停、双击复制、Enter 展开、导出过滤结果、拖拽多文件（含异构拒绝）、CLI 启动、文件关联注册/撤销。

> Linux 说明：所有测试基于 Qt 与临时目录，理论上可直接在 Linux 运行；但 **v1 不执行 Linux 侧构建与测试**（REQ-PLAT-10），文档与 README 必须携带该声明。

## 11. 性能预算与优化手段

| 场景 | 预算 | 手段 |
| --- | --- | --- |
| 打开 1.8 MB / 9187 行（附件样本） | ≤ 200 ms | 单次顺序读 + 一次扫描 |
| 打开 100 万行 / ~200 MB | ≤ 2.5 s 建索引 | 64 KB 块扫描、偏移表 `std::vector<qint64>`；工作线程 + 进度条 |
| 首屏渲染 | ≤ 100 ms | 只渲染可见行；预热可见窗口的 `ParsedEntry` 缓存 |
| 过滤应用 | ≤ 300 ms | worker 线程位图 + 前缀和；增量刷新；可取消 |
| 滚动 | ≥ 55 FPS | delegate 自绘 + 布局缓存；避免每帧重建 `QTextDocument` |
| Find 输入响应 | ≤ 200 ms（防抖后首帧） | 后台匹配 + 位图增量提交 |
| Monitor 追加延迟 | ≤ 300 ms | `QFileSystemWatcher` + 300 ms 轮询双保险；只读新增字节 |
| 内存 | ≤ 250 MB（1M 行 + 10 万缓存） | 索引 8 B/行；LRU 解析缓存；监控追加行与全局行数上限一致（`general.maxLinesPerFile`，REQ-MON-06） |

> Linux 备注：inotify watch 数量受 `fs.inotify.max_user_watches` 限制，轮询兜底保证功能不受影响（REQ-MON-08）。

## 12. 里程碑与验收

| 里程碑 | 交付内容 | 验收标准 |
| --- | --- | --- |
| **M1** 工程骨架与界面框架 | CMake/presets/scripts（Windows + Linux 脚本骨架）、Qt 安装脚本、`CliParser`、`PlatformInfo`、主窗口静态布局（三分组框 + 菜单）、Settings 菜单全部入口（字体三项/语言/布局/主题/关于）、i18n 骨架与即时切换、主题框架、设置持久化 | 程序可运行；英文↔中文即时切换；三项字体与右/底布局切换生效并持久化；1920×1080 下布局符合 §6.2；AC-14（CLI）通过 |
| **M2** 解析与表格 | `EncodingBackend`（双平台实现）+ `EncodingDetector`、12 个格式解析器、`LineIndex`、`LogSource`、`LogTableModel`、表头/单元格样式、行高与截断 | AC-01 通过；`tst_formats`/`tst_lineindex`/`tst_encoding` 通过 |
| **M3** 查找与过滤 | FindPanel、SearchFilterPanel、LevelFilterBar、TimeRangePanel、Matcher、TimeRangeFilter、高亮引擎（级别/关键词/片段）、匹配导航、导出 | AC-02/03/07 通过；`tst_matcher`/`tst_snippets`/`tst_highlight` 通过 |
| **M4** 详情、多文件与监控 | DetailPane、`LogDocument` 合并、续行合并、`LogWatcher`、智能跟随、`FileAssociation`（双平台行为） | AC-04/05/06/08/09/15 通过；`tst_document`/`tst_watcher` 通过 |
| **M5** 打磨与性能 | 深色主题细调、表格渲染优化、列宽/排序/展开状态记忆、基准确认、高 DPI 与 Wayland/X11 兼容性检查（Windows 侧验证） | AC-12 达标；视觉规范逐条核对 |
| **M6** 测试、文档与打包 | 测试补全、`README.md`/`README.zh_CN.md`（含双平台章节与未验证声明）、`package.ps1`（Windows）、`packaging/linux/` + CPack DEB 配置 + `package-deb.sh`、`register-association` 双平台脚本、SMOKE 清单 | AC-13/AC-18 通过；`dist\` 可双击运行；Linux 侧资源与配置静态检查通过（AC-17 标注未验证） |

> Linux 后续工作（不在 v1 验收范围）：在 WSL2 Debian 13（已探明 apt Qt 6.8.2 + WSLg 可用）或真实 Linux 机器上执行 AC-16/AC-17，并据结果修订文档。

## 13. 风险与对策

| 风险 | 影响 | 对策 |
| --- | --- | --- |
| **Linux 构建/运行未在本开发机验证**（用户决定，将在 Debian 原生环境自行验证） | 交付物在 Linux 上可能需小幅修正才能编译通过 | 代码只使用 Qt 与标准库；平台差异集中在 `src/platform/`；CMake 平台分支显式且简单；避免 MSVC 专有扩展（`/utf-8` 对应源码 UTF-8 无 BOM）；README 明确标注未验证；预留 CI 工作流文件供后续启用 |
| Qt 6.8.3 下载体积大（1.5–2 GB） | M1 起步慢 | 一次性安装；aqtinstall 支持断点续传；Windows 侧必需，Linux 侧不下载（用发行版包） |
| GB18030 解码差异 | 非中文区域设置、musl 或无 iconv 环境乱码 | 平台后端（Windows ANSI/CP936、Linux iconv）+ **内置紧凑 GB18030/GBK 解码表兜底**，三个平台行为一致（OPEN-09 采纳方案） |
| 表格渲染性能（自绘 + 高亮）不达标 | 大文件卡顿 | 布局缓存 + 只渲染可见单元格 + 片段检测剪枝；M5 专项优化 |
| 正则灾难性回溯 | UI 假死 | 匹配在可取消工作线程；模式长度 ≤ 512；文档提示（OPEN-06） |
| 深色主题下控件观感不一致 | 视觉不统一 | 只定制 GroupBox/表头/分隔条/状态栏等少量控件，其余用 Qt 原生调色板；跨平台统一（Fusion 基准） |
| 块格式（Windows 事件文本）与续行合并冲突 | 解析错乱 | 块格式自行消费整块行并跳过续行逻辑；测试覆盖半截块 |
| 监控轮转/截断导致重复或丢行 | 数据错误 | 记录已读字节与文件标识；检测到大小回退或替换则重开并去重；`tst_watcher` 覆盖（临时文件模拟，双平台可跑） |
| 无时间戳格式的时间过滤语义分歧 | 用户困惑 | 默认"不匹配"（可配置），详情面板与状态栏提示 |
| 文件关联被用户或第三方重置 | 关联失效 | 脚本可反复执行；`-Unregister` 完整撤销；文档说明系统更新可能重置关联 |

## 14. 开发约定

- 命名：类 `PascalCase`，成员变量 `m_` 前缀，方法 `camelCase`，常量 `kPascalCase`；文件 `ClassName.h/.cpp`，目录小写。
- 注释与提交信息一律英文；界面字符串全部 `tr()` 包裹，禁止拼接不可翻译句子；CLI `--help` 文本同样走翻译。
- 分层：`src/core`、`src/model`、`src/highlight`、`src/platform` 不得依赖 `src/ui`；`CliParser` 不得依赖 UI。
- 可移植性（REQ-PLAT-01、CON-9/CON-10）：
  - 禁止 `#ifdef _WIN32` / `Q_OS_*` 出现在 `src/platform/` 之外；
  - 禁止 `\` 或 `/` 字面量拼接路径，一律 `QDir`/`QFileInfo`/`QStandardPaths`；
  - 禁止假设文件名大小写、换行符、系统编码；
  - 不得依赖 MSVC 专有扩展（不写 `__declspec`、不用 `#pragma once` 之外的非标准头）；源码保持 UTF-8 无 BOM（LF 行尾）；
  - 新增平台相关代码必须在 `src/platform/` 提供两套实现并更新 §4.9 差异表。
- 每个公开类必须有对应单元测试或明确豁免；新增格式必须同时提交正/负例样本与 CRLF/LF 两个版本。
- 提交粒度：每个里程碑内按"功能 + 测试"成对提交；提交信息 `area: summary`（如 `parser: add windows event text format`）。

## 16. 实施记录

### 16.1 M1 完成情况（2026-09-29）

| 里程碑项 | 状态 | 证据 |
| --- | --- | --- |
| CMake + Presets + 构建/运行/测试脚本（含 VS 定位与 vcvars 导入） | ✅ | `scripts/build.ps1`、`run.ps1`、`test.ps1`、`install-qt.ps1` 可用 |
| Qt 6.8.3 LTS 安装（代理下载 1959 MB） | ✅ | `D:\Qt\6.8.3\msvc2022_64`（含 lupdate/lrelease/windeployqt） |
| 主窗口静态布局（Search & Filter / Log / Details 三分组框、内容列居中、分组框等宽） | ✅ | `screenshots/01-demo-english-preview.png`、`02-reallog-chinese-preview.png`；布局自检输出（见 16.3） |
| Settings 菜单全部入口（Font 三项 / Language / Layout / Appearance / About） | ✅ | 截图；`%APPDATA%\LogViewer\settings.ini` 已写入 |
| 中英文即时切换（Qt Linguist，125 条 0 未完成） | ✅ | 中文截图；`src/i18n/logviewer_zh_CN.ts` |
| 主题框架（Light / Dark / Follow system） | ✅ | `app/ThemeManager`，深色调色板与分组框样式 |
| 设置持久化（字体/语言/布局/主题/最近文件/窗口几何/分隔条） | ✅ | `tst_settings`（5 个用例）+ 运行时 `settings.ini` |
| 命令行接口（`--help/--version/--lang/--monitor/--format/--demo`，退出码 0/2） | ✅ | AC-14 手工验证 + `tst_cli`（12 个用例） |
| 表头加粗 + 渐变阴影、级别色块、行高 2 行、超长 `…` 截断 | ✅ | 截图验证（`screenshots/01-demo-english-preview.png` 的两行行高与 `…` 截断） |
| 详情面板（字段名加粗 + 完整消息 + 复制按钮，右/底可切、首次点击才出现） | ✅ | 截图；`REQ-DETAIL-01…07` 大部分已实现 |
| 双击单元格复制完整文本 / Ctrl+C / Enter 展开 / 右键菜单 | ✅ | `LogTableView`、`LogItemDelegate` |
| 单元测试 6/6 通过 | ✅ | `ctest --preset windows-msvc-qt6-release` |

### 16.2 相对设计文档的偏差（已实施并记录）

| 项 | 设计 | 实际实施 | 原因 |
| --- | --- | --- | --- |
| 设置入口 | `SettingsDialog`（Font/Language/Layout/Appearance/About 五页） | Settings 菜单直改（子菜单 + QFontDialog / QColorDialog / AboutDialog） | 与 `spec.md` REQ-UI-02…06 的菜单结构一致，避免重复入口 |
| 过滤面板类拆分 | FindPanel / SearchFilterPanel / LevelFilterBar / TimeRangePanel 四个类 | 单一 `FilterPanel` 类 | M1 控件数量有限，拆分留待 M3 接入引擎时按需进行 |
| 状态栏 | `StatusBarWidget` 独立类 | 内联在 `MainWindow` | 四个 QLabel（左下耗时标签 + 右侧文件名/统计/Monitor），耗时文本由 `core/DurationFormat` 生成，无需独立类 |
| M2 内容提前 | 仅 M1 骨架 | 同时落地了 `LineIndex`、`LogSource`、`tracing`/`generic` 解析器、`LogTableModel`、表头样式、截断与复制交互、详情面板 | 让 M1 可用真实日志验收，而不是只能看空壳界面 |

### 16.3 开发辅助工具（已实现）

| 工具 | 用法 | 说明 |
| --- | --- | --- |
| 演示数据 | `log-viewer --demo` | 覆盖全部日志级别、超长消息、内嵌 JSON/XML/YAML 片段、多行条目；用于界面预览与截图 |
| 布局自检 | 设置环境变量 `LOGVIEWER_DUMP_LAYOUT=<文件路径>` | 启动 1.2 s 后把窗口/内容列/分组框/表格/详情面板的尺寸与 `minimumSizeHint` 写入文件（含 `filterCollapse` 行：折叠状态、切换按钮的几何与可见区域），2.2 s 后自动退出；用于布局回归与 DPI 排查 |
| UI 截图脚本 | `capture-ui.ps1`（scripts 目录外，本地验证用） | 进程 DPI 感知 + 点击 + 窗口位图导出；产出 `screenshots/*.png` |

### 16.4 M2 交接清单

1. **解析器**：`syslog3164` / `syslog5424` / `journalctl_short` / `journal_json` / `json_lines` / `logfmt` / `csv_tsv` / `python_logging` / `serilog` / `log4j_logback` / `wevt_text` / `iis_w3c`（共 12 个，当前已实现 `tracing`、`generic`）。
2. **编码后端**：`src/platform/EncodingBackend`（Windows `MultiByteToWideChar` / Linux `iconv`）+ 内置紧凑 GB18030/GBK 解码表兜底 + 候选编码判定（GB18030 → Big5 → Shift_JIS → CP1252）；替换当前的 `fromLocal8Bit` 回退。
3. **后台索引与进度**：把 `LineIndex::build`、级别计数（当前上限 30 万行同步计算）迁移到 `JobRunner` 工作线程，带进度与取消；行数上限弹窗走同一流程。
4. **列与视图记忆**：`view/columnWidths`、`view/sort`（按文档签名）、列宽拖拽与双击自适应已可用但未持久化；动态 `extra` 列按 ≥50% 出现率生成（已实现，需补多格式样本测试）。
5. **性能**：100 万行基准（`bench_1m_lines`）与渲染缓存（当前每次绘制重新排版可见单元格）。

### 16.5 已知问题与后续项

1. 时间列默认 230 px 以保证完整时间戳单行显示；窗口更窄时会换行（可接受，列宽记忆在 M2/M5 补齐）。
2. 代码片段高亮、关键词高亮、级别/时间过滤尚未接入（M3）；过滤面板控件会给出"M3 接入"的提示信息。
3. Monitor 菜单项目前禁用并提示 M4 接入（`LogWatcher` 未实现）。
4. 详情面板消息区固定 `NoWrap`，换行开关计划随 M4 一起提供。
5. 多文件合并尚未实现（M4）：当前打开多个文件时只加载第一个并给出提示。
6. 显示器环境：4K 面板 + 200% 缩放（Qt `devicePixelRatio=2`，逻辑工作区 1920×1080）；"1920×1080 设计基准"按逻辑像素理解，默认窗口 1600×950 逻辑像素。

### 16.6 后续里程碑实施记录（M2 / M4 / M6）

| 里程碑 | 交付内容 | 验证 |
| --- | --- | --- |
| M2 解析器 | 新增 10 类解析器：`syslog3164`、`journalctl_short`（共用实现）、`syslog5424`、`journal_json`、`json_lines`、`logfmt`、`csv_tsv`、`python_logging`、`serilog`、`log4j_logback`、`wevt_text`（块式）、`iis_w3c`；共 14 个格式 | `tst_formats_extra` 13 个用例逐个断言字段与探测结果 |
| M2 解析器（补充） | 依据用户提供的真实导出文件新增 wevt_tsv（事件查看器制表符文本：按「级别 + 时间」识别事件行，多行消息作为续行合并并去掉引号包裹）与 wevt_xml（事件查看器 XML：新增 ILogFormat::parseDocument 文档级解析，按 <Event> 逐个建立条目，一行多事件也能正确切分；级别优先取 RenderingInfo 的本地化名，其次映射 System 的 0-5 数值） | 真实文件验证：	est-event-logs.txt → 27518 条目（INFO 26960 / WARN 408 / ERROR 150）、	est-event-logs.xml → 3620 条目（INFO 3525 / WARN 70 / ERROR 25）；	st_formats_extra 含两个确定性用例 |
| M2 条目模型 | 物理行与日志条目解耦：`ILogFormat::startsEntry/parseEntry`，`LogSource` 建立条目索引，支持**块式格式**（Windows 事件文本、IIS/CSV 表头）与**续行合并**（堆栈、折行消息）；纯文本文件自动退化为"一行一条目" | `tst_formats_extra::continuationLinesAreMerged / continuationMergingCanBeDisabled / windowsEventBlocks` |
| M2 编码 | `src/platform/EncodingBackend`：Windows `MultiByteToWideChar`（CP_ACP / 936 / 54936 / 950 / 932 / 1252）、Linux `iconv`；探测阶段判定 GB18030 → Big5 → Shift_JIS → CP1252，解码结果在状态栏与详情框显示 | 真实样本 + `tst_formats` 的 UTF-8 断言；非 UTF-8 路径待 Linux/GBK 样本补充 |
| M2 列宽记忆 | `view/columnWidths`：按"格式 id + 列标题"签名记忆非消息列宽度，重建文档后恢复 | 手工（`MainWindow::setProvider/closeEvent` 保存与恢复） |
| M4 多文件合并 | `core/LogDocument`：同格式校验（不一致时拒绝并说明）、按时间 k 路归并、无时间戳时按打开顺序拼接并提示、`File` 列通过 `IEntryProvider::sourceName(entry.sourceIndex)` 显示每个条目所属文件 | `tst_document` 6 个用例 |
| M6 打包 | `scripts/package.ps1` 生成便携目录（exe + Qt DLL/插件/翻译 + 文档，约 31 MB）；`scripts/register-association.ps1`（HKCU、可撤销、默认仅 `.log`）；Linux 侧 `packaging/linux/`（.desktop、AppStream、hicolor 图标、CPack DEB 配置）与 `scripts/*.sh`（build/test/run/package-deb/register-association） | 便携目录已实际运行验证（`--version`、打开真实日志 17958 条）；Linux 侧为静态交付（REQ-PLAT-10） |
| M6 文档 | `README.md`（英文）与 `README.zh_CN.md`（中文）：功能表、双平台构建/运行/打包、命令行、快捷键、菜单、设置位置、目录结构；`LICENSE`（MIT） | 已入库 |
| UI 折叠与全屏 | 查询与过滤分组框可折叠为标题单行（REQ-UI-13）；Settings ▸ Full Screen（REQ-UI-14，F11）进入全屏时自动折叠该分组框，`Esc` 退出全屏并恢复进入前的折叠状态与最大化状态；关闭窗口时不保存全屏几何；两者均不写入设置 | `tst_mainwindow_behavior` 新增 2 个用例：折叠高度约为展开的一半且表格获得空间、条件在折叠后继续生效；F11 / Esc 往返、折叠状态恢复与最大化往返 |

**尚未完成（诚实记录）**

1. **Linux 构建与运行验证**：由用户在 Debian 原生环境执行（OPEN-07）；因此 `.deb`、`.desktop`、iconv 路径与 `scripts/*.sh` 均未实际运行。
2. **列排序记忆**：REQ-TABLE-05 中的"排序状态记忆"未实现（未提供点击表头排序）；列宽记忆已实现。
3. **后台索引与进度**：索引、条目分组与过滤仍在 UI 线程同步执行（大文件时显示等待光标），尚未迁移到 `JobRunner`；100 万行基准脚本未编写。
4. **详情框行内展开上限**：完整内容模式引擎保留但 UI 不暴露（见 §4.8）。
5. `windeployqt` 未部署 VC 运行时与 d3d 编译器垫片，分发目录在目标机需要 VC++ 运行库（README 已说明）。

### 16.7 打开大文件性能修复（2026-09-29）

背景：命令行 / 双击打开 6.9 MB（38035 行）的 `sslocal.2026-09-27.log` 实测约 71 秒（用户报告"超过半分钟"；状态栏「加载耗时」显示约 1 分 11 秒）。逐段计时定位到三处放大：

| # | 根因 | 观测 | 修复 |
| --- | --- | --- | --- |
| 1 | `LogTableView::updateVisibleRowHeights()` 取不到末行时兜底为**整篇文档**：日志在窗口首次布局前打开（`viewport()->height() == 0`，`rowAt()` 返回 -1），而列宽应用 / 恢复会逐个 `setColumnWidth`，每次都在 `onSectionResized()` 里同步触发一次全量行高计算 | 单次 `setColumnWidth` 约 4.4–5.7 s，一次打开累计 50 s 以上 | 只更新视口内可见行；视口高度为 0（尚未布局）时直接返回，由 `applyRowHeights()` 与首次 resize/scroll 补齐 |
| 2 | `MainWindow::setProvider()` 期间每个中间步骤各跑一次完整行高 pass（model reset、列显隐、Columns 菜单重建） | 38035 行 × 3 次 | 新增 `LogTableView::setHeightPassSuspended()`：挂起期间只失效缓存，恢复时统一跑一次；`setProvider()` / `onDocumentRebuilt()` 使用 |
| 3 | `LogSource::readRawLine()` 每读一行重开一次文件 | 建条目索引 76270 次 open（约 0.87 s）、级别统计 38031 次（约 0.54 s） | 新增 `beginBulkRead()` / `endBulkRead()`（RAII `BulkReadScope`）：格式探测、条目索引、级别统计期间保持句柄，pass 结束立即关闭——两次 pass 之间不持有句柄，轮转/改名语义不变 |

实测（Release，本机 4K/200%；状态栏「加载耗时」，Windows UI Automation 读取）：

| 文件 | 行数 | 修复前 | 修复后 |
| --- | --- | --- | --- |
| `sslocal.2026-09-27.log`（6.9 MB） | 38035 | 约 71 s | **3.19 s** |
| `test-event-logs.txt`（5.4 MB） | 27518 | 未逐项实测 | **2.82 s** |
| `sslocal.2026-09-28.log`（3.4 MB） | 17958 | 约 16 s（§4.8 记录） | **1.28 s** |

验证：`ctest` 14/14 通过；三个真实样本的 `LOGVIEWER_DUMP_LAYOUT` 输出均为 `fullContent=1, truncatedRows=0`（完整内容模式行高仍精确、无截断）。双击启动到窗口可用的墙钟时间约 4.6 s，其中 `MainWindow::show()` 本身约 1.6 s（与日志无关，空窗口同样存在，用户决定暂不处理）。

## 15. 修订记录

| 版本 | 日期 | 修改人 | 说明 |
| --- | --- | --- | --- |
| 1.0 | 2026-09-29 | AI 助手 | 初稿：技术选型、架构、目录结构、核心模块、高亮引擎、UI 布局与线框图、交互与快捷键、设置表、构建打包、测试计划、性能预算、里程碑、风险、约定 |
| 1.1 | 2026-09-29 | AI 助手 | 增加 Linux 支持：技术选型双平台化、`src/platform/` 平台抽象层与新目录结构、§4.3 编码后端平台化、§4.9 平台差异表、§7.2 命令行接口、§9 双平台构建与 CPack DEB、`packaging/linux/` 桌面集成、`tst_cli`/`tst_encoding` 测试项、里程碑与风险补充"Linux 未验证"声明 |
| 1.2 | 2026-09-29 | AI 助手 | 依据用户对 OPEN 项的批复同步：取消单实例（多实例并行）、监控改为完整展示 + 全量追加（取消独立保留上限，改用全局行数上限）、内置 GB18030/GBK 兜底解码器并记录备选方案评估、新增 `--demo` 自检选项、记录下载代理要求（`http://localhost:1081`）、设置入口改为菜单直改（去掉多页设置对话框） |
| 1.3 | 2026-09-29 | AI 助手 | 新增 §16 实施记录：M1 完成情况与证据、相对设计的偏差、开发辅助工具（`--demo`、`LOGVIEWER_DUMP_LAYOUT`、UI 截图脚本）、M2 交接清单与已知问题；记录显示器为 4K + 200% 缩放（逻辑 1920×1080）对"设计基准"的影响 |
| 1.4 | 2026-09-29 | AI 助手 | 行号列改为表格第一列（列生成规则同步更新） |
| 1.5 | 2026-09-29 | AI 助手 | 移除内容列 1600 px 最大宽度：内容列随窗口比例伸展以铺满可用像素（§6.1/§6.2 同步） |
| 1.6 | 2026-09-29 | AI 助手 | 新增 Settings ▸ Details Pane 子菜单与完整内容行高模式：菜单结构、设置表、§4.8（含行高缓存/抽样/自愈策略与两模式对照表） |
| 1.7 | 2026-09-29 | AI 助手 | 缺陷修复：详情框仅在显式用户激活或 Show at Startup 时打开（rowActivatedByUser 信号 + 打开时机说明）；UI 源码拆分为静态库 logviewer_ui 以支持 MainWindow 行为测试；新增 tst_mainwindow_behavior（6 用例） |
| 1.8 | 2026-09-29 | AI 助手 | Settings ▸ Details Pane 简化为单个勾选项 Always Show Details（§6.3 菜单树、§8 设置键与迁移、§4.8 模式表重写；完整内容模式引擎保留但不再暴露）；UI 行为测试同步更新 |
| 1.9 | 2026-09-29 | AI 助手 | 勾选项更名为 Show Details Pane 并改为纯开关语义（勾选=始终显示，取消=始终不显示）；设置键 view/showDetailsPane 及三级迁移；关闭时 Layout ▸ Details 置灰；UI 行为测试同步（8 用例） |
| 1.10 | 2026-09-29 | AI 助手 | 详情框消息区改为始终自动折行且禁用横向滚动条（§4.8 补充内容区说明）；新增 UI 测试用例 |
| 1.11 | 2026-09-29 | AI 助手 | 实现 Monitor（§4.8 新增监控实现表：轮询+文件监视、增量索引、半行处理、轮转重建、不长期持有句柄、智能跟随按钮、计数同步）；快捷键表加入 Ctrl+M；测试新增 tst_monitor（5 用例）与 UI 监控用例，样本改为冻结切片 + 活文件结构断言 |
| 1.12 | 2026-09-29 | AI 助手 | 实现 M3：§4.8 新增「查找与过滤引擎」实现表（Matcher、FilterSpec、可见行映射、命中导航、片段 tokenizer、配色、详情高亮、导出）；测试新增 tst_matcher、tst_filter、tst_highlight 以及两条 UI 用例（共 11 个测试目标） |
| 1.13 | 2026-09-29 | AI 助手 | 完成 M2/M4/M6：新增 14 个日志格式与条目/续行模型、平台编码后端、列宽记忆、LogDocument 多文件合并、便携打包与双平台脚本、双语 README 与 LICENSE；新增 §16.6 实施记录与未完成项清单 |
| 1.14 | 2026-09-29 | AI 助手 | 补充 Windows 事件导出解析器 wevt_tsv / wevt_xml 与文档级 parseDocument 机制；级别表改用 QString 以支持中文级别名；时间戳解析支持单位数小时；§16.6 记录真实文件验证结果 |
| 1.15 | 2026-09-29 | AI 助手 | 关于(About) 提升为菜单栏顶级项（§6.3 菜单结构同步）；MainWindow 用 menuBar()->addAction() 直接挂载 |
| 1.16 | 2026-09-29 | AI 助手 | Layout 移入 Details Pane 子菜单（§6.3 同步）；pplyDetailsBehavior() 改为置灰整个 Layout 子菜单 |
| 1.17 | 2026-09-29 | AI 助手 | 缺陷修复：截断行改为带格式重绘（新增 LogItemDelegate::shiftFormatsToElidedLine() 并记录原因与复现路径） |
| 1.18 | 2026-09-29 | AI 助手 | 补实现拖拽打开文件（§7.1 交互表补充实现要点）；新增 UI 用例覆盖拖入合并与文件夹忽略 |
| 1.19 | 2026-09-29 | AI 助手 | Find / Filter 改为回车提交（§7.1 交互表更新）；FilterPanel 移除 textChanged 实时提交、保留 Enter/Apply/清空即时生效；MainWindow::gotoMatch() 对未提交模式先提交再跳转 |
| 1.20 | 2026-09-29 | AI 助手 | 缺陷修复：完整内容模式与「详情框关闭」绑定（§4.8 模式表恢复该行）；修复多段消息段落内行位置误用全局行号导致的文字下移/裁切 |
| 1.21 | 2026-09-29 | AI 助手 | 缺陷修复：行键改用「源序号 + 源内行号」（原「起始物理行」在 XML 类文档格式中不唯一，导致行高与展开状态串行）；行高扫描固定列宽快照并不再丢弃进行中的重算请求；writeLayout() 增加 truncatedRows 健康指标 |
| 1.22 | 2026-09-29 | AI 助手 | 级别过滤行改为按文档动态生成（§4.8 引擎表新增该行说明）；控件重建时先 `setParent(nullptr)` 再 `deleteLater()`，避免 `findChild()` 命中待删除的旧控件 |
| 1.23 | 2026-09-29 | AI 助手 | 缺陷修复：`wevt_xml` / `wevt_tsv` 的 Line 列改显源内条目序号（原为物理行号：XML 全部显示第 2 行、TSV 从 2 起）；新增 `ILogFormat::numbersEntriesSequentially()` 由 `LogSource` 统一编号；XML 解析器不再为定位行号重复扫描换行；删除未使用的 `LogSource::firstPhysicalLine()`；列生成规则补充 `Line` 取值说明 |
| 1.24 | 2026-09-29 | AI 助手 | 斑马纹 + 单元格分隔线（§5.6 渲染路径补充实现要点）：`LogItemDelegate::paint()` 按显示行序在 `Base` / `AlternateBase` 间交替、选中行用 `Highlight`；每格右缘/下缘各画 1px 分隔线，配色与 `LogHeaderView` 列分隔线统一（`#3A3A3E` / `#D9D9DD`）；新增像素级 UI 用例 `zebraStripesAndGridLines` |
| 1.25 | 2026-09-29 | AI 助手 | 新增顶级菜单 Columns（§6.3 菜单结构 + §4.8 引擎表新增「列显隐」行）：`LogTableModel` 持有隐藏列键集合（唯一事实源）、`columnsVisibilityChanged` 信号驱动 `LogTableView::applyColumnVisibility()`；`stretchColumn()` 在消息列隐藏时把 Stretch 让给最后一个可见列；消息列隐藏时行高退回默认（`messageColumnVisible()` 守卫）；列显隐按文档签名持久化（`SettingsStore::hiddenColumns`） |
| 1.26 | 2026-09-29 | AI 助手 | 级别过滤行去掉计数标签（§4.8 引擎表更新）：`rebuildLevelChecks()` 只建复选框、间距 4→14；计数改由 `levelTooltip()` 放进悬停提示，`m_lastLevelCounts` 保存最近一次直方图供语言切换时重写提示 |
| 1.27 | 2026-09-29 | AI 助手 | 缺陷修复：`LogItemDelegate::sizeHint()` 由「回显 option.rect 宽度」改为**按完整文本计算内容宽度**（+ 内边距；级别列加 chip 内边距并用粗体；上限 `kMaxSizeHintWidth=2000`，长行只测前 `kMaxMeasuredChars=512` 字符）——此前 `resizeColumnToContents()`（右键 Auto-fit / 双击列边界）会把所有列压成同一最小值。§5.6 渲染路径补充该约定；新增 UI 用例 `autoFitColumnsFitsContent` |
| 1.28 | 2026-09-29 | AI 助手 | 列宽自适应算法改为「时间列完整、消息列优先、其余按内容比例缩放」（§4.1 列生成规则新增该段）：`autoFitColumns()` 先预留时间列（`max(内容, 215)`，不换行）与消息列份额（≥ 40%、≥ 240 px），其余列在预算内等比分配（充裕取内容宽度、不足按比例缩小、下限 24 px、末列吃余数）；修复紧空间分支未跳过时间列的回归缺陷；§16 增加 `columnWidths=` 布局自检输出 |
| 1.29 | 2026-09-29 | AI 助手 | 列宽自适应改为**严格内容宽度**（§4.1 段落更新）：`内容 + kFitSafetyPixels(2)`、表头标题更宽时取 `headerTitleWidth()`、下限 `kMinFittedColumnWidth(36)`、上限 600 px——时间列不再用 215 px 最小宽度预留（留白约 50 px 的根因）；新增 `LogTableView::headerTitleWidth()`；用例增加宽度上界断言 |
| 1.30 | 2026-09-29 | AI 助手 | 级别列完整显示（§4.1 段落更新）：新增 `LogTableView::contentWidthHint()`（前 500 行连续 + 其后 3500 行均匀抽样）并覆盖 `sizeHintForColumn()`——Qt 自带实现只看向前部窗口、曾漏掉更宽的级别值；时间列与级别列都改为预算前**预留**、不参与缩放；用例新增级别 chip 完整性与窄窗口断言 |
| 1.31 | 2026-09-29 | AI 助手 | 新增反向过滤（REQ-FILTER-09）：`FilterSpec::invertKeyword` + `LogTableModel::entryMatches()` 取反；`FilterPanel` 新增 Invert 勾选框（切换即 emitFilterChanged，Clear 同步取消勾选），`MainWindow::applyFilterSettings()` 传入该标志；§4.8 引擎表 FilterSpec 行更新；新增 UI 用例 `invertedFilterShowsNonMatchingRows` |
| 1.32 | 2026-09-29 | AI 助手 | 查询与过滤分组框可折叠为单行（REQ-UI-13：输入行容器 + 标题栏右侧 ▾/▸ 按钮）；Settings ▸ Full Screen（REQ-UI-14，F11，Esc 退出）：进入时自动折叠、退出时恢复原折叠状态与最大化状态，F11 勾选态与窗口状态双向同步，关闭窗口时不持久化全屏几何；**全屏切换由 `setWindowState()` 改为 `showFullScreen()`/`showMaximized()`/`showNormal()`**——否则「最大化 → 全屏」在 Windows 上窗口右移约 48 逻辑像素、折叠按钮被裁到屏幕外；`writeLayout()` 增加折叠按钮几何输出；§4.8 引擎表、§6.1/§6.2/§6.3、§7.1、§16.6 同步；新增 2 个 UI 用例（含最大化往返） |
| 1.33 | 2026-09-29 | AI 助手（缺陷修复） | 修复「目标列最后一行文本超出单元格边框」：① 完整内容模式行高改为**覆盖所有可见列**（`contentHeightForRow()`：消息列作基准 + 其余列 `maxLines+1` 探测，超过基准才完整计算；实测 4664 行 +约 30% 开销），原实现只按消息列导致长目标 + 短消息的行高不足；② `drawClampedText()` 段落裁剪改用 `Qt::IntersectClip`（原 `ReplaceClip` 替换掉单元格裁剪、放行越界绘制）；行高缓存/自愈改为**全列宽快照**（`columnWidthSnapshot()`，`m_heightPassWidths`），任意列宽变化统一失效 + 延迟合并重算；`LOGVIEWER_DUMP_LAYOUT` 改为 dump 完成即退出（报告现在逐行校验行高，固定宽限期会截断大文档输出）；新增 UI 用例 `rowHeightCoversTheTallestColumn`（含像素级越界检查，双向验证：还原旧 clip 或旧行高算法都会失败） |
| 1.34 | 2026-09-29 | AI 助手（依据用户反馈） | 新增 REQ-UI-15：状态栏左下角显示最近一次打开日志的耗时（自适应 `s` / `min` / `h` 分量格式，排除大文件对话框等待；Refresh 更新、关闭文档与 `--demo` 清除、语言切换重译）；新增 `core/DurationFormat`（`formatDuration()`，上下文 `DurationFormat`）与 `tst_duration`，UI 用例覆盖标签位置与清除；§4.8、§6.1、§6.2、§16.2 同步 |
| 1.35 | 2026-09-29 | AI 助手（性能优化） | 打开大文件性能修复（新增 §16.7）：① `updateVisibleRowHeights()` 不再在视口高度为 0（首次布局前）时遍历整篇文档，只更新可见行；② 新增 `LogTableView::setHeightPassSuspended()`，`setProvider()` / `onDocumentRebuilt()` 期间合并为单次行高 pass；③ `LogSource` 新增 `beginBulkRead()` / `endBulkRead()`，条目索引 / 级别统计不再逐行重开文件（pass 之间不持有句柄，轮转语义不变）。实测 6.9 MB / 38035 行由约 71 s 降至 3.19 s（5.4 MB 为 2.82 s），`ctest` 14/14 通过、`truncatedRows=0` |
| 1.36 | 2026-09-29 | AI 助手（缺陷修复） | 修复「切换界面语言后日志表格列头仍为旧语言」：`LogTableModel` 不再在 `rebuildColumns()` 时缓存译好的列标题（`Column` 去掉 `title` 字段），`headerData()` 改为按需翻译（extra 列仍用数据键，不翻译）；新增 `LogTableModel::retranslateHeaders()` 并由 `MainWindow::retranslateUi()` 调用，发出 `headerDataChanged` 使表头随语言切换即时刷新（REQ-I18N-02），Columns 菜单文本同步更新；新增 UI 用例 `headersFollowLanguageSwitch`（英文 → 中文 → 英文，含无翻译目录时 SKIP 守卫） |
