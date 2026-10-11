#ifndef PICK_SYSTEM_HOSTFS_POSIX_HOST_FILESYSTEM_H
#define PICK_SYSTEM_HOSTFS_POSIX_HOST_FILESYSTEM_H

#include <gemini/HostFilesystem.hpp>

#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace Gemini {
    /// Portable host-path stream backend using `std::fstream` (Milestone 22 Stage 2).
    class PosixHostFilesystem final : public HostFilesystem {
    public:
        PosixHostFilesystem() = default;
        ~PosixHostFilesystem() override;

        PosixHostFilesystem(const PosixHostFilesystem &) = delete;
        PosixHostFilesystem &operator=(const PosixHostFilesystem &) = delete;

        [[nodiscard]] int openRead(const std::string &path) override;
        [[nodiscard]] int openWrite(const std::string &path) override;
        [[nodiscard]] int openAppend(const std::string &path) override;
        [[nodiscard]] std::string read(int handle, int maxBytes) override;
        [[nodiscard]] int write(int handle, const std::string &data) override;
        void close(int handle) override;

    private:
        struct Slot {
            std::fstream stream;
        };

        [[nodiscard]] static bool pathEmpty(const std::string &path);
        [[nodiscard]] int allocateHandle(std::unique_ptr<Slot> slot);
        [[nodiscard]] Slot &requireSlot(int handle);

        std::vector<std::unique_ptr<Slot>> slots_;
    };
} // namespace Gemini

#endif // PICK_SYSTEM_HOSTFS_POSIX_HOST_FILESYSTEM_H
