<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="zh_CN">
<context>
    <name>CliParser</name>
    <message>
        <location filename="../app/CliParser.cpp" line="46"/>
        <source>Unknown option: %1</source>
        <translation>未知选项：%1</translation>
    </message>
    <message>
        <location filename="../app/CliParser.cpp" line="56"/>
        <source>Option --lang requires a value (en or zh_CN).</source>
        <translation>选项 --lang 需要取值（en 或 zh_CN）。</translation>
    </message>
    <message>
        <location filename="../app/CliParser.cpp" line="57"/>
        <source>Option --format requires a value (a format id or &apos;auto&apos;).</source>
        <translation>选项 --format 需要取值（格式 id 或 auto）。</translation>
    </message>
    <message>
        <location filename="../app/CliParser.cpp" line="63"/>
        <source>Unsupported language &apos;%1&apos; (supported: en, zh_CN).</source>
        <translation>不支持的语言“%1”（支持：en、zh_CN）。</translation>
    </message>
    <message>
        <location filename="../app/CliParser.cpp" line="70"/>
        <source>Usage: log-viewer [options] [files...]

Opens one or more log files. Files given at the same time are merged
into a single view when they share the same format.

Options:
  -h, --help             Show this help and exit
  -v, --version          Show version and build information and exit
      --lang &lt;en|zh_CN&gt;  Override the interface language
      --format &lt;id&gt;      Force a log format (&apos;auto&apos; for detection,
                         &apos;list&apos; to print the available ids)
      --monitor          Enable live monitoring after opening one file
      --demo             Load built-in demo data (UI preview only)
      --register-association
                         Associate .log files with this executable
                         (current user only; undo with the option below)
      --unregister-association
                         Remove the .log association and restore the
                         previous one
      --force            Replace an existing .log association without
                         warning (--register-association only)
</source>
        <translation>用法：log-viewer [选项] [日志文件...]

打开一个或多个日志文件。格式相同的多个文件会合并到同一个视图中显示。

选项：
  -h, --help             显示本帮助并退出
  -v, --version          显示版本与构建信息并退出
      --lang &lt;en|zh_CN&gt;  覆盖界面语言
      --format &lt;id&gt;      强制指定日志格式（auto 为自动探测，
                         list 列出可用的格式 id）
      --monitor          打开单个文件后立即启用实时监控
      --demo             载入内置演示数据（仅用于界面预览）
      --register-association
                         把 .log 文件关联到本程序的当前用户
                         （仅当前用户；用下一条选项撤销）
      --unregister-association
                         移除 .log 关联并还原原有的关联
      --force            覆盖已有的 .log 关联且不再提示
                         （仅与 --register-association 同用）
</translation>
    </message>
    <message>
        <location filename="../app/CliParser.cpp" line="99"/>
        <source>Available log formats:</source>
        <translation>可用的日志格式：</translation>
    </message>
    <message>
        <location filename="../app/CliParser.cpp"/>
        <source>Options --register-association and --unregister-association cannot be combined.</source>
        <translation>选项 --register-association 与 --unregister-association 不能同时使用。</translation>
    </message>
    <message>
        <location filename="../app/CliParser.cpp"/>
        <source>Option --force is only valid together with --register-association.</source>
        <translation>选项 --force 只能与 --register-association 一起使用。</translation>
    </message>
    <message>
        <location filename="../app/CliParser.cpp"/>
        <source>Options --register-association/--unregister-association cannot be combined with file arguments.</source>
        <translation>选项 --register-association/--unregister-association 不能与文件参数同时使用。</translation>
    </message>
    <message>
        <location filename="../app/CliParser.cpp"/>
        <source>%1 files now open with:
%2
Remove the association with --unregister-association.</source>
        <translation>%1 文件现在用以下程序打开：
%2
可用 --unregister-association 取消关联。</translation>
    </message>
    <message>
        <location filename="../app/CliParser.cpp"/>
        <source>The %1 association was removed and the previous one restored.</source>
        <translation>已移除 %1 关联并还原原有的关联。</translation>
    </message>
