#ifndef GEMINI_HOST_FILESYSTEM_HPP
#define GEMINI_HOST_FILESYSTEM_HPP

#include <string>

namespace Gemini {
    /// Host-path sequential stream façade (Milestone 22).
    ///
    /// Injectable into language modules via `hostContext`. When unbound (null),
    /// CALL_FUNC handlers throw `HOSTFS: unbound`. Implementations throw
    /// `std::runtime_error` with a stable `HOSTFS:` prefix for all failures.
    ///
    /// Handles are positive dense ints (1-based); `0` is invalid.
    /// Paths are host strings: empty rejected; absolute OK; relative → process cwd
    /// (optional sandbox root is a Stage 2 backend detail).
    class HostFilesystem {
    public:
        virtual ~HostFilesystem() = default;

        /// Open for sequential read. Returns handle ≥ 1.
        [[nodiscard]] virtual int openRead(const std::string &path) = 0;

        /// Open for write (create/truncate). Returns handle ≥ 1.
        [[nodiscard]] virtual int openWrite(const std::string &path) = 0;

        /// Open for append (create if missing). Returns handle ≥ 1.
        [[nodiscard]] virtual int openAppend(const std::string &path) = 0;

        /// Read up to `maxBytes` bytes. Empty string means EOF.
        [[nodiscard]] virtual std::string read(int handle, int maxBytes) = 0;

        /// Write `data` bytes. Returns byte count written.
        [[nodiscard]] virtual int write(int handle, const std::string &data) = 0;

        /// Close handle. No-op or throw `HOSTFS: bad handle` / `HOSTFS: close failed`.
        virtual void close(int handle) = 0;
    };
} // namespace Gemini

#endif // GEMINI_HOST_FILESYSTEM_HPP
