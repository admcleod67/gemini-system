//
// Read optional default data file pointer from logical MD (account binding).
//

#ifndef PICK_SYSTEM_CORE_MD_MDDEFAULTDATAFILE_H
#define PICK_SYSTEM_CORE_MD_MDDEFAULTDATAFILE_H

#pragma once

#include <optional>
#include <string>

namespace PickFS {
    class FileSystem;
}

namespace PickCore {
    /// Logical file `MD`, record `DEFDATA`: first non-empty line = logical Pick file name.
    /// Returns nullopt if missing, unreadable, or name invalid.
    std::optional<std::string> loadDefaultDataFileFromMd(const PickFS::FileSystem &fs);
} // namespace PickCore

#endif // PICK_SYSTEM_CORE_MD_MDDEFAULTDATAFILE_H
