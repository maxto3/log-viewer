#pragma once

#include "highlight/HighlightTheme.h"

#include <QString>
#include <QVector>

namespace lv {

/// Finds JSON / XML / YAML snippets inside a log message and tokenizes them so
/// the message can be painted with the VSCode colours (spec.md REQ-HL-02).
///
/// False positives are avoided by requiring a minimum length, a complete
/// pairing of braces/tags and by limiting the work per message (REQ-HL-04).
class SnippetTokenizer
{
public:
    static constexpr int kMinSnippetLength = 8;
    static constexpr int kMaxSnippets = 4;
    static constexpr int kMaxSnippetChars = 2000;

    /// Token spans for all snippets found in \a text, sorted by position.
    static QVector<TokenSpan> tokenize(const QString &text);

private:
    static void tokenizeJsonSnippets(const QString &text, int from, QVector<TokenSpan> &spans, int *budget);
    static void tokenizeXmlSnippets(const QString &text, int from, QVector<TokenSpan> &spans, int *budget);
    static void tokenizeYamlSnippets(const QString &text, int from, QVector<TokenSpan> &spans, int *budget);

    static void tokenizeJsonRange(const QString &text, int start, int length, QVector<TokenSpan> &spans);
    static void tokenizeXmlRange(const QString &text, int start, int length, QVector<TokenSpan> &spans);
    static void tokenizeYamlRange(const QString &text, int start, int length, QVector<TokenSpan> &spans);
};

} // namespace lv