</context>
<context>
    <name>Columns</name>
    <message>
        <location filename="../core/LogTableModel.cpp" line="18"/>
        <source>Time</source>
        <translation>时间</translation>
    </message>
    <message>
        <location filename="../core/LogTableModel.cpp" line="19"/>
        <source>Level</source>
        <translation>级别</translation>
    </message>
    <message>
        <location filename="../core/LogTableModel.cpp" line="20"/>
        <source>Thread</source>
        <translation>线程</translation>
    </message>
    <message>
        <location filename="../core/LogTableModel.cpp" line="21"/>
        <source>Target</source>
        <translation>目标</translation>
    </message>
    <message>
        <location filename="../core/LogTableModel.cpp" line="22"/>
        <source>PID</source>
        <translation>PID</translation>
    </message>
    <message>
        <location filename="../core/LogTableModel.cpp" line="23"/>
        <source>Host</source>
        <translation>主机</translation>
    </message>
    <message>
        <location filename="../core/LogTableModel.cpp" line="24"/>
        <source>File</source>
        <translation>文件</translation>
    </message>
    <message>
        <location filename="../core/LogTableModel.cpp" line="25"/>
        <source>Line</source>
        <translation>行号</translation>
    </message>
    <message>
        <location filename="../core/LogTableModel.cpp" line="26"/>
        <source>Message</source>
        <translation>消息</translation>
    </message>
</context>
<context>
    <name>DemoData</name>
    <message>
        <location filename="../ui/DemoData.cpp" line="147"/>
        <source>demo-preview.log</source>
        <translation>演示预览.log</translation>
    </message>
    <message>
        <location filename="../ui/DemoData.cpp" line="150"/>
        <source>Demo data</source>
        <translation>演示数据</translation>
    </message>
</context>
<context>
    <name>DurationFormat</name>
    <message>
        <location filename="../core/DurationFormat.cpp" line="28"/>
        <source>%1 s</source>
        <translation>%1 秒</translation>
    </message>
    <message>
        <location filename="../core/DurationFormat.cpp" line="37"/>
        <source>%1 h %2 min %3 s</source>
        <translation>%1 时 %2 分 %3 秒</translation>
    </message>
    <message>
        <location filename="../core/DurationFormat.cpp" line="39"/>
        <source>%1 h %2 min</source>
        <translation>%1 时 %2 分</translation>
    </message>
    <message>
        <location filename="../core/DurationFormat.cpp" line="41"/>
        <source>%1 h %2 s</source>
        <translation>%1 时 %2 秒</translation>
    </message>
    <message>
        <location filename="../core/DurationFormat.cpp" line="42"/>
        <source>%1 h</source>
        <translation>%1 时</translation>
    </message>
    <message>
        <location filename="../core/DurationFormat.cpp" line="45"/>
        <source>%1 min %2 s</source>
        <translation>%1 分 %2 秒</translation>
    </message>
    <message>
        <location filename="../core/DurationFormat.cpp" line="46"/>
        <source>%1 min</source>
        <translation>%1 分</translation>
    </message>
</context>
<context>
    <name>FileAssociation</name>
    <message>
        <location filename="../platform/FileAssociation.cpp"/>
        <source>Windows registry access failed (error %1).</source>
        <translation>访问 Windows 注册表失败（错误 %1）。</translation>
    </message>
    <message>
        <location filename="../platform/FileAssociation.cpp"/>
        <source>File association is only available on Windows. On Linux use &quot;xdg-mime default log-viewer.desktop text/x-log&quot; instead.</source>
        <translation>文件关联仅在 Windows 上可用。Linux 下请改用 &quot;xdg-mime default log-viewer.desktop text/x-log&quot;。</translation>
    </message>
    <message>
        <location filename="../platform/FileAssociation.cpp"/>
        <source>&apos;%1&apos; is not a file extension.</source>
        <translation>“%1”不是有效的文件扩展名。</translation>
    </message>
    <message>
        <location filename="../platform/FileAssociation.cpp"/>
        <source>Executable not found: %1</source>
        <translation>找不到可执行文件：%1</translation>
    </message>
    <message>
        <location filename="../platform/FileAssociation.cpp"/>
        <source>%1 currently opens with %2. That value was backed up and is restored when the association is removed.</source>
        <translation>%1 目前由 %2 打开。该值已被备份，取消关联时会自动还原。</translation>
    </message>
    <message>
        <location filename="../platform/FileAssociation.cpp"/>
        <source>Log file</source>
        <translation>日志文件</translation>
    </message>
    <message>
        <location filename="../platform/FileAssociation.cpp"/>
        <source>Log File</source>
        <translation>日志文件</translation>
    </message>
    <message>
        <location filename="../platform/FileAssociation.cpp"/>
        <source>Log Viewer</source>
        <translation>Log Viewer</translation>
    </message>
