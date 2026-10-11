#include "PosixHostFilesystem.h"

#include <filesystem>
#include <stdexcept>
#include <utility>
#include <vector>

namespace Gemini {
    namespace {
        [[noreturn]] void throwHostFs(const char *message) {
            throw std::runtime_error(message);
        }
    } // namespace

    PosixHostFilesystem::~PosixHostFilesystem() {
        for (auto &slot : slots_) {
            if (slot && slot->stream.is_open()) {
                slot->stream.close();
            }
        }
    }

    bool PosixHostFilesystem::pathEmpty(const std::string &path) {
        return path.empty();
    }

    int PosixHostFilesystem::allocateHandle(std::unique_ptr<Slot> slot) {
        for (std::size_t i = 0; i < slots_.size(); ++i) {
            if (!slots_[i]) {
                slots_[i] = std::move(slot);
                return static_cast<int>(i + 1);
            }
        }
        slots_.push_back(std::move(slot));
        return static_cast<int>(slots_.size());
    }

    PosixHostFilesystem::Slot &PosixHostFilesystem::requireSlot(const int handle) {
        if (handle < 1 || static_cast<std::size_t>(handle) > slots_.size()) {
            throwHostFs("HOSTFS: bad handle");
        }
        auto &slot = slots_[static_cast<std::size_t>(handle - 1)];
        if (!slot || !slot->stream.is_open()) {
            throwHostFs("HOSTFS: bad handle");
        }
        return *slot;
    }

    int PosixHostFilesystem::openRead(const std::string &path) {
        if (pathEmpty(path)) {
            throwHostFs("HOSTFS: invalid path");
        }
        std::error_code ec;
        if (!std::filesystem::exists(path, ec) || ec) {
            throwHostFs("HOSTFS: not found");
        }
        auto slot = std::make_unique<Slot>();
        slot->stream.open(path, std::ios::in | std::ios::binary);
        if (!slot->stream.is_open()) {
            throwHostFs("HOSTFS: open failed");
        }
        return allocateHandle(std::move(slot));
    }

    int PosixHostFilesystem::openWrite(const std::string &path) {
        if (pathEmpty(path)) {
            throwHostFs("HOSTFS: invalid path");
        }
        auto slot = std::make_unique<Slot>();
        slot->stream.open(path, std::ios::out | std::ios::trunc | std::ios::binary);
        if (!slot->stream.is_open()) {
            throwHostFs("HOSTFS: open failed");
        }
        return allocateHandle(std::move(slot));
    }

    int PosixHostFilesystem::openAppend(const std::string &path) {
        if (pathEmpty(path)) {
            throwHostFs("HOSTFS: invalid path");
        }
        auto slot = std::make_unique<Slot>();
        slot->stream.open(path, std::ios::out | std::ios::app | std::ios::binary);
        if (!slot->stream.is_open()) {
            throwHostFs("HOSTFS: open failed");
        }
        return allocateHandle(std::move(slot));
    }

    std::string PosixHostFilesystem::read(const int handle, const int maxBytes) {
        if (maxBytes <= 0) {
            (void) requireSlot(handle);
            return {};
        }
        Slot &slot = requireSlot(handle);
        std::vector<char> buf(static_cast<std::size_t>(maxBytes));
        slot.stream.read(buf.data(), static_cast<std::streamsize>(maxBytes));
        const auto got = slot.stream.gcount();
        if (got < 0) {
            throwHostFs("HOSTFS: read failed");
        }
        if (slot.stream.bad()) {
            throwHostFs("HOSTFS: read failed");
        }
        // eofbit / failbit after short read at EOF are normal
        slot.stream.clear(slot.stream.rdstate() & ~std::ios::failbit);
        return std::string(buf.data(), static_cast<std::size_t>(got));
    }

    int PosixHostFilesystem::write(const int handle, const std::string &data) {
        Slot &slot = requireSlot(handle);
        if (!data.empty()) {
            slot.stream.write(data.data(), static_cast<std::streamsize>(data.size()));
            if (!slot.stream) {
                throwHostFs("HOSTFS: write failed");
            }
            slot.stream.flush();
            if (!slot.stream) {
                throwHostFs("HOSTFS: write failed");
            }
        }
        return static_cast<int>(data.size());
    }

    void PosixHostFilesystem::close(const int handle) {
        if (handle < 1 || static_cast<std::size_t>(handle) > slots_.size()) {
            throwHostFs("HOSTFS: bad handle");
        }
        auto &slotPtr = slots_[static_cast<std::size_t>(handle - 1)];
        if (!slotPtr || !slotPtr->stream.is_open()) {
            throwHostFs("HOSTFS: bad handle");
        }
        slotPtr->stream.close();
        if (slotPtr->stream.fail()) {
            slotPtr.reset();
            throwHostFs("HOSTFS: close failed");
        }
        slotPtr.reset();
    }
} // namespace Gemini
