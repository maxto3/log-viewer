# Log Viewer 需求规格说明书（Spec）

| 项目 | 内容 |
| --- | --- |
| 文档版本 | 1.1 |
| 日期 | 2026-09-29 |
| 产品名称 | Log Viewer（可执行文件名 `log-viewer`） |
| 关联文档 | `design-doc.md`（技术设计）、`README.md` / `README.zh_CN.md`（用户文档，实施阶段生成） |
| 状态 | 已定稿，待用户确认后进入实施 |
| 目标平台 | **Windows 11 x64（首要支持，v1 交付并验证）** + **Linux x64（源码级跨平台支持，v1 不做构建验证）** |

本文件是需求的唯一权威来源（Single Source of Truth）。后续任何需求变更都必须修改本文件并更新修订记录，再同步到 `design-doc.md`。

## 目录

1. [范围与目标](#1-范围与目标)
2. [术语](#2-术语)
3. [技术约束](#3-技术约束)
4. [功能需求](#4-功能需求)
5. [界面与视觉需求](#5-界面与视觉需求)
6. [非功能需求](#6-非功能需求)
7. [范围外（Out of Scope）](#7-范围外out-of-scope)
8. [验收标准](#8-验收标准)
9. [开放问题与建议项](#9-开放问题与建议项)
10. [修订记录](#10-修订记录)

---

## 1. 范围与目标

### 1.1 目标

构建一款跨平台（Windows / Linux）桌面 GUI 日志查看器，用于高效阅读、搜索、过滤和追踪文本日志文件。设计参考 KDE Plasma 的 KSystemLog 的信息组织方式（级别区分、过滤面板、详情面板、状态栏统计），但视觉与交互遵循本文档第 5 节的对称化布局规范。

### 1.2 必须覆盖的日志类型

| 编号 | 类型 | 说明 |
| --- | --- | --- |
| FMT-1 | Rust `tracing` / `tracing-subscriber` | 见 `sslocal.2026-09-28.log` 实样本 |
| FMT-2 | syslog（RFC3164 / RFC5424）与 `journalctl` 导出（short / json） | 含 `PRI` / `PRIORITY` 级别映射 |
| FMT-3 | Python `logging`、.NET/Serilog 文本、Java log4j / logback | 常见模板，含 3 字母级别缩写（INF/WRN/ERR…） |
| FMT-4 | 结构化行：JSON Lines、logfmt(`k=v`)、带表头 CSV/TSV | 键名自动映射到标准字段 |
| FMT-5 | Windows 事件日志导出与 IIS W3C 日志 | 事件查看器「文本(制表符分隔)」导出（`wevt_tsv`）、「XML」导出（`wevt_xml`，一个物理行可含多个事件，按事件建立条目）、`Format-List` 文本块（`wevt_text`）、CSV/TSV 导出（`csv_tsv`）；IIS W3C（`iis_w3c`） |
| FMT-6 | 通用启发式兜底 | 任意"时间 + 级别 + 消息"文本日志；无级别时退化为消息单列 |

> 未列入 v1 专门解析器：Apache / Nginx 访问与错误日志（由 FMT-6 兜底：可提取时间戳，其余归入消息列）。如需专门解析器，属于后续增量需求。

### 1.3 平台矩阵

| 平台 | 支持级别 | v1 构建验证 | v1 交付形式 |
| --- | --- | --- | --- |
| Windows 11 x64 | 首要支持 | **验证**：MSVC 2022 + Qt 6.8.3 (msvc2022_64)，单元测试 + GUI 冒烟 | `scripts\package.ps1` → `dist\` 便携目录（windeployqt） |
| Linux x64（glibc ≥ 2.28） | 源码级跨平台支持 | **不验证**（用户决定；Linux 构建验证列为后续工作，见 §9 OPEN-07） | 源码 + CMake `install` 规则 + **CPack DEB（`.deb`）** + `.desktop` / MIME / AppStream（配置交付，产物未验证） |
| macOS | 不支持 | — | 范围外 |

## 2. 术语

| 术语 | 含义 |
| --- | --- |
| 物理行 | 文件中的一行（以 `\n` / `\r\n` 分隔） |
| 日志条目（Entry） | 表格中显示的一行；可能由多行物理行合并而成（续行合并） |
| 续行 | 不以格式锚点（时间戳/级别等）开头的物理行，视为上一条日志的延续 |
| 源（Source） | 单个被打开的日志文件 |
| 文档（Document） | 当前打开的 1 个或多个同格式源的集合 |
| Find | 查找：命中项高亮显示，**不隐藏**任何行 |
| Filter | 过滤：不匹配的行**从表格中隐藏** |
| 片段 | 日志消息中内嵌的 JSON / XML / YAML 代码文本 |
| 平台抽象层 | `src/platform/` 下集中承载平台差异的代码，其余代码不得出现平台条件编译 |

## 3. 技术约束

| 编号 | 约束 |
| --- | --- |
| CON-1 | 基于开源 Qt：**Qt 6.8.x LTS（LGPLv3）**。Windows 使用 msvc2022_64 预编译包（6.8.3）；Linux 使用发行版 Qt 6.8.x 软件包或 aqtinstall gcc_64 包（6.8.3）。使用 Qt Widgets，不使用 QML |
| CON-2 | 语言标准 **C++20**；构建系统 CMake ≥ 3.25 + Ninja |
| CON-3 | 编译器：Windows = MSVC 2022 x64；Linux = GCC ≥ 13 或 Clang ≥ 16 |
| CON-4 | 不引入第三方运行时依赖（JSON/XML/YAML 片段高亮自研；SVG 图标内置；Linux 编码回退使用 glibc 内置 `iconv`） |
| CON-5 | Windows 通过 `aqtinstall` 免账号下载 Qt 官方二进制到 `D:\Qt`；Linux 优先发行版软件包，也可用 aqtinstall 保持版本一致 |
| CON-6 | 不使用 Qt Creator；构建/运行/打包全部通过 `scripts/*.ps1`（Windows）与 `scripts/*.sh`（Linux）+ CMake preset 完成 |
| CON-7 | GUI 界面语言默认**全英文**，并提供中文界面；翻译用 Qt Linguist（`.ts` → `.qm`） |
| CON-8 | 代码注释、提交信息一律使用英文 |
| CON-9 | **可移植性**：除 `src/platform/` 外禁止平台专有 API 与 `#ifdef _WIN32/Q_OS_*`；路径一律使用 `QDir`/`QFileInfo`/`QStandardPaths`，禁止拼接 `\` 或 `/` 字面量 |
| CON-10 | 不假设大小写不敏感文件系统；不假设换行符；不假设本地 8 位编码为 GBK |
| CON-11 | 配置存储：Windows `%APPDATA%\LogViewer\settings.ini`；Linux `$XDG_CONFIG_HOME/LogViewer/settings.ini`（默认 `~/.config/LogViewer/`），统一通过 `QStandardPaths::AppConfigLocation` 获取 |
| CON-12 | 最低 glibc 2.28（Qt 6.8 官方 Linux 二进制要求）；不支持 musl 环境（Alpine）的 GB18030 解码（见 OPEN-09） |

## 4. 功能需求

需求编号规则：`REQ-<域>-<序号>`；优先级：`必须` / `应该` / `可选`。

### REQ-FILE 文件与文档管理

| 编号 | 需求 | 优先级 |
| --- | --- | --- |
| REQ-FILE-01 | File 菜单提供 **Open**（支持多选，多选即合并打开）与 **Refresh**（重新从磁盘读取当前文档，保留过滤条件与滚动位置尽量不变） | 必须 |
| REQ-FILE-02 | 支持将日志文件**拖拽**到主窗口打开（Windows 资源管理器 / Linux 文件管理器均可）；拖入多个文件时按合并规则处理；拖拽目标为主窗口任意位置（子控件不吞掉拖放事件），文件夹被忽略并提示 | 必须 |
| REQ-FILE-03 | File 菜单提供 **Monitor** 可勾选菜单项（详见 REQ-MON） | 必须 |
| REQ-FILE-04 | 多文件合并仅允许**同一格式**（格式 ID 与列结构一致）；不一致时拒绝加载并弹窗说明原因与检测到的格式名 | 必须 |
| REQ-FILE-05 | 多文件合并时按时间戳交错排序；任一源无法解析时间戳时，退化为按源打开顺序拼接并在状态栏提示 | 必须 |
| REQ-FILE-06 | 打开新文档时若已有未保存状态（如监控中），先提示确认 | 应该 |
| REQ-FILE-07 | 记住最近打开文件列表（File 菜单底部，最多 10 项），可清空 | 必须 |
| REQ-FILE-08 | File 菜单提供 **Export Filtered Results…**：将当前过滤后的可见行导出为 CSV 或纯文本（UTF-8 with BOM）；换行符默认使用平台换行，可选择强制 LF | 应该 |
| REQ-FILE-09 | File 菜单提供 **Close** 与 **Exit** | 应该 |

### REQ-PARSE 解析与编码

| 编号 | 需求 | 优先级 |
| --- | --- | --- |
| REQ-PARSE-01 | 自动探测日志格式（第 1.2 节 FMT-1…FMT-6），无需用户手工选择；探测结果在状态栏显示 | 必须 |
| REQ-PARSE-02 | 表格列**自动依据日志内容判定**；默认标准列包含 **Time、Level、Message**，并按解析结果补充 Thread、Target、PID、Host 等列 | 必须 |
| REQ-PARSE-03 | 表格中**第一列（最左侧）固定为 Line 列**：默认为**源文件物理行号**（便于与原文对照）；对**记录式导出**（Windows 事件日志 XML / TSV）显示**源内条目序号**（1 起）——这类导出的物理行无法标识记录（XML 导出整个文档常在一行内、所有事件同为一个物理行；TSV 首行是表头，数据从第 2 行开始）；多文件合并时紧随其后增加 **File** 列 | 必须 |
| REQ-PARSE-04 | 编码自动检测：BOM（UTF-8/UTF-16/UTF-32）→ 严格 UTF-8 校验 → UTF-16 → 本地 8 位编码回退；**所有非 UTF-8 内容自动转换为 UTF-8 后显示**，不当场改写磁盘文件。回退编码在 Windows 使用系统 ANSI 代码页（简体中文 = CP936/GB18030），在 Linux 使用 `iconv`（GB18030 → UTF-8） | 必须 |
| REQ-PARSE-05 | 无法解码的字节以 U+FFFD 占位，并在状态栏给出提示 | 应该 |
| REQ-PARSE-06 | 续行合并：不以格式锚点开头的物理行自动合并到上一条日志的 Message（默认开启，Settings 中可关闭） | 必须 |
| REQ-PARSE-07 | 时间戳解析支持：`ISO8601`（含小数秒与 `Z`/`±HH:MM` 偏移）、`YYYY-MM-DD HH:MM:SS[,.SSS]`、syslog `Mmm dd HH:MM:SS`、仅时间 `HH:MM:SS`（日期由文件名或文件修改时间推断，并在详情面板标注"日期为推断值"） | 必须 |
| REQ-PARSE-08 | 级别归一化为 TRACE / DEBUG / INFO / NOTICE / WARN / ERROR / FATAL / OTHER，并保留原始文本；syslog 数字优先级按 RFC5424 映射 | 必须 |
| REQ-PARSE-09 | 单文件默认行数上限 2,000,000 行（Settings 可调）；超过上限时弹窗提示"已加载前 N 行"，并提供 **Continue loading all** 与 **Cancel** 选项 | 必须 |
| REQ-PARSE-10 | 解析失败的行不丢弃：整行作为 Message 显示，级别记为 OTHER | 必须 |
| REQ-PARSE-11 | 换行符 CRLF / LF / 混合 均可正确索引与显示（Windows 与 Linux 生成的日志混用场景） | 必须 |

### REQ-FIND 查找（高亮，不隐藏）

| 编号 | 需求 | 优先级 |
| --- | --- | --- |
| REQ-FIND-01 | 独立的 **Find** 输入框；命中项高亮显示，**不隐藏任何行** | 必须 |
| REQ-FIND-02 | 匹配模式可切换：**全词匹配 / 通配符匹配（`*` `?`）/ 正则表达式**（PCRE2，Qt `QRegularExpression`） | 必须 |
| REQ-FIND-03 | 可通过 GUI 切换是否**忽略大小写** | 必须 |
| REQ-FIND-04 | 命中高亮默认 **绿色背景 + 黑色字体**，颜色可在 Settings 中修改 | 必须 |
| REQ-FIND-05 | 提供匹配导航：跳到上一条 / 下一条命中（F3 / Shift+F3），并显示 `当前序号/命中总数`；无命中显示 `0/0` | 应该 |
| REQ-FIND-06 | 匹配范围覆盖表格所有可见列；Message 列为主要场景 | 应该 |
| REQ-FIND-07 | Find 同样**在回车（或 ▶/◀ 导航按钮、F3）时提交**：输入过程中不扫描文档、不刷新表格；清空输入框时立即清除高亮。未提交的模式在按 F3 时会先自动提交再跳转 | 应该 |
| REQ-FIND-08 | 全词匹配规则：以 `[A-Za-z0-9_]` 为单词字符做边界判定；当模式首/尾字符为 CJK（或其他非单词字符）时该侧不做边界判定（保证中文可全词匹配） | 必须 |

### REQ-FILTER 过滤（隐藏不匹配）

| 编号 | 需求 | 优先级 |
| --- | --- | --- |
| REQ-FILTER-01 | 独立的 **Filter** 输入框（与 Find 分开，可同时生效）；匹配模式与忽略大小写开关独立于 Find 设置。**过滤在用户按回车或点击 Apply 后执行，输入过程中不刷新表格**；清空输入框时立即恢复全部行；级别复选与时间范围仍为点击即时生效 | 必须 |
| REQ-FILTER-02 | 按**级别**过滤：级别复选列表**按当前文档动态生成**——只列出实际出现的级别（TRACE/DEBUG/INFO/NOTICE/WARN/ERROR/FATAL/OTHER 中计数 > 0 的项），切换文档时重建列表（不保留 0 计数的置灰占位项）；**不显示各级别计数**（计数只通过复选项悬停提示给出）；用户取消勾选的级别在列表重建后保持取消 | 必须 |
| REQ-FILTER-03 | 按**开始/截止时间**过滤：可选复选框启用，配起止时间选择器；比较按**绝对时刻**（含日志自带时区偏移换算） | 必须 |
| REQ-FILTER-04 | 时间预设：最近 5 分钟 / 最近 15 分钟 / 最近 1 小时 / 今天 / 全部 | 应该 |
| REQ-FILTER-05 | 关键词过滤命中范围与 Find 一致（所有可见列）；可与级别、时间条件叠加（AND） | 必须 |
| REQ-FILTER-06 | 提供 Clear / Reset 按钮一键清除全部过滤条件 | 必须 |
| REQ-FILTER-07 | 过滤后状态栏显示 `显示行数 / 总行数` | 应该 |
| REQ-FILTER-08 | 时间过滤对无法解析时间的条目：默认视为不匹配（可在 Settings 中改为放行），并在详情面板说明 | 应该 |
| REQ-FILTER-09 | **反向过滤**：Filter 行提供 **Invert** 勾选框；勾选后**只显示不匹配过滤关键词/模式的行**（匹配的行被隐藏），未勾选时保持原语义；与级别、时间条件按 AND 叠加；**切换勾选框立即生效**（无需回车）；过滤输入为空时不产生任何隐藏（反向亦不例外）；**Clear** 一键清除时同时取消勾选 | 必须 |

### REQ-TABLE 表格展示

| 编号 | 需求 | 优先级 |
| --- | --- | --- |
| REQ-TABLE-01 | 日志内容以**表格**形式展示，列由日志内容自动判定（REQ-PARSE-02） | 必须 |
| REQ-TABLE-02 | 表头**字体加粗**，背景使用**类阴影渐变**效果（浅色主题：浅灰→略深灰；深色主题：对应深色渐变），带细底边线 | 必须 |
| REQ-TABLE-03 | 单元格内容**水平靠左、垂直居中** | 必须 |
| REQ-TABLE-04 | 行高默认容纳**两行文本**；超长内容按 `…` 截断表示（选中行后按 Enter/Space 可临时展开整行查看，上限 30 行）。**例外**：当 Settings ▸ Details Pane ▸ Show Details Pane 关闭时，表格作为唯一阅读位置进入**完整内容模式**——每行高度按内容自适应、不截断（单条约安全上限 200 行） | 必须 |
| REQ-TABLE-05 | 表格支持列宽拖动、双击列边界按**内容**自适应、列宽与**列显隐状态**记忆（按文档签名保存）；右键菜单 **Auto-fit Columns** 与双击列边界同义，算法为：① **时间列与级别列始终完整显示**（时间戳不被换行、级别 chip 不被裁切；宽度 = 内容宽度 + 2 px 取整余量），两列在预算前**预留**、不参与比例缩放；② **消息列优先**拿走剩余宽度（不低于表格的 40%，至少 240 px）；③ **其余列按内容自适应**（内容宽度 + 2 px；表头标题更宽时以表头为准；下限 36 px、上限 600 px），空间不足时在剩余宽度内按内容长度等比缩小（下限 24 px，末列吃余数）；④ 内容宽度必须采样**整篇文档**（前 500 行连续 + 其后均匀抽样，共约 4000 行；Qt 自带的 `sizeHintForColumn()` 只看向前部窗口，会漏掉更宽的级别/目标值——级别列曾因此按 INFO 拟合而裁掉 DEBUG 的 chip）；⑤ 不得为凑最小宽度留下可见空白 | 应该 |
| REQ-TABLE-06 | 行内展开：选中行后按 **Enter 或 Space** 切换该行完整高度展开（展开状态不随过滤/排序丢失，按「源序号 + 源内行号」标识保持；该标识必须对每个条目唯一，物理行号在 XML 类文档格式中不唯一，不可单独用作 ID）；展开状态在行首标记可见 | 应该 |
| REQ-TABLE-07 | 支持行选中（单选）与单元格区域选择；`Ctrl+C` 复制选中内容 | 应该 |
| REQ-TABLE-08 | 大文档下滚动流畅（见 REQ-PERF-01），只渲染可见单元格 | 必须 |
| REQ-TABLE-09 | 行**隔行交替底色**（斑马纹）以提升可读性：奇数行使用主题的 AlternateBase 底色（按**显示行序**交替，过滤后仍逐行交替）；选中行使用选中色，斑马纹不覆盖选中状态 | 必须 |
| REQ-TABLE-10 | 单元格之间显示**分隔线**：每格右缘一条竖线、下缘一条横线（深/浅主题各一套低对比配色，与表头列分隔线同色），便于按列/行对齐查看；单元格分隔线不因行展开或完整内容模式而缺失 | 必须 |
| REQ-TABLE-11 | **列显隐**（入口见 REQ-UI-12）：取消勾选的列在表格中不显示，其数据与列宽仍保留（重新勾选即恢复原宽度）；**隐藏的列不参与关键词过滤与 Find**（不可见的高亮没有意义），**导出**（Ctrl+E）只写当前显示的列；列显隐与列宽一样**按文档签名记忆**（REQ-TABLE-05）；当**消息列被隐藏**时，行高退回默认两行模式（不再按内容自适应、Enter 展开无效），避免为不可见内容预留高度 | 必须 |

### REQ-DETAIL 详情面板

| 编号 | 需求 | 优先级 |
| --- | --- | --- |
| REQ-DETAIL-01 | 详情分组框的显示由 Settings ▸ Details Pane ▸ **Show Details Pane** 决定（见 REQ-UI-11）：勾选时始终显示并在打开日志时自动展开第一条，取消勾选时始终隐藏 | 必须 |
| REQ-DETAIL-02 | 详情面板完整展示因超长而被截断的日志内容，以及未在表格中完整显示的其它列内容（不截断） | 必须 |
| REQ-DETAIL-03 | 详情面板为单页结构：上部**字段表**（字段名**加粗**：Time / Level / Thread / Target / PID / Host / File / Line / 自定义列），下部**完整 Message**（语法高亮、只读、可选中） | 必须 |
| REQ-DETAIL-04 | Message **始终自动折行**（按面板宽度换行、长单词/无空格串按任意位置断开），**不得出现横向滚动条**；内容过长时在面板内纵向滚动，不撑破布局；字段值同样自动折行 | 必须 |
| REQ-DETAIL-05 | 提供复制按钮：复制 Message、复制全部字段 | 应该 |
| REQ-DETAIL-06 | 面板位置可切换：主窗口**右侧**（默认）或**底部**（Settings ▸ Layout），切换后立即生效并记忆；分隔条尺寸可拖动并记忆 | 必须 |
| REQ-DETAIL-07 | 多条物理行合并的条目，详情面板按原样显示所有物理行 | 应该 |

### REQ-HL 语法高亮

| 编号 | 需求 | 优先级 |
| --- | --- | --- |
| REQ-HL-01 | 日志消息文本自动套用 VSCode 展示 `.log` 文件时的语法高亮模式：时间戳弱化色、级别着色（ERROR 红 / WARN 黄 / INFO 蓝 / DEBUG 绿 / TRACE 灰等） | 必须 |
| REQ-HL-02 | 消息中含 **JSON / XML / YAML** 片段时，自动按 VSCode 默认主题配色高亮代码片段 | 必须 |
| REQ-HL-03 | 提供 VSCode **Dark+** 与 **Light+** 两套 token 配色；默认**跟随应用主题**，也可在 Settings 中强制指定 | 必须 |
| REQ-HL-04 | 片段识别需控制误报：最小长度阈值 ≥ 8 字符、要求括号/标签配对完整、单条消息最多高亮 4 个片段、单片段 Token 化上限 2000 字符 | 必须 |
| REQ-HL-05 | 高亮不得显著影响滚动性能（见 REQ-PERF-01），渲染结果按 行/列/版本 缓存；**被 `…` 截断的行、以及扩展/完整内容模式下的行都必须保留高亮**（不得因列宽变化而丢失着色） | 必须 |
| REQ-HL-06 | 详情面板对完整消息做等价高亮（可跨物理行的多行片段） | 应该 |
| REQ-HL-07 | 关键词命中高亮（Find/Filter）优先于代码片段配色（关键词高亮底色覆盖其上） | 必须 |

### REQ-MON Monitor（tail -f）

| 编号 | 需求 | 优先级 |
| --- | --- | --- |
| REQ-MON-01 | 仅当**只打开了一个文件**且勾选 Monitor 时生效；多文件时菜单项禁用并给出提示；`--demo` 等非文件文档禁用该菜单项；快捷键 `Ctrl+M` | 必须 |
| REQ-MON-02 | 实现 `tail -f` 语义：追加写入的新日志自动出现在表格末尾（保留过滤条件下旧行不重复显示） | 必须 |
| REQ-MON-03 | 智能跟随：视图位于底部时自动滚动；用户上滚阅读时暂停跟随，并显示"新行 N ↓"按钮，点击后跳到底部恢复跟随 | 必须 |
| REQ-MON-04 | 新到达的行短暂高亮提示（可关闭） | 应该 |
| REQ-MON-05 | 检测文件轮转（rename/replace）与截断，自动重新打开并继续跟随，状态栏提示 | 必须 |
| REQ-MON-06 | 监控模式**先完整展示打开时所含的全部内容**，随后把新到达的内容全部追加显示；不设独立的"保留最近 N 行"上限，容量约束统一沿用全局限制（REQ-PARSE-09 的 `general.maxLinesPerFile`，默认 200 万行），超出时走同一"可继续加载"流程 | 必须 |
| REQ-MON-07 | 监控开启时禁止对同一文件重复打开；退出或关闭文件时停止监控 | 应该 |
| REQ-MON-08 | 文件监视机制在两个平台均可工作：Windows 使用 `QFileSystemWatcher`（ReadDirectoryChangesW），Linux 使用 `QFileSystemWatcher`（inotify）；两者都必须有轮询兜底（Linux inotify watch 数量受限时仍可用） | 必须 |

### REQ-UI 菜单与设置

| 编号 | 需求 | 优先级 |
| --- | --- | --- |
| REQ-UI-01 | 菜单栏**四个平级顶级项**：**File**、**Settings**、**Columns** 与 **About**（结构见 design-doc §6.3） | 必须 |
| REQ-UI-02 | **Settings ▸ Font** 子菜单：分别更改 ①菜单与界面字体 ②日志表格字体 ③日志表头字体；修改即时生效并持久化，提供 Reset to defaults | 必须 |
| REQ-UI-03 | **Settings ▸ Language** 子菜单：English / 简体中文；**即时切换**（无需重启），持久化 | 必须 |
| REQ-UI-04 | **Settings ▸ Details Pane ▸ Layout** 子菜单（Layout 挂在 Details Pane 之下，因为它只影响详情框）：Details pane 位置 = Right / Bottom；详情框关闭时该子菜单置灰 | 必须 |
| REQ-UI-11 | **Settings ▸ Details Pane** 为一个可勾选项 **Show Details Pane**（默认关闭）：☑ 勾选时详情框**始终显示**——打开日志自动选中第一条并展开，点击行仅更新内容，面板不自动隐藏；☐ 取消勾选时详情框**始终不显示**——点击某一行也不会展开，同时**表格进入完整内容模式**（行高按内容自适应、不截断，因为此时表格是唯一的阅读位置）。开启时「Layout ▸ Details: Right/Bottom」可用，关闭时置灰。**详情框只能由该勾选项或「用户显式点击/键盘导航选中某行（仅在勾选时）」触发**：模型重置、刷新、重新打开文件、过滤等程序化选中一律不得自动弹出详情框 | 必须 |
| REQ-UI-12 | **顶级菜单 Columns**（位于 Settings 与 About 之间，结构见 design-doc §6.3）：菜单项**按当前文档的列动态生成**（文本 = 列名，含 extra 列，随文档切换重建；无文档时整个菜单置灰为空）；每项为勾选项：☑ 显示该列、☐ 隐藏该列；Line 列常显（REQ-PARSE-03），其项禁用并悬停提示；另提供 **Show All Columns**（仅当存在隐藏列时可用） | 必须 |
| REQ-UI-05 | **Settings ▸ Appearance** 子菜单：Theme = Light / Dark / Follow system（默认 Light）；Syntax highlighting = Follow theme / VSCode Dark+ / VSCode Light+；Highlight color（Find/Filter 高亮底色与文字色） | 必须 |
| REQ-UI-06 | **顶级菜单 About**（与 File、Settings 平级，点击直接打开关于对话框）：产品名、版本号、构建信息（Qt 版本、编译器、构建时间、**目标平台**）、许可证说明、开源组件致谢 | 必须 |
| REQ-UI-07 | 查询与过滤功能集中在一个主界面**分组框**中，位于日志表格分组框**上方** | 必须 |
| REQ-UI-08 | 所有设置持久化（路径见 CON-11，QSettings INI 格式）；提供"Reset all settings"入口 | 必须 |
| REQ-UI-09 | 状态栏展示：文件名（合并时显示数量）、编码、格式名、总行数/显示行数、Monitor 状态、最近一次操作反馈（如复制提示） | 应该 |
| REQ-UI-10 | 界面在暗色/亮色系统主题、不同 DPI 缩放（100%/125%/150%/200%）下均正常（REQ-PLAT-07） | 必须 |

### REQ-CLIP 剪贴板与单元格交互

| 编号 | 需求 | 优先级 |
| --- | --- | --- |
| REQ-CLIP-01 | **双击任意日志单元格，将该单元格的完整原始文本（非屏幕上被 `…` 截断的文本）复制到系统剪贴板** | 必须 |
| REQ-CLIP-02 | 复制后状态栏短暂提示，例如 `Copied 398 chars from "Message"` | 应该 |
| REQ-CLIP-03 | `Ctrl+C` 复制当前选中的单元格区域/整行，多单元格以制表符分隔、多行以换行分隔 | 应该 |
| REQ-CLIP-04 | 空白单元格双击视为复制空字符串，并给出提示 | 可选 |
| REQ-CLIP-05 | 提供右键菜单：Copy Cell / Copy Row / Copy Message / Auto-fit columns（不包含书签、标记等高级项） | 必须 |

### REQ-CLI 命令行接口

| 编号 | 需求 | 优先级 |
| --- | --- | --- |
| REQ-CLI-01 | 用法：`log-viewer [options] [files...]`；位置参数为要打开的日志文件，多个文件按 REQ-FILE-04 合并规则处理 | 必须 |
| REQ-CLI-02 | `--lang <en\|zh_CN>`：覆盖界面语言（优先于已保存设置，不改写设置） | 必须 |
| REQ-CLI-03 | `--monitor`：启动并打开单个文件时立即开启 Monitor（多文件时忽略并提示） | 应该 |
| REQ-CLI-04 | `--format <id>`：强制使用指定日志格式（`auto` 为默认，`--format list` 可列出可用 id） | 应该 |
| REQ-CLI-05 | `--version` / `--help`：输出版本与用法到标准输出后退出（退出码 0）；Windows 为 GUI 子系统，使用 `AttachConsole` 兼容从控制台启动的场景 | 应该 |
| REQ-CLI-06 | 参数错误时输出用法到标准错误并返回退出码 2；文件不存在时输出错误信息到标准错误（不弹 GUI 对话框） | 应该 |
| REQ-CLI-07 | **不做单实例限制**：允许同时运行多个实例，便于并行查看不同日志；不提供单实例开关 | 必须 |
| REQ-CLI-08 | 命令行打开文件时，界面语言与主题仍遵循设置（`--lang` 除外） | 必须 |
| REQ-CLI-09 | `--demo`：载入内置演示数据（覆盖各日志级别、超长消息、内嵌 JSON/XML/YAML 片段），用于界面自检与截图，不参与正式功能 | 可选 |

### REQ-ASSOC 文件关联（双平台）

| 编号 | 需求 | 优先级 |
| --- | --- | --- |
| REQ-ASSOC-01 | Windows：提供 `scripts\register-association.ps1`，在 **HKCU**（`HKCU\Software\Classes`）注册 `.log`（以及 `.txt` 可选）→ 应用 ProgID，支持 `-Unregister` 完全撤销；**不要求管理员权限，不修改 HKLM，不劫持系统默认关联** | 应该 |
| REQ-ASSOC-02 | Windows：注册项必须支持 `"%1"` 参数（双击文件即打开），并出现在"打开方式"列表中 | 必须 |
| REQ-ASSOC-03 | Linux：提供 `log-viewer.desktop`（`Exec=log-viewer %F`、`Terminal=false`、`MimeType=text/x-log;application/json;text/plain;`）、AppStream metainfo 与 hicolor 主题图标；安装后可通过 `xdg-mime default log-viewer.desktop text/x-log` 设为默认打开方式 | 应该 |
| REQ-ASSOC-04 | 关联行为必须在文档中说明为**用户可选、可撤销**，并在撤销后恢复系统原状 | 必须 |
| REQ-ASSOC-05 | 不做静默修改默认程序、不做系统级强制关联 | 必须 |

### REQ-PLAT 平台与可移植性（Windows / Linux）

| 编号 | 需求 | 优先级 |
| --- | --- | --- |
| REQ-PLAT-01 | 同一套源码可在 Windows 与 Linux x64 上配置、编译、运行；平台差异集中在 `src/platform/`，其余代码不含平台条件编译 | 必须 |
| REQ-PLAT-02 | 本地编码回退实现按平台提供：Windows = `MultiByteToWideChar`（ANSI 代码页，含 CP936/GB18030）；Linux = `iconv`（GB18030/GBK/ISO-8859-1 → UTF-8）；并内置紧凑的 GB18030/GBK 解码表作为**跨平台兜底**（musl、无 iconv 或平台 API 失败时行为一致） | 必须 |
| REQ-PLAT-03 | 配置文件位置按平台规范（CON-11），迁移/复制配置目录即可跨平台搬运设置 | 必须 |
| REQ-PLAT-04 | 默认等宽字体按平台解析：Windows 首选 `Consolas`（回退 `Cascadia Mono` / 系统等宽）；Linux 使用 `QFontDatabase::systemFont(QFontDatabase::FixedFont)`（DejaVu Sans Mono / Noto Sans Mono 等）；界面字体使用系统 UI 字体；CJK 字体回退交由 Qt 处理 | 必须 |
| REQ-PLAT-05 | 路径处理全部使用 Qt API；文件扩展名与格式映射大小写不敏感（`.LOG` 与 `.log` 等价）；不依赖文件名排序规则 | 必须 |
| REQ-PLAT-06 | 高 DPI：在 100%/125%/150%/200% 缩放下布局不错位；默认窗口尺寸按可用屏幕自适应收缩（不低于逻辑 1024×600） | 必须 |
| REQ-PLAT-07 | Wayland 与 X11 均可运行（不调用平台专有图形 API）；如遇环境问题允许通过 `QT_QPA_PLATFORM` 覆盖，属可接受限制 | 应该 |
| REQ-PLAT-08 | 不调用平台专有外部进程（如 `tasklist`/`ps`）；不依赖 shell 特性 | 必须 |
| REQ-PLAT-09 | 版本信息、About 页面与日志输出中标注当前平台与构建类型，便于问题定位 | 应该 |
| REQ-PLAT-10 | Linux 侧代码路径按可移植性要求编写，但 **v1 不做构建与运行验证**；未验证状态必须在文档中明确标注 | 必须 |

### REQ-I18N 国际化

| 编号 | 需求 | 优先级 |
| --- | --- | --- |
| REQ-I18N-01 | 源码界面字符串以英文书写，`.ts` 翻译文件覆盖简体中文，覆盖率 100%（含状态栏、对话框、错误提示、命令行 `--help`） | 必须 |
| REQ-I18N-02 | 语言切换即时生效：已打开的对话框/面板同步重译（`QEvent::LanguageChange` + `retranslateUi`） | 必须 |
| REQ-I18N-03 | 日志内容本身永不翻译；日期时间格式按当前语言的区域设置显示 | 必须 |

## 5. 界面与视觉需求

| 编号 | 需求 |
| --- | --- |
| REQ-VIS-01 | 以 **1920×1080** 分辨率为设计基准；默认窗口尺寸 1600×950 居中显示（可用屏幕不足时按 REQ-PLAT-06 收缩），最小尺寸逻辑 1280×720 |
| REQ-VIS-02 | 遵循人类审美对称原则：内容列**随窗口宽度按比例伸展并铺满全部可用像素**（无固定最大宽度），左右边距对称相等（各 16 px）；窗口最大化时不得出现左右空白带 |
| REQ-VIS-03 | 菜单项**自左开始排列**（Windows 原生风格；Linux 下使用 Qt 原生菜单栏行为），不做菜单栏居中处理 |
| REQ-VIS-04 | 界面分组框整体**居中靠左对齐**（分组框左边界与内容列左边界一致） |
| REQ-VIS-05 | 多个分组框必须**同宽对齐**，禁止宽窄不一；分组框之间垂直间距统一（12 px），内容列外边距 16 px；窗口缩放/最大化时三个分组框同步伸缩，始终保持等宽 |
| REQ-VIS-06 | 日志表格表头：字体加粗 + 阴影类渐变背景 + 单元格内容垂直居中水平左对齐 |
| REQ-VIS-07 | 表格行高默认显示两行内容；超长内容以 `…` 截断 |
| REQ-VIS-08 | 详情面板中字段名加粗显示，字段值常规字重；Message 使用等宽字体 |
| REQ-VIS-09 | 字体家族/字号不得写死：表格与表头字体可通过 Settings 修改；中文字体回退到系统 UI 字体（Windows: Microsoft YaHei；Linux: Noto Sans CJK / 文泉驿 等，由 Qt 字体回退选择） |
| REQ-VIS-10 | 深色主题下所有自定义绘制（表头渐变、级别色块、高亮色、代码片段配色）都必须可读且对比度达标（正文对比度 ≥ 4.5:1） |
| REQ-VIS-11 | 所有图标使用内置 SVG 资源，深浅主题各一套颜色变体；Linux 下同时提供 hicolor 主题 PNG/SVG 应用图标用于桌面集成 |

## 6. 非功能需求

| 编号 | 需求 |
| --- | --- |
| REQ-PERF-01 | 性能预算（参考机：本机 Windows 11 / SSD，Linux 指标同标准但 v1 不验证）：100 万行（约 200 MB）文本日志 → 打开建索引 ≤ 2.5 s、首屏可见 ≤ 100 ms、过滤应用 ≤ 300 ms、滚动 ≥ 55 FPS；1.8 MB 附件样本文件打开 ≤ 200 ms |
| REQ-PERF-02 | 内存预算：100 万行索引 + 10 万条解析缓存 ≤ 250 MB；监控模式受 REQ-MON-06 约束 |
| REQ-PERF-03 | 所有耗时操作（建索引、过滤、查找、导出）在工作线程执行，UI 线程不阻塞；带进度显示与取消能力 |
| REQ-PERF-04 | 打开 > 500 MB 的文件时给出耗时预估提示（可关闭） |
| REQ-REL-01 | 文件被占用/无权限/被删除时给出明确错误提示，且不影响已加载内容 |
| REQ-REL-02 | 不修改、不写回被打开的日志文件（只读）；导出的结果写入用户指定位置 |
| REQ-REL-03 | 异常日志内容（超长行、二进制字节、空文件、无末尾换行）不得导致崩溃 |
| REQ-MAINT-01 | 单元测试覆盖：解析器（每种格式含正/负例）、匹配器、时间解析、片段检测、行索引、监控追加/轮转、命令行参数解析、编码回退 |
| REQ-MAINT-02 | 使用附件真实样本作为回归测试数据：9187 行、DEBUG 9121 / INFO 23 / ERROR 43、最长行 398 字符、170 行含中文（UTF-8 校验）、续行数 0 |
| REQ-MAINT-03 | 提交信息与代码注释英文；界面字符串全部可翻译；不引入未使用依赖 |
| REQ-MAINT-04 | 文档：`README.md`（英文）与 `README.zh_CN.md`（中文），含 Windows 与 Linux 双平台的构建、运行、打包、快捷键、支持的格式与已知限制（含"Linux 路径未经运行验证"声明） |
| REQ-MAINT-05 | 测试代码可移植：使用临时目录与 Qt API，不依赖平台特定路径；测试同时覆盖 LF 与 CRLF 样本 |

## 7. 范围外（Out of Scope）

| 项 | 说明 |
| --- | --- |
| 自定义正则格式编辑器 | v1 不做（已确认），仅内置格式 + 通用启发式兜底 |
| Apache / Nginx 专用解析器 | v1 不做，由通用启发式兜底 |
| 统计面板（级别计数图表、时间分布、Top 模块） | 不做（级别计数在过滤面板中以复选框角标体现） |
| 书签 / 标记行 | 不做 |
| Windows NSIS 安装包 / CPack | 不做，仅交付构建脚本 + `windeployqt` 便携目录 |
| Linux AppImage / Flatpak / Snap | 不做（v1 仅配置 .deb 打包） |
| **Linux 构建与运行验证** | 本开发机不做；用户将在 **Debian Linux 原生环境**自行验证，届时据结果修订 AC-16 / AC-17 |
| Linux 源码包（.dsc/.tar.gz）与 PPA / COPR 仓库 | 不做 |
| macOS 支持 | 不做 |
| 多标签页 / 多窗口 | 不做（单文档合并视图） |
| 远程/网络日志源（syslog 监听、SSH 读取、journalctl 实时订阅） | 不做（仅支持 journalctl 导出文件的离线查看） |
| 日志内容编辑/保存回原文件 | 不做（只读） |
| ICU 依赖 | 默认不引入；GB18030 由平台 API（Windows ANSI 代码页 / Linux iconv）+ 内置紧凑解码表兜底完成 |

## 8. 验收标准

| 编号 | 验收项 | 验证方式 | v1 是否验证 |
| --- | --- | --- | --- |
| AC-01 | 打开附件 `sslocal.2026-09-28.log`（现位于 `test-data/`，不入库）：显示 9187 条，列含 Time/Level/Thread/Target/Message/Line，级别计数与真实值一致，中文消息正确显示 | 手工 + 单元测试断言 | 是 |
| AC-02 | Find 输入 `连接` 或 `tunnel` 命中高亮（绿底黑字）且行数不变；切换忽略大小写/通配符/正则均按预期变化 | 手工 + 单元测试 | 是 |
| AC-03 | Filter 输入 `ERROR` + 勾选 ERROR 级别后仅显示 43 行；叠加时间范围后进一步收敛 | 手工 + 单元测试 | 是 |
| AC-04 | 点击任意行后详情面板出现并完整显示 398 字符长消息；切换到底部布局立即生效 | 手工 | 是 |
| AC-05 | 双击 Message 单元格后可粘贴出**完整**未截断文本，状态栏提示字符数 | 手工 + 单元测试（剪贴板取值路径） | 是 |
| AC-06 | 选中行按 Enter 展开为完整高度，再按 Enter 收起 | 手工 | 是 |
| AC-07 | 消息中嵌入 JSON/XML/YAML 片段时按 VSCode 配色高亮，非片段文本不误报 | 单元测试 + 手工样例 | 是 |
| AC-08 | 拖入 3 个同格式文件合并按时间交错显示且带 File 列；拖入不同格式文件被拒绝并提示 | 手工 | 是 |
| AC-09 | 单文件勾选 Monitor 后用外部程序追加日志，表格自动出现新行并遵循智能跟随 | 手工 + 集成测试（临时文件模拟） | 是 |
| AC-10 | Settings ▸ Language 切换为简体中文后界面即时变中文，重启后保持 | 手工 | 是 |
| AC-11 | 三项字体（界面/表格/表头）修改即时生效并持久化 | 手工 | 是 |
| AC-12 | 100 万行合成日志达到 REQ-PERF-01 指标 | 基准测试脚本 | 是 |
| AC-13 | `ctest` 全部通过；`scripts\package.ps1` 产出可双击运行的 `dist\log-viewer.exe` | 命令行验证 | 是 |
| AC-14 | `log-viewer --help` / `--version` 输出正确且退出码为 0；`log-viewer <file>` 直接打开；`--lang zh_CN` 生效；参数错误退出码为 2 | 命令行验证 + 单元测试 | 是 |
| AC-15 | `scripts\register-association.ps1` 注册后双击 `.log` 用本应用打开；`-Unregister` 后 `HKCU\Software\Classes` 相关键完全移除 | 手工 + 注册表核对 | 是 |
| AC-16 | 源码在 Linux（Debian 13 / GCC 14 / Qt 6.8.2）下 `cmake` 配置与编译一次通过 | 命令行验证 | **否（本机不验证；由用户在 Debian 原生环境验证）** |
| AC-17 | `cpack -G DEB` 产出 `.deb`，安装后 `log-viewer`、`.desktop`、MIME、AppStream、翻译文件就位 | 命令行验证 | **否（本机不验证；由用户在 Debian 原生环境验证）** |
| AC-18 | CPack 配置与 `packaging/linux/` 资源文件存在且语法正确（`cmake --install --dry-run` 级别的静态检查） | 静态检查 | 是（静态） |

## 9. 开放问题与建议项

| 编号 | 问题 | 建议 | 状态 |
| --- | --- | --- | --- |
| OPEN-01 | 右键菜单是否需要？ | **已确认：需要**。保留实现（REQ-CLIP-05）：Copy Cell / Copy Row / Copy Message / Auto-fit Columns | 已决定 |
| OPEN-02 | 是否显示 Line 列？ | **已确认：始终显示**（REQ-PARSE-03 提升为必须） | 已决定 |
| OPEN-03 | 是否需要"最近文件"列表？ | **已确认：需要**（REQ-FILE-07 提升为必须） | 已决定 |
| OPEN-04 | GB18030 解码在非中文区域设置的系统上可能失败 | **已确认：按现计划**（Windows 系统 ANSI 代码页，必要时显式 CP936；Linux iconv GB18030），并增加内置解码表兜底（见 OPEN-09） | 已决定 |
| OPEN-05 | 监控模式保留行数上限 | **已确认：取消独立上限**。监控先完整展示打开时的全部内容，随后把新内容全部追加显示；容量约束统一沿用全局限制（`general.maxLinesPerFile`，默认 200 万行） | 已决定（REQ-MON-06 已改写） |
| OPEN-06 | 正则性能保护 | **已确认：同意**（模式长度 ≤ 512 + 可取消工作线程 + UI 性能提示） | 已决定 |
| OPEN-07 | **Linux 构建与运行验证** | **已确认：本开发机不验证**；用户将在 Debian Linux 原生环境验证，届时据结果修订 AC-16/AC-17 | 已决定 |
| OPEN-08 | Linux 构建依赖清单 | **已确认：按现设计**：`build-essential cmake ninja-build qt6-base-dev qt6-base-dev-tools qt6-l10n-tools libgl1-mesa-dev` | 已决定（待用户实测） |
| OPEN-09 | musl 环境（Alpine）不支持 GB18030 | **已探索并采纳建议**：内置一份紧凑的 GB18030/GBK 解码表（公开映射数据生成，约数十 KB）作为跨平台兜底；候选编码按 GB18030 → Big5 → Shift_JIS → CP1252 做有效性校验。评估过的备选：ICU（体积大，否决）、Qt5Compat QTextCodec（需额外模块且无 ICU 时能力受限，否决） | 已决定（采纳建议） |
| OPEN-10 | 单实例运行 | **已确认：不做单实例**。允许同时运行多个实例以查看不同日志（REQ-CLI-07 已改写） | 已决定 |
| OPEN-11 | Windows 是否注册 `.txt` 关联 | **已确认：默认只注册 `.log`**；`.txt` 需显式参数 `-IncludeTxt` | 已决定 |
| OPEN-12 | 下载依赖（Qt / pip）需要 HTTP 代理 | 本机代理为 `http://localhost:1081`；`scripts/install-qt.ps1` 提供 `-Proxy` 参数（默认取 `HTTPS_PROXY` 环境变量），脚本内同时设置 `HTTP_PROXY/HTTPS_PROXY/ALL_PROXY` | 已决定（环境事实） |

## 10. 修订记录

| 版本 | 日期 | 修改人 | 说明 |
| --- | --- | --- | --- |
| 1.0 | 2026-09-29 | AI 助手（依据用户需求整理） | 初稿：汇总全部已确认需求与决策；新增 REQ-CLIP-01 双击单元格复制；Enter 展开行内内容 |
| 1.1 | 2026-09-29 | AI 助手（依据用户新增要求） | 增加 Linux 平台支持：平台矩阵（§1.3）、可移植性约束（CON-9…CON-12）、REQ-PLAT 域（10 条）、REQ-CLI 域（8 条）、REQ-ASSOC 域（5 条）、编码/文件监视/字体/高 DPI 的平台化要求、双平台打包（.deb）与文件关联、AC-14…AC-18、范围外条目调整、OPEN-07…OPEN-11 |
| 1.2 | 2026-09-29 | AI 助手（依据用户对 OPEN 项的批复） | 落定全部开放问题：右键菜单必需（OPEN-01）、Line 列常显（OPEN-02）、最近文件必需（OPEN-03）；监控改为完整展示 + 全量追加，取消独立保留上限（OPEN-05，REQ-MON-06 改写）；不做单实例、允许多实例（OPEN-10，REQ-CLI-07 改写）；GB18030 增加内置紧凑解码表兜底并记录 ICU/Qt5Compat 备选评估（OPEN-09，REQ-PLAT-02 更新）；Linux 验证改由用户在 Debian 原生环境执行（OPEN-07）；新增 OPEN-12 代理要求 |
| 1.3 | 2026-09-29 | AI 助手（依据用户反馈） | 行号列（Line）调整为表格第一列，符合自上而下、自左而右的阅读习惯；同步更新 REQ-PARSE-03 与列生成规则 |
| 1.4 | 2026-09-29 | AI 助手（依据用户反馈） | 取消内容列 1600 px 最大宽度：内容列随窗口按比例伸展并铺满可用像素，最大化时不再出现左右空白；同步更新 REQ-VIS-02 / REQ-VIS-05 |
| 1.5 | 2026-09-29 | AI 助手（依据用户反馈） | 新增 Settings ▸ Details Pane 子菜单（REQ-UI-11）：Show at Startup 控制打开日志时自动选中第一行并展开详情框；Show on Row Click 关闭时点击行不显示详情框，表格改为完整内容模式（行高自适应、不截断）。同步更新 REQ-TABLE-04 / REQ-DETAIL-01 |
| 1.6 | 2026-09-29 | AI 助手（缺陷修复） | 修复「启动时显示未勾选时打开日志仍自动弹出详情框」：详情框改为只由显式用户激活（鼠标点击/键盘导航）或 Show at Startup 打开，新增 LogTableView::rowActivatedByUser 信号；REQ-UI-11 补充该语义 |
| 1.7 | 2026-09-29 | AI 助手（依据用户反馈） | Settings ▸ Details Pane 由两个勾选项简化为单个勾选项 **Always Show Details**（勾选=始终显示，取消=点击行时显示）；撤销上一版引入的「完整内容模式」相关需求表述（REQ-TABLE-04 回退为两行截断 + Enter 展开）；旧设置键 detailsOnStartup/detailsOnRowClick 自动迁移 |
| 1.8 | 2026-09-29 | AI 助手（依据用户反馈） | 勾选项更名为 **Show Details Pane**，语义改为开关式：勾选=始终显示，取消=始终不显示（点击行也不再展开）；开启时 Layout 菜单可用、关闭时置灰；设置键改为 view/showDetailsPane 并兼容迁移 alwaysShowDetails / detailsOnStartup / detailsOnRowClick |
| 1.9 | 2026-09-29 | AI 助手（依据用户反馈） | 详情框消息区改为始终自动折行（WidgetWidth + WrapAtWordBoundaryOrAnywhere），彻底移除横向滚动条；REQ-DETAIL-04 升级为必须并删除 Wrap 开关要求 |
| 1.10 | 2026-09-29 | AI 助手（缺陷修复） | 实现 Monitor（tail -f）：此前菜单项为占位禁用状态，现支持增量索引追加、智能跟随（底部自动滚动，上滚暂停并显示「新行 N ▼」按钮）、轮转/截断自动重载、级别计数实时刷新、Ctrl+M 快捷键；REQ-MON-01 补充 demo/快捷键约束；测试样本改为「冻结切片 + 活文件结构断言」以适配持续增长的日志文件 |
| 1.11 | 2026-09-29 | AI 助手 | 实现 M3：Find / Filter 引擎（全词、通配符、正则 + 大小写开关；级别、时间、关键词过滤；命中导航与计数；导出过滤结果）与 VSCode 语法高亮（JSON/XML/YAML 片段 token 化、Dark+ / Light+ 配色、关键词高亮覆盖）；新增 Settings ▸ Appearance ▸ Syntax Highlighting 三个选项 |
| 1.12 | 2026-09-29 | AI 助手 | 完成剩余待办：M2（其余 10 类解析器、块式条目与续行合并、编码后端与候选编码检测、列宽记忆）、M4（多文件按时间合并 + File 列 + 格式一致性校验）、M6（package.ps1 便携目录、register-association 双平台脚本、Linux .desktop/AppStream/图标/CPack DEB 与脚本、README.md 与 README.zh_CN.md）；测试增至 13 个目标 |
| 1.13 | 2026-09-29 | AI 助手 | 依据用户提供的真实测试文件补充两种 Windows 事件导出格式：事件查看器制表符文本导出（多行消息自动合并、本地化级别名如「信息/警告/错误」）与 XML 导出（按 <Event> 建立条目，支持一行多事件）；FMT-5 行同步更新 |
| 1.14 | 2026-09-29 | AI 助手（依据用户反馈） | 关于(About) 从 Settings 子项调整为与 File、Settings 平级的顶级菜单项（点击直接打开对话框）；REQ-UI-01 与 REQ-UI-06 同步更新 |
| 1.15 | 2026-09-29 | AI 助手（依据用户反馈） | Layout 子菜单从 Settings 顶层移入 **Details Pane ▸ Layout**（语义上只影响详情框）；详情框关闭时整个 Layout 子菜单置灰；REQ-UI-04 同步更新 |
| 1.16 | 2026-09-29 | AI 助手（缺陷修复） | 修复「打开详情框后表格高亮消失」：被 … 截断的末行原先用无格式的 drawText 重绘，导致该行的高亮丢失（详情框打开→消息列变窄→更多内容落在截断行）；现改为将格式区间映射到省略后的文本再绘制；REQ-HL-05 补充该约束 |
| 1.17 | 2026-09-29 | AI 助手（缺陷修复） | 补实现拖拽打开文件（REQ-FILE-02 此前只写了需求，代码缺失）：主窗口 setAcceptDrops(true) + dragEnter/dragMove/dropEvent，查找/过滤输入框、表格、详情消息区显式关闭自身拖放以免吞掉事件；文件夹被忽略并提示；新增两条 UI 用例（拖入两个同格式文件按合并打开、拖入文件夹被忽略） |
| 1.18 | 2026-09-29 | AI 助手（依据用户反馈） | 过滤改为**回车 / Apply 提交**后执行，输入过程中不再实时刷新表格；Find 同步改为回车（或 F3 / 导航按钮）提交，输入时不做全量扫描；清空输入框立即生效；REQ-FILTER-01 与 REQ-FIND-07 同步更新，新增 UI 用例 ilterAppliesOnEnterOnly |
| 1.19 | 2026-09-29 | AI 助手（缺陷修复） | 修复「不显示详情框时消息仍被截断」：将完整内容模式与 Show Details Pane 开关绑定（关闭详情框即启用，行高按内容自适应）；同时修复多段消息的绘制缺陷——段落内行位置误用全局行号导致文字逐段下移并被裁掉，现改为段内相对坐标；REQ-TABLE-04 恢复例外条款、REQ-UI-11 补充说明，新增 UI 用例 uncheckedPaneShowsCompleteRows |
| 1.20 | 2026-09-29 | AI 助手（缺陷修复） | 修复「首次打开 test-event-logs.xml 时行显示不完整，打开其它日志后正常」：行高缓存/展开状态的行键原先用（源序号 + 起始物理行），而 XML 导出中所有事件共用同一物理行 → 键冲突使 3620 行共用同一高度（实测 513 行被截断）；改为「源序号 + 源内行号」保证唯一；并在行高计算中固定单次列宽快照、不再丢弃进行中的重算请求；REQ-TABLE-06 补充标识唯一性要求 |
| 1.21 | 2026-09-29 | AI 助手（依据用户反馈） | 级别过滤列表改为**按文档动态生成**（此前固定 8 项、把 0 计数的级别置灰）：只显示实际出现的级别，切换文档时重建；用户取消勾选的级别跨文档保持；实时监控中若可见级别集合不变仅刷新计数、出现新级别才重建控件；REQ-FILTER-02 同步更新 |
| 1.22 | 2026-09-29 | AI 助手（缺陷修复） | 修复「test-event-logs.xml 所有行号都是 2、test-event-logs.txt 行号从 2 开始」：Line 列原先固定显示条目首个物理行号，而事件日志导出的物理行无法标识记录（XML 导出整份文档写在 1 个物理行内，TSV 第 1 行是表头）；现对 `wevt_xml` / `wevt_tsv` 改显**源内条目序号 1..N**（新增 `ILogFormat::numbersEntriesSequentially()`，由 LogSource 统一编号），其余格式仍显示物理行号；REQ-PARSE-03 同步更新 |
| 1.23 | 2026-09-29 | AI 助手（依据用户反馈） | 表格可读性：REQ-TABLE-09「斑马纹」由**可选**升为**必须**并明确按显示行序交替、不覆盖选中色；新增 REQ-TABLE-10 单元格分隔线（每格右缘竖线 + 下缘横线，与表头分隔线同色）；新增 UI 用例 zebraStripesAndGridLines |
| 1.24 | 2026-09-29 | AI 助手（依据用户反馈） | 新增顶级菜单 **Columns**（位于 Settings 与 About 之间，REQ-UI-12）：按当前文档的列动态生成勾选项，取消勾选即隐藏该列，Line 列常显（禁用项），另有 Show All Columns；新增 REQ-TABLE-11（列显隐语义：隐藏列不参与过滤/查找/导出、按文档签名与列宽一同记忆、消息列隐藏时行高退回默认）；REQ-UI-01 改为四个顶级菜单、REQ-TABLE-05 补充列显隐记忆；新增 UI 用例 columnsMenuHidesColumns |
| 1.25 | 2026-09-29 | AI 助手（依据用户反馈） | 查找与过滤 ▸ 级别：复选项右侧**不再显示计数**（列表更紧凑、复选项间距加大；各级别条目数改为悬停提示），REQ-FILTER-02 同步更新 |
| 1.26 | 2026-09-29 | AI 助手（缺陷修复） | 修复「Auto-fit Columns 只是把消息列拉长、其余列宽都一样」：`LogItemDelegate::sizeHint()` 原先返回 `option.rect.width()`（即当前单元格宽度），导致 `resizeColumnToContents()`（右键菜单自适应与双击列边界共用）把每列压到同一最小值；现按单元格完整文本 + 内边距（级别列额外加 chip 内边距、粗体）计算内容宽度并上限 2000 px，长行只测前 512 字符；REQ-TABLE-05 明确按内容适配与 60–600 px 限制；新增 UI 用例 autoFitColumnsFitsContent |
| 1.27 | 2026-09-29 | AI 助手（依据用户反馈） | 重写列宽自适应算法（REQ-TABLE-05）：**时间列始终完整显示时间戳**（内容宽度与 215 px 取大，绝不换行）、**消息列优先**拿走剩余宽度（≥ 40%、≥ 240 px）、其余列在剩余宽度内按内容长度比例缩放（充裕时按内容、不足时等比缩小，下限 24 px，单列上限 600 px）；修复紧空间分支未排除已预留时间列、把时间戳再次缩小的缺陷；`autoFitColumns()` 改为公开方法；`tst_mainwindow_behavior` 用例扩展到宽/窄两种窗口并断言时间戳不换行 |
| 1.28 | 2026-09-29 | AI 助手（依据用户反馈） | 时间列不再为凑 215 px 最小宽度留白：自适应改为**严格按内容宽度**（内容 + 2 px 取整余量；表头标题更宽时以表头为准；下限 36 px、上限 600 px），所有列一视同仁；实测 `sslocal` 日志中时间列 215 → 164 px、消息列 951 → 1007 px；REQ-TABLE-05 同步更新；用例新增"不留多余空白"上界断言 |
| 1.29 | 2026-09-29 | AI 助手（依据用户反馈） | 级别列也需完整显示：① 覆盖 `LogTableView::sizeHintForColumn()`，内容宽度改为**整篇文档采样**（新增 `contentWidthHint()`：前 500 行连续 + 其后均匀抽样至 3500 行）——Qt 自带实现只看向前部窗口，曾把级别列按 INFO 拟合（64 px）而裁掉 DEBUG 的 chip；② 级别列与时间列一样在预算前**预留**、不参与比例缩放（窄窗口下 chip 不再被压缩）；实测 `sslocal`：级别列 64 → 66 px（= DEBUG/ERROR 完整宽度），时间/消息列不受影响；REQ-TABLE-05 同步更新；用例新增级别 chip 完整性与宽度上界断言 |
| 1.30 | 2026-09-29 | AI 助手（依据用户反馈） | 新增**反向过滤**（REQ-FILTER-09）：Filter 行新增 **Invert** 勾选框，勾选后只显示不包含过滤关键词/模式的行（可与级别、时间条件 AND 叠加），切换立即生效，输入为空时不隐藏任何行，Clear 同时取消勾选；`FilterSpec` 新增 `invertKeyword`，模型在 `entryMatches()` 中统一取反；新增 UI 用例 invertedFilterShowsNonMatchingRows |
| 1.31 | 2026-09-29 | AI 助手（依据用户指示） | 测试用样本日志移动到新目录 **`test-data/`**（`sslocal.2026-09-27.log`、`sslocal.2026-09-28.log`、`test-event-logs.txt`、`test-event-logs.xml`）并加入 `.gitignore`（不入库）；`tests/CMakeLists.txt` 的 `LOGVIEWER_SAMPLE_LOG` 指向新路径（仍以 EXISTS 守卫，缺失时相关用例自动跳过）；冻结样本 `tests/data/tracing-sample.log` 保留在版本库中；README / run.ps1 示例路径同步更新 |