</context>
<context>
    <name>Formats</name>
    <message>
        <location filename="../core/formats/GenericFormat.cpp" line="46"/>
        <source>Generic text log</source>
        <translation>通用文本日志</translation>
    </message>
    <message>
        <location filename="../core/formats/TracingFormat.cpp" line="29"/>
        <source>Rust tracing</source>
        <translation>Rust tracing</translation>
    </message>
</context>
<context>
    <name>ElevatedFileReader</name>
    <message>
        <source>The system authentication helper (pkexec) is not available. Grant read access manually (for example add your user to the &quot;adm&quot; group) or copy the file to a readable location.</source>
        <translation>系统认证助手（pkexec）不可用。请手工授予读取权限（例如把当前用户加入 &quot;adm&quot; 组），或把文件复制到可读取的位置。</translation>
    </message>
    <message>
        <source>Reading files with elevated privileges is only available on Linux.</source>
        <translation>以管理员权限读取文件仅在 Linux 上可用。</translation>
    </message>
    <message>
        <source>Authentication was cancelled or failed.</source>
        <translation>认证被取消或失败。</translation>
    </message>
    <message>
        <source>Not authorized to read '%1' with elevated privileges.</source>
        <translation>未获授权以管理员权限读取“%1”。</translation>
    </message>
    <message>
        <source>The authentication helper failed (exit code %1).</source>
        <translation>认证助手执行失败（退出码 %1）。</translation>
    </message>
    <message>
        <source>Not a regular file: %1</source>
        <translation>不是常规文件：%1</translation>
    </message>
    <message>
        <source>Cannot create the private snapshot directory: %1</source>
        <translation>无法创建私有快照目录：%1</translation>
    </message>
    <message>
        <source>Cannot create the temporary snapshot: %1</source>
        <translation>无法创建临时快照：%1</translation>
    </message>
    <message>
        <source>Cannot start the system authentication helper: %1</source>
        <translation>无法启动系统认证助手：%1</translation>
    </message>
    <message>
        <source>Cannot write the temporary snapshot: %1</source>
        <translation>无法写入临时快照：%1</translation>
    </message>
</context>
<context>
    <name>LogSource</name>
    <message>
        <location filename="../core/LogSource.cpp" line="29"/>
        <source>File does not exist: %1</source>
        <translation>文件不存在：%1</translation>
    </message>
    <message>
        <location filename="../core/LogSource.cpp" line="54"/>
        <source>Permission denied: cannot read '%1'</source>
        <translation>权限不足，无法读取：%1</translation>
    </message>
    <message>
        <location filename="../core/LogSource.cpp" line="59"/>
        <source>Unknown log format: %1</source>
        <translation>未知日志格式：%1</translation>
    </message>
</context>
<context>
    <name>Matcher</name>
    <message>
        <location filename="../core/Matcher.cpp" line="49"/>
        <source>The pattern is limited to %1 characters.</source>
        <translation>模式长度上限为 %1 个字符。</translation>
    </message>
    <message>
        <location filename="../core/Matcher.cpp" line="84"/>
        <source>Wildcard</source>
        <translation>通配符</translation>
    </message>
    <message>
        <location filename="../core/Matcher.cpp" line="86"/>
        <source>Regular expression</source>
        <translation>正则表达式</translation>
    </message>
    <message>
        <location filename="../core/Matcher.cpp" line="90"/>
        <source>Whole word</source>
        <translation>全词匹配</translation>
    </message>
