#include "LanguageRegistryFormat.h"

namespace PickCore::Languages {
    void formatLanguagesReport(std::ostream &out,
                               const LanguageRegistry &registry,
                               const LanguageModuleBootLog &bootLog,
                               const bool verbose) {
        const std::vector<LanguageNamespaceSummary> namespaces = registry.listNamespaces();
        out << "Language namespaces (" << namespaces.size() << "):\n";
        for (const LanguageNamespaceSummary &ns: namespaces) {
            out << "  " << ns.id << ' ' << ns.metadata.name << ' ' << ns.metadata.version << ' ' << ns.functionCount
                << '\n';
            if (!verbose) {
                continue;
            }
            for (const LanguageFunctionSlotSummary &slot: registry.functionSlotSummaries(ns.id)) {
                out << "    " << slot.id << ' ';
                if (slot.implemented) {
                    out << slot.arity;
                } else {
                    out << '-';
                }
                out << '\n';
            }
        }

        if (bootLog.attemptedCount() > 0) {
            out << "Module boot: " << bootLog.loadedCount() << " loaded, " << bootLog.failedCount() << " failed ("
                << bootLog.attemptedCount() << " attempted)\n";
        }

        if (bootLog.failedCount() > 0) {
            out << "Boot failures:\n";
            for (const LanguageModuleBootEntry &entry: bootLog.entries()) {
                if (entry.loaded) {
                    continue;
                }
                out << "  " << entry.fileName;
                if (!entry.detail.empty()) {
                    out << ": " << entry.detail;
                }
                out << '\n';
            }
        }
    }
} // namespace PickCore::Languages
