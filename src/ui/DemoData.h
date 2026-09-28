#pragma once

#include "core/IEntryProvider.h"

namespace lv {

/// Synthetic document used by `log-viewer --demo` to preview the UI: it covers
/// every log level, very long messages, embedded JSON/XML/YAML snippets and
/// multi line entries.
EntryProviderPtr createDemoProvider();

} // namespace lv