</context>
<context>
    <name>lv::AboutDialog</name>
    <message>
        <location filename="../ui/AboutDialog.cpp" line="69"/>
        <source>About Log Viewer</source>
        <translation>关于 Log Viewer</translation>
    </message>
    <message>
        <location filename="../ui/AboutDialog.cpp" line="70"/>
        <source>Log Viewer</source>
        <translation>Log Viewer</translation>
    </message>
    <message>
        <location filename="../ui/AboutDialog.cpp" line="71"/>
        <source>Version %1</source>
        <translation>版本 %1</translation>
    </message>
    <message>
        <location filename="../ui/AboutDialog.cpp" line="72"/>
        <source>Build: %1</source>
        <translation>构建：%1</translation>
    </message>
    <message>
        <location filename="../ui/AboutDialog.cpp" line="74"/>
        <source>A fast log file viewer for Windows and Linux with table based reading, filtering by level, time and keyword, syntax highlighting for embedded JSON, XML and YAML snippets, and live monitoring (tail -f).</source>
        <translation>一款面向 Windows 与 Linux 的快速日志查看器：以表格方式阅读日志，支持按级别、时间和关键词过滤，对消息中内嵌的 JSON、XML、YAML 片段进行语法高亮，并支持实时监控（tail -f）。</translation>
    </message>
    <message>
        <location filename="../ui/AboutDialog.cpp" line="78"/>
        <source>Licensed under the MIT License. Built with Qt %1, used under the terms of the GNU Lesser General Public License v3.</source>
        <translation>本项目以 MIT 许可证发布。基于 Qt %1 构建，Qt 依据 GNU 宽通用公共许可证第 3 版使用。</translation>
    </message>
    <message>
        <location filename="../ui/AboutDialog.cpp" line="81"/>
        <source>Close</source>
        <translation>关闭</translation>
    </message>
</context>
<context>
    <name>lv::DetailPane</name>
    <message>
        <location filename="../ui/DetailPane.cpp" line="99"/>
        <source>Details</source>
        <translation>详情</translation>
    </message>
    <message>
        <location filename="../ui/DetailPane.cpp" line="100"/>
        <source>Message</source>
        <translation>消息</translation>
    </message>
    <message>
        <location filename="../ui/DetailPane.cpp" line="101"/>
        <source>Copy Message</source>
        <translation>复制消息</translation>
    </message>
    <message>
        <location filename="../ui/DetailPane.cpp" line="102"/>
        <source>Copy All Fields</source>
        <translation>复制全部字段</translation>
    </message>
    <message>
        <location filename="../ui/DetailPane.cpp" line="164"/>
        <source>Time</source>
        <translation>时间</translation>
    </message>
    <message>
        <location filename="../ui/DetailPane.cpp" line="169"/>
        <source>Level</source>
        <translation>级别</translation>
    </message>
    <message>
        <location filename="../ui/DetailPane.cpp" line="172"/>
        <source>Thread</source>
        <translation>线程</translation>
    </message>
    <message>
        <location filename="../ui/DetailPane.cpp" line="174"/>
        <source>Target</source>
        <translation>目标</translation>
    </message>
    <message>
        <location filename="../ui/DetailPane.cpp" line="176"/>
        <source>PID</source>
        <translation>PID</translation>
    </message>
    <message>
        <location filename="../ui/DetailPane.cpp" line="178"/>
        <source>Host</source>
        <translation>主机</translation>
    </message>
    <message>
        <location filename="../ui/DetailPane.cpp" line="181"/>
        <source>File</source>
        <translation>文件</translation>
    </message>
    <message>
        <location filename="../ui/DetailPane.cpp" line="182"/>
        <source>Line</source>
        <translation>行号</translation>
    </message>
    <message>
        <location filename="../ui/DetailPane.cpp" line="184"/>
        <source>Lines</source>
        <translation>行数</translation>
    </message>
