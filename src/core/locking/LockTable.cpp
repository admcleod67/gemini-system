#include "LockTable.h"

namespace PickCore::Locking {
    namespace {
        bool isValidKey(const std::string &value) {
            return !value.empty();
        }

        void fillConflict(LockConflict *out,
                          const std::string &fileName,
                          const std::string &recordId,
                          const LockEntry &entry) {
            if (out == nullptr) {
                return;
            }
            out->fileName = fileName;
            out->recordId = recordId;
            out->ownerSessionId = entry.ownerSessionId;
            out->lockType = entry.lockType;
        }
    } // namespace

    std::string lockTypeLabel(const LockType type) {
        switch (type) {
            case LockType::ReadU:
                return "READU";
            case LockType::WriteU:
                return "WRITEU";
        }
        return "READU";
    }

    std::string describeLockConflict(const LockConflict &conflict) {
        return std::string{"RECORD LOCKED: "} + conflict.fileName + " " + conflict.recordId + " (owner " +
               conflict.ownerSessionId + ", " + lockTypeLabel(conflict.lockType) + ")";
    }

    bool LockTable::tryAcquire(const std::string &sessionId,
                               const std::string &fileName,
                               const std::string &recordId,
                               const LockType lockType,
                               LockConflict *conflictOut) {
        if (!isValidKey(sessionId) || !isValidKey(fileName) || !isValidKey(recordId)) {
            return false;
        }

        std::unordered_map<std::string, LockEntry> &fileLocks = locks_[fileName];
        const auto it = fileLocks.find(recordId);
        if (it != fileLocks.end()) {
            if (it->second.ownerSessionId != sessionId) {
                fillConflict(conflictOut, fileName, recordId, it->second);
                return false;
            }
            it->second.lockType = lockType;
            it->second.acquiredAt = std::chrono::steady_clock::now();
            return true;
        }

        LockEntry entry;
        entry.ownerSessionId = sessionId;
        entry.lockType = lockType;
        entry.acquiredAt = std::chrono::steady_clock::now();
        fileLocks.emplace(recordId, std::move(entry));
        return true;
    }

    bool LockTable::release(const std::string &sessionId,
                           const std::string &fileName,
                           const std::string &recordId) {
        if (!isValidKey(sessionId) || !isValidKey(fileName) || !isValidKey(recordId)) {
            return false;
        }

        const auto fileIt = locks_.find(fileName);
        if (fileIt == locks_.end()) {
            return false;
        }

        const auto recordIt = fileIt->second.find(recordId);
        if (recordIt == fileIt->second.end() || recordIt->second.ownerSessionId != sessionId) {
            return false;
        }

        fileIt->second.erase(recordIt);
        if (fileIt->second.empty()) {
            locks_.erase(fileIt);
        }
        return true;
    }

    std::size_t LockTable::releaseAllForSession(const std::string &sessionId) {
        if (!isValidKey(sessionId)) {
            return 0;
        }

        std::size_t removed = 0;
        for (auto fileIt = locks_.begin(); fileIt != locks_.end();) {
            auto &recordMap = fileIt->second;
            for (auto recordIt = recordMap.begin(); recordIt != recordMap.end();) {
                if (recordIt->second.ownerSessionId == sessionId) {
                    recordIt = recordMap.erase(recordIt);
                    ++removed;
                } else {
                    ++recordIt;
                }
            }
            if (recordMap.empty()) {
                fileIt = locks_.erase(fileIt);
            } else {
                ++fileIt;
            }
        }
        return removed;
    }

    std::size_t LockTable::releaseAllForFile(const std::string &fileName) {
        if (!isValidKey(fileName)) {
            return 0;
        }

        const auto fileIt = locks_.find(fileName);
        if (fileIt == locks_.end()) {
            return 0;
        }

        const std::size_t removed = fileIt->second.size();
        locks_.erase(fileIt);
        return removed;
    }

    std::optional<LockEntry> LockTable::lookup(const std::string &fileName,
                                               const std::string &recordId) const {
        if (!isValidKey(fileName) || !isValidKey(recordId)) {
            return std::nullopt;
        }

        const auto fileIt = locks_.find(fileName);
        if (fileIt == locks_.end()) {
            return std::nullopt;
        }

        const auto recordIt = fileIt->second.find(recordId);
        if (recordIt == fileIt->second.end()) {
            return std::nullopt;
        }
        return recordIt->second;
    }
} // namespace PickCore::Locking
