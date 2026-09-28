#include "ui/DemoData.h"

#include <QCoreApplication>

namespace lv {
namespace {

LogEntry makeEntry(const QDateTime &time, LogLevel level, const QString &thread,
                   const QString &target, const QString &message, qint64 line)
{
    LogEntry entry;
    entry.time = time;
    entry.level = level;
    entry.rawLevel = logLevelCanonicalName(level);
    entry.thread = thread;
    entry.target = target;
    entry.message = message;
    entry.firstLine = line;
    return entry;
}

class DemoProvider : public IEntryProvider
{
public:
    DemoProvider()
    {
        const QDate day(2026, 9, 28);
        int line = 1;

        m_entries.append(makeEntry(QDateTime(day, QTime(22, 15, 43, 303)), LogLevel::Info,
                                   QStringLiteral("ThreadId(02)"),
                                   QStringLiteral("shadowsocks_rust::service::local"),
                                   QStringLiteral("shadowsocks local 1.25.0 build 2026-08-29T07:51:00"),
                                   line++));

        m_entries.append(makeEntry(QDateTime(day, QTime(22, 15, 44, 17)), LogLevel::Debug,
                                   QStringLiteral("ThreadId(02)"),
                                   QStringLiteral("loadbalancing::ping_balancer"),
                                   QStringLiteral("balancer: checked & updated remote UDP server "
                                                  "216.24.176.223:27894 (BWG-GIA) (score: 88), "
                                                  "ServerStatData { latency_median: 178, fail_rate: 0.0, "
                                                  "latency_stdev: 3535.5339059327375, latency_mean: 178.0, "
                                                  "latency_mad: 0 }"),
                                   line++));

        m_entries.append(makeEntry(QDateTime(day, QTime(22, 16, 7, 110)), LogLevel::Debug,
                                   QStringLiteral("tokio-rt-worker ThreadId(04)"),
                                   QStringLiteral("http::http_service"),
                                   QStringLiteral("CONNECT relay connected 127.0.0.1:50424 <-> "
                                                  "login.live.com:443 (proxied)"),
                                   line++));

        m_entries.append(makeEntry(
            QDateTime(day, QTime(22, 16, 8, 492)), LogLevel::Warn,
            QStringLiteral("tokio-rt-worker ThreadId(27)"), QStringLiteral("utils"),
            QStringLiteral("slow tunnel detected: proxy config = {\"server\":\"23.105.206.76\","
                           "\"port\":9684,\"method\":\"aes-256-gcm\",\"plugin\":\"kcptun-client\","
                           "\"plugin_opts\":{\"mode\":\"fast2\",\"mtu\":1350,\"nocomp\":false},"
                           "\"timeout\":300,\"balanced\":[{\"host\":\"bwg-cn2\",\"latency_ms\":182},"
                           "{\"host\":\"bwg-gia\",\"latency_ms\":178}]}"),
            line++));

        m_entries.append(makeEntry(
            QDateTime(day, QTime(22, 16, 17, 159)), LogLevel::Error,
            QStringLiteral("tokio-rt-worker ThreadId(27)"),
            QStringLiteral("relay::tcprelay::utils"),
            QStringLiteral("copy bidirection ends with error: 远程主机强迫关闭了一个现有的连接。 "
                           "(os error 10054), a_to_b: Running(CopyBuffer { read_done: false, pos: 0, "
                           "cap: 0, amt: 0, .. }), b_to_a: Running(CopyBuffer { read_done: false, "
                           "pos: 8097, cap: 8097, amt: 44094, .. })"),
            line++));

        m_entries.append(makeEntry(
            QDateTime(day, QTime(22, 17, 2, 5)), LogLevel::Trace,
            QStringLiteral("tokio-rt-worker ThreadId(11)"), QStringLiteral("local::socks"),
            QStringLiteral("socks5 handshake <request><version>5</version><command>CONNECT</command>"
                           "<address>api.github.com</address><port>443</port></request>"),
            line++));

        m_entries.append(makeEntry(
            QDateTime(day, QTime(22, 17, 30, 220)), LogLevel::Notice,
            QStringLiteral("ThreadId(02)"), QStringLiteral("service::local"),
            QStringLiteral("configuration reloaded from C:/shadowsocks/config.json:\n"
                           "server: 23.105.206.76\n"
                           "mode: tcp_and_udp\n"
                           "servers: 4 entries (BWG-CN2, BWG-GIA, BWG-KVM, VULTR)"),
            line++));

        m_entries.append(makeEntry(
            QDateTime(day, QTime(22, 18, 11, 940)), LogLevel::Fatal,
            QStringLiteral("ThreadId(02)"), QStringLiteral("service::local"),
            QStringLiteral("failed to bind 127.0.0.1:1080 after 3 retries: address already in use; "
                           "shutting down gracefully - see "
                           "https://example.invalid/log-viewer/troubleshooting#port-in-use for details"),
            line++));

        LogEntry multiline = makeEntry(
            QDateTime(day, QTime(22, 19, 3, 12)), LogLevel::Error,
            QStringLiteral("tokio-rt-worker ThreadId(19)"), QStringLiteral("plugin::manager"),
            QStringLiteral("plugin failed to start: kcptun-client.exe (exit code 1)\n"
                           "stderr: 2026-09-28 22:19:03 ERROR listen udp :49671: bind: "
                           "An attempt was made to access a socket in a way forbidden by its access permissions.\n"
                           "  at shadowsocks::plugin::Plugin::spawn (plugin.rs:214)\n"
                           "  at shadowsocks::service::local::run (local.rs:97)"),
            line);
        multiline.physicalLines = 4;
        multiline.extra.insert(QStringLiteral("ThreadId"), QStringLiteral("19"));
        m_entries.append(multiline);
        line += 4;

        m_entries.append(makeEntry(
            QDateTime(day, QTime(22, 20, 45, 700)), LogLevel::Info,
            QStringLiteral("ThreadId(02)"), QStringLiteral("service::local"),
            QStringLiteral("yaml configuration accepted:\n"
                           "log:\n"
                           "  level: info\n"
                           "  format: plain\n"
                           "  timestamps: true\n"
                           "servers:\n"
                           "  - host: 23.105.206.76\n"
                           "    port: 9684\n"
                           "    tag: BWG-CN2\n"
                           "  - host: 216.24.176.223\n"
                           "    port: 27894\n"
                           "    tag: BWG-GIA"),
            line++));

        for (LogEntry &entry : m_entries)
            entry.sourceIndex = 0;
    }

    int rowCount() const override { return m_entries.size(); }

    const LogEntry &entryAt(int row) const override { return m_entries.at(row); }

    QVector<int> levelCounts() const override
    {
        QVector<int> counts(kLogLevelCount, 0);
        for (const LogEntry &entry : m_entries)
            counts[logLevelIndex(entry.level)] += 1;
        return counts;
    }

    DocumentInfo documentInfo() const override
    {
        DocumentInfo info;
        info.fileName = QCoreApplication::translate("DemoData", "demo-preview.log");
        info.encodingName = QStringLiteral("UTF-8");
        info.formatId = QStringLiteral("demo");
        info.formatName = QCoreApplication::translate("DemoData", "Demo data");
        info.lineCount = m_entries.size();
        info.fileSize = 4096;
        info.multiFile = false;
        return info;
    }

    QString sourceName(int sourceIndex) const override
    {
        Q_UNUSED(sourceIndex);
        return QCoreApplication::translate("DemoData", "demo-preview.log");
    }

private:
    QVector<LogEntry> m_entries;
};

} // namespace

EntryProviderPtr createDemoProvider()
{
    return std::make_shared<DemoProvider>();
}

} // namespace lv