</context>
<context>
    <name>lv::FilterPanel</name>
    <message>
        <location filename="../ui/FilterPanel.cpp" line="196"/>
        <source>Search &amp;&amp; Filter</source>
        <translation>查找与过滤</translation>
    </message>
    <message>
        <location filename="../ui/FilterPanel.cpp" line="197"/>
        <source>Find:</source>
        <translation>查找：</translation>
    </message>
    <message>
        <location filename="../ui/FilterPanel.cpp" line="198"/>
        <source>Filter:</source>
        <translation>过滤：</translation>
    </message>
    <message>
        <location filename="../ui/FilterPanel.cpp" line="199"/>
        <source>Levels:</source>
        <translation>级别：</translation>
    </message>
    <message>
        <location filename="../ui/FilterPanel.cpp" line="200"/>
        <source>Time:</source>
        <translation>时间：</translation>
    </message>
    <message>
        <location filename="../ui/FilterPanel.cpp" line="202"/>
        <source>Highlight matches (rows stay visible)</source>
        <translation>高亮匹配项（不隐藏任何行）</translation>
    </message>
    <message>
        <location filename="../ui/FilterPanel.cpp" line="203"/>
        <source>Hide rows that do not match</source>
        <translation>隐藏不匹配的行</translation>
    </message>
    <message>
        <location filename="../ui/FilterPanel.cpp" line="204"/>
        <source>Matches are highlighted; no row is hidden</source>
        <translation>匹配项会被高亮显示，不会隐藏任何行</translation>
    </message>
    <message>
        <location filename="../ui/FilterPanel.cpp" line="205"/>
        <source>Rows without a match are hidden</source>
        <translation>不匹配的行会被隐藏</translation>
    </message>
    <message>
        <location filename="../ui/FilterPanel.cpp" line="211"/>
        <source>Whole word</source>
        <translation>全词匹配</translation>
    </message>
    <message>
        <location filename="../ui/FilterPanel.cpp" line="211"/>
        <source>Wildcard</source>
        <translation>通配符</translation>
    </message>
    <message>
        <location filename="../ui/FilterPanel.cpp" line="211"/>
        <source>Regular expression</source>
        <translation>正则表达式</translation>
    </message>
    <message>
        <location filename="../ui/FilterPanel.cpp" line="217"/>
        <location filename="../ui/FilterPanel.cpp" line="218"/>
        <source>Match case</source>
        <translation>区分大小写</translation>
    </message>
    <message>
        <location filename="../ui/FilterPanel.cpp" line="219"/>
        <source>Previous match (Shift+F3)</source>
        <translation>上一个匹配（Shift+F3）</translation>
    </message>
    <message>
        <location filename="../ui/FilterPanel.cpp" line="220"/>
        <source>Next match (F3)</source>
        <translation>下一个匹配（F3）</translation>
    </message>
    <message>
        <location filename="../ui/FilterPanel.cpp" line="221"/>
        <source>Current match / total matches</source>
        <translation>当前匹配 / 匹配总数</translation>
    </message>
    <message>
        <location filename="../ui/FilterPanel.cpp" line="223"/>
        <source>Apply</source>
        <translation>应用</translation>
    </message>
    <message>
        <location filename="../ui/FilterPanel.cpp" line="224"/>
        <source>Clear</source>
        <translation>清除</translation>
    </message>
    <message>
        <location filename="../ui/FilterPanel.cpp" line="225"/>
        <source>Invert</source>
        <translation>反向过滤</translation>
    </message>
    <message>
        <location filename="../ui/FilterPanel.cpp" line="226"/>
        <source>Show only rows that do not match the filter pattern</source>
        <translation>只显示不包含过滤关键词或模式的行</translation>
    </message>
    <message>
        <location filename="../ui/FilterPanel.cpp" line="233"/>
        <source>%1 min</source>
        <translation>%1 分钟</translation>
    </message>
    <message>
        <location filename="../ui/FilterPanel.cpp" line="235"/>
        <source>1 hour</source>
        <translation>1 小时</translation>
    </message>
    <message>
        <location filename="../ui/FilterPanel.cpp" line="237"/>
        <source>Today</source>
        <translation>今天</translation>
    </message>
    <message>
        <location filename="../ui/FilterPanel.cpp" line="239"/>
        <source>All</source>
        <translation>全部</translation>
    </message>
    <message>
        <location filename="../ui/FilterPanel.cpp" line="242"/>
        <source>Enable the time range filter</source>
        <translation>启用时间范围过滤</translation>
    </message>
    <message>
        <location filename="../ui/FilterPanel.cpp" line="288"/>
        <source>Show entries of this level</source>
        <translation>显示该级别的条目</translation>
    </message>
    <message>
        <location filename="../ui/FilterPanel.cpp" line="302"/>
        <source>Show entries of this level (%1 entries)</source>
        <translation>显示该级别的条目（%1 条）</translation>
    </message>
    <message>
        <location filename="../ui/FilterPanel.cpp" line="269"/>
        <source>Expand the search and filter panel</source>
        <translation>展开查找与过滤面板</translation>
    </message>
    <message>
        <location filename="../ui/FilterPanel.cpp" line="270"/>
        <source>Collapse the search and filter panel</source>
        <translation>折叠查找与过滤面板</translation>
    </message>
