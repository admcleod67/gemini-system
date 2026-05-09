//
// File-backed HELP topic resolution (Milestone 6).
//

#ifndef PICK_SYSTEM_TCL_HELPTOPICS_H
#define PICK_SYSTEM_TCL_HELPTOPICS_H

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace PickFS {
    class FileSystem;
}
namespace PickVoc {
    class VocResolver;
}

namespace PickShell::HelpTopics {

/// Join operand tokens for error display (preserves operator casing).
[[nodiscard]] std::string joinOperandsDisplay(const std::vector<std::string> &tokens, std::size_t operandStart);

/// Canonical lookup key: trim/collapse spaces, ASCII uppercase; single-token operands use VOC verb resolution first.
[[nodiscard]] std::string canonicalTopicKey(PickVoc::VocResolver &voc,
                                            const std::vector<std::string> &tokens,
                                            std::size_t operandStart);

/// PickFS record id for logical file HELP (spaces in canonical keys become underscores).
[[nodiscard]] std::string storageRecordIdFromCanonical(std::string_view canonicalKey);

/// Display form for HELP-LIST (underscores become spaces; see docs for limitations).
[[nodiscard]] std::string canonicalTopicFromStorageRecordId(std::string_view storageId);

[[nodiscard]] std::optional<std::string> resolveHelpBody(const PickFS::FileSystem &sessionFs,
                                                         const std::optional<std::filesystem::path> &geminiCatalogRoot,
                                                         const std::filesystem::path &sessionPickRoot,
                                                         std::string_view canonicalKey);

[[nodiscard]] const std::unordered_map<std::string, std::string> &builtinHelpBodies();

[[nodiscard]] std::string helpZeroArgumentFallbackLine();

[[nodiscard]] std::vector<std::string> listCanonicalHelpTopicsSorted(const PickFS::FileSystem &sessionFs);

} // namespace PickShell::HelpTopics

#endif // PICK_SYSTEM_TCL_HELPTOPICS_H