</context>
<context>
    <name>lv::LogTableView</name>
    <message>
        <location filename="../ui/LogTableView.cpp" line="105"/>
        <source>New lines: %1 ▼</source>
        <translation>新行：%1 ▼</translation>
    </message>
    <message>
        <location filename="../ui/LogTableView.cpp" line="516"/>
        <source>Copied %1 characters from &quot;%2&quot;</source>
        <translation>已从“%2”复制 %1 个字符</translation>
    </message>
    <message>
        <location filename="../ui/LogTableView.cpp" line="533"/>
        <source>Copied row %1 (%2 characters)</source>
        <translation>已复制第 %1 行（%2 个字符）</translation>
    </message>
    <message>
        <location filename="../ui/LogTableView.cpp" line="566"/>
        <source>Copied %1 cells (%2 characters)</source>
        <translation>已复制 %1 个单元格（%2 个字符）</translation>
    </message>
    <message>
        <location filename="../ui/LogTableView.cpp" line="576"/>
        <source>Copy Cell</source>
        <translation>复制单元格</translation>
    </message>
    <message>
        <location filename="../ui/LogTableView.cpp" line="577"/>
        <source>Copy Row</source>
        <translation>复制整行</translation>
    </message>
    <message>
        <location filename="../ui/LogTableView.cpp" line="578"/>
        <source>Copy Message</source>
        <translation>复制消息</translation>
    </message>
    <message>
        <location filename="../ui/LogTableView.cpp" line="580"/>
        <source>Auto-fit Columns</source>
        <translation>列宽自适应</translation>
    </message>
    <message>
        <location filename="../ui/LogTableView.cpp" line="599"/>
        <source>Copied message (%1 characters)</source>
        <translation>已复制消息（%1 个字符）</translation>
    </message>
</context>
<context>
    <name>lv::MainWindow</name>
    <message>
        <location filename="../ui/MainWindow.cpp" line="235"/>
        <location filename="../ui/MainWindow.cpp" line="477"/>
        <source>Reset All Settings</source>
        <translation>重置全部设置</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="236"/>
        <source>Restore every setting (fonts, language, layout, appearance) to its default value?</source>
        <translation>将所有设置（字体、语言、布局、外观）恢复为默认值吗？</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="435"/>
        <source>&amp;File</source>
        <translation>文件(&amp;F)</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="436"/>
        <source>&amp;Settings</source>
        <translation>设置(&amp;S)</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="437"/>
        <source>Recent Files</source>
        <translation>最近打开的文件</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="438"/>
        <source>Font</source>
        <translation>字体</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="439"/>
        <source>Language</source>
        <translation>语言</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="440"/>
        <source>Layout</source>
        <translation>布局</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="441"/>
        <source>Details Pane</source>
        <translation>详情框</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="442"/>
        <source>Appearance</source>
        <translation>外观</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="443"/>
        <source>Theme</source>
        <translation>主题</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="444"/>
        <source>Open…</source>
        <translation>打开…</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="445"/>
        <source>Refresh</source>
        <translation>刷新</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="446"/>
        <source>Close</source>
        <translation>关闭</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="447"/>
        <source>Monitor</source>
        <translation>监控</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="448"/>
        <source>Follow the file like &quot;tail -f&quot; (Ctrl+M, single file only)</source>
        <translation>像 “tail -f” 一样实时跟随文件（Ctrl+M，仅单文件可用）</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="449"/>
        <source>Export Filtered Results…</source>
        <translation>导出过滤结果…</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="450"/>
        <source>Exit</source>
        <translation>退出</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="451"/>
        <source>Clear List</source>
        <translation>清空列表</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="453"/>
        <source>Interface Font…</source>
        <translation>界面字体…</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="454"/>
        <source>Log Table Font…</source>
        <translation>日志表格字体…</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="455"/>
        <source>Log Header Font…</source>
        <translation>日志表头字体…</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="456"/>
        <source>Reset to Defaults</source>
        <translation>恢复默认值</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="458"/>
        <source>English</source>
        <translation>English</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="461"/>
        <source>Details: Right</source>
        <translation>详情：右侧</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="462"/>
        <source>Details: Bottom</source>
        <translation>详情：底部</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="464"/>
        <source>Show Details Pane</source>
        <translation>显示详情框</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="466"/>
        <source>Checked: the details pane is always visible (the first entry is selected when a log is loaded). Unchecked: the pane is never shown - use Enter to expand a row instead</source>
        <translation>勾选：详情框始终显示（打开日志时自动选中第一条）。取消勾选：详情框始终不显示，可选中行后按 Enter 展开查看完整内容</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="469"/>
        <source>Light</source>
        <translation>浅色</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="470"/>
        <source>Dark</source>
        <translation>深色</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="471"/>
        <source>Follow System</source>
        <translation>跟随系统</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="472"/>
        <source>Highlight Color…</source>
        <translation>高亮颜色…</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="473"/>
        <source>Syntax Highlighting</source>
        <translation>语法高亮</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="474"/>
        <source>Follow Theme</source>
        <translation>跟随主题</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="479"/>
        <source>About</source>
        <translation>关于</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="481"/>
        <source>Log</source>
        <translation>日志</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="482"/>
        <source>No log file open</source>
        <translation>未打开日志文件</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="484"/>
        <source>Open a log file with File ▸ Open, drop one onto this window, or start the application with a file name.
Run &quot;log-viewer --demo&quot; to preview the interface with sample data.</source>
        <translation>通过“文件 ▸ 打开”选择日志文件，或把文件拖到本窗口，也可以在启动时直接附带文件名。
运行“log-viewer --demo”可用示例数据预览界面。</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="490"/>
        <source>Open Log File…</source>
        <translation>打开日志文件…</translation>
    </message>
    <message>
        <source>Open as Administrator…</source>
        <translation>以管理员权限打开…</translation>
    </message>
    <message>
        <source>(elevated snapshot)</source>
        <translation>（已提权快照）</translation>
    </message>
    <message>
        <source>Read &quot;%1&quot; with administrator rights and open a read-only snapshot.</source>
        <translation>以管理员权限读取“%1”，并以只读快照方式打开。</translation>
    </message>
    <message>
        <source>Could not open &quot;%1&quot; with elevated privileges: %2</source>
        <translation>无法以管理员权限打开“%1”：%2</translation>
    </message>
    <message>
        <source>Opened a read-only snapshot of &quot;%1&quot; with elevated privileges.</source>
        <translation>已通过管理员权限打开“%1”的只读快照。</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="492"/>
        <location filename="../ui/MainWindow.cpp" line="628"/>
        <source>Monitor: off</source>
        <translation>监控：关闭</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="609"/>
        <source>Monitoring %1 (tail -f)</source>
        <translation>正在监控 %1（tail -f）</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="633"/>
        <source>Monitor: on (%1 new lines)</source>
        <translation>监控：开启（%1 条新行）</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="635"/>
        <source>Monitor: on</source>
        <translation>监控：开启</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="647"/>
        <source>%1 new lines (follow paused)</source>
        <translation>%1 条新行（已暂停跟随）</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="660"/>
        <source>The log file was rotated or truncated; the document was reloaded.</source>
        <translation>日志文件已轮转或被截断，文档已重新加载。</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="673"/>
        <source>Open Log Files</source>
        <translation>打开日志文件</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="674"/>
        <source>Log files (*.log *.txt *.out);;All files (*)</source>
        <translation>日志文件 (*.log *.txt *.out);;所有文件 (*)</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="779"/>
        <location filename="../ui/MainWindow.cpp" line="810"/>
        <source>Invalid pattern: %1</source>
        <translation>模式无效：%1</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="868"/>
        <source>Export Filtered Results</source>
        <translation>导出过滤结果</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="869"/>
        <source>CSV files (*.csv);;Text files (*.txt)</source>
        <translation>CSV 文件 (*.csv);;文本文件 (*.txt)</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="875"/>
        <source>Cannot write &apos;%1&apos;: %2</source>
        <translation>无法写入“%1”：%2</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="908"/>
        <source>Exported %1 rows to %2</source>
        <translation>已导出 %1 行到 %2</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="946"/>
        <source>Interface Font</source>
        <translation>界面字体</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="950"/>
        <source>Log Table Font</source>
        <translation>日志表格字体</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="954"/>
        <source>Log Header Font</source>
        <translation>日志表头字体</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="977"/>
        <source>Highlight Color</source>
        <translation>高亮颜色</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="1005"/>
        <source>No file open</source>
        <translation>未打开文件</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="1020"/>
        <source>Log document</source>
        <translation>日志文档</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="1022"/>
        <source>(truncated)</source>
        <translation>（已截断）</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="1031"/>
        <source>%1 of %2 lines</source>
        <translation>显示 %1 / 共 %2 行</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="1033"/>
        <source>%1 lines</source>
        <translation>%1 行</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="1056"/>
        <source>Demo data loaded. Filtering and monitoring arrive in later milestones.</source>
        <translation>演示数据已加载。过滤与监控功能将在后续里程碑中提供。</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="1067"/>
        <source>Merging several files arrives in a later milestone; opening the first file.</source>
        <translation>多文件合并将在后续里程碑中提供；当前只打开第一个文件。</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="1085"/>
        <source>Large file</source>
        <translation>大文件</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="1086"/>
        <source>The file contains more than %1 lines; only the first %1 lines are loaded.

Load the complete file now?</source>
        <translation>文件超过 %1 行，当前只加载了前 %1 行。

现在加载完整文件吗？</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="455"/>
        <source>&amp;Columns</source>
        <translation>列(&amp;C)</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="1131"/>
        <source>Column %1</source>
        <translation>第 %1 列</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="1141"/>
        <source>The Line column is always shown</source>
        <translation>行号列始终显示，无法隐藏</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="1155"/>
        <source>Show All Columns</source>
        <translation>显示全部列</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="518"/>
        <source>Full Screen</source>
        <translation>全屏</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="520"/>
        <source>Collapse the search and filter panel and use the whole screen (F11; leave with Esc)</source>
        <translation>折叠查找与过滤面板并使用全屏显示（F11；按 Esc 退出全屏）</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp" line="1354"/>
        <source>Loaded in %1</source>
        <translation>加载耗时 %1</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp"/>
        <source>File Association</source>
        <translation>文件关联</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp"/>
        <source>Associate .log Files</source>
        <translation>关联 .log 文件</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp"/>
        <source>Register .log files for the current user so that double-clicking one opens this executable. Uncheck to remove the association again.</source>
        <translation>为当前用户注册 .log 文件关联，双击 .log 文件即用本程序打开。取消勾选即可移除关联。</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp"/>
        <source>Double-clicking a %1 file now opens:
%2</source>
        <translation>现在双击 %1 文件将用以下程序打开：
%2</translation>
    </message>
    <message>
        <location filename="../ui/MainWindow.cpp"/>
        <source>The %1 association was removed and the previous one restored.</source>
        <translation>已移除 %1 关联并还原原有的关联。</translation>
    </message>
</context>
</TS>
