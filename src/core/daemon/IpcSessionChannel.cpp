#include "IpcSessionChannel.h"

#include <cerrno>
#include <unistd.h>

namespace PickCore {
    class ChannelInputBuffer final : public std::streambuf {
    public:
        explicit ChannelInputBuffer(IpcSessionChannel &owner) : owner_(owner) {}

    protected:
        int underflow() override {
            if (gptr() < egptr()) {
                return traits_type::to_int_type(*gptr());
            }
            const int ch = owner_.readInputChar();
            if (ch == traits_type::eof()) {
                return traits_type::eof();
            }
            staging_ = static_cast<char>(ch);
            setg(&staging_, &staging_, &staging_ + 1);
            return traits_type::to_int_type(staging_);
        }

    private:
        IpcSessionChannel &owner_;
        char staging_{0};
    };

    class ChannelOutputBuffer final : public std::streambuf {
    public:
        ChannelOutputBuffer(IpcSessionChannel &owner, const bool diagnostic)
            : owner_(owner),
              diagnostic_(diagnostic) {}

    protected:
        int overflow(const int ch) override {
            if (ch == traits_type::eof()) {
                return traits_type::not_eof(ch);
            }
            owner_.writeOutputChar(ch, diagnostic_);
            return ch;
        }

        std::streamsize xsputn(const char *data, const std::streamsize count) override {
            if (count <= 0) {
                return 0;
            }
            owner_.writeOutputBytes(data, count, diagnostic_);
            return count;
        }

        int sync() override {
            owner_.markPendingOutput();
            return 0;
        }

    private:
        IpcSessionChannel &owner_;
        bool diagnostic_;
    };

    IpcSessionChannel::IpcSessionChannel()
        : inputBuffer_(std::make_unique<ChannelInputBuffer>(*this)),
          outputBuffer_(std::make_unique<ChannelOutputBuffer>(*this, false)),
          diagnosticBuffer_(std::make_unique<ChannelOutputBuffer>(*this, true)),
          inputStream_(inputBuffer_.get()),
          outputStream_(outputBuffer_.get()),
          diagnosticStream_(diagnosticBuffer_.get()) {}

    IpcSessionChannel::~IpcSessionChannel() = default;

    std::istream &IpcSessionChannel::input() {
        return inputStream_;
    }

    std::ostream &IpcSessionChannel::output() {
        return outputStream_;
    }

    std::ostream &IpcSessionChannel::diagnostic() {
        return diagnosticStream_;
    }

    void IpcSessionChannel::pushInput(const std::span<const std::uint8_t> bytes) {
        if (bytes.empty()) {
            return;
        }
        {
            const std::lock_guard lock(mutex_);
            if (closed_) {
                return;
            }
            inputQueue_.insert(inputQueue_.end(), bytes.begin(), bytes.end());
        }
        inputAvailable_.notify_all();
    }

    void IpcSessionChannel::close() {
        {
            const std::lock_guard lock(mutex_);
            closed_ = true;
        }
        inputAvailable_.notify_all();
    }

    bool IpcSessionChannel::hasPendingOutput() const {
        const std::lock_guard lock(mutex_);
        return hasPendingOutput_;
    }

    void IpcSessionChannel::flushPendingOutput(const int clientFd) {
        for (;;) {
            std::vector<std::uint8_t> outputChunk;
            std::vector<std::uint8_t> diagnosticChunk;
            {
                const std::lock_guard lock(mutex_);
                if (outputQueue_.empty() && diagnosticQueue_.empty()) {
                    hasPendingOutput_ = false;
                    return;
                }
                outputChunk.swap(outputQueue_);
                diagnosticChunk.swap(diagnosticQueue_);
            }

            if (!outputChunk.empty() && !flushQueue(clientFd, DaemonIpcMessageType::SessionOutput, outputChunk)) {
                const std::lock_guard lock(mutex_);
                outputQueue_.insert(outputQueue_.begin(), outputChunk.begin(), outputChunk.end());
                hasPendingOutput_ = true;
                return;
            }

            if (!diagnosticChunk.empty() &&
                !flushQueue(clientFd, DaemonIpcMessageType::SessionDiagnostic, diagnosticChunk)) {
                const std::lock_guard lock(mutex_);
                diagnosticQueue_.insert(diagnosticQueue_.begin(), diagnosticChunk.begin(), diagnosticChunk.end());
                hasPendingOutput_ = true;
                return;
            }
        }
    }

    bool IpcSessionChannel::flushQueue(const int clientFd,
                                       const DaemonIpcMessageType type,
                                       std::vector<std::uint8_t> &queue) {
        while (!queue.empty()) {
            const std::size_t chunkSize = std::min(queue.size(), kDaemonIpcMaxSessionDataSize);
            DaemonIpcSessionDataPayload payload{};
            payload.data.assign(queue.begin(), queue.begin() + static_cast<std::ptrdiff_t>(chunkSize));

            DaemonIpcFrame frame{};
            frame.type = type;
            frame.payload = encodeSessionDataPayload(payload);
            const std::vector<std::uint8_t> bytes = encodeDaemonIpcFrame(frame);

            std::size_t offset = 0;
            while (offset < bytes.size()) {
                const auto written = ::write(clientFd, bytes.data() + offset, bytes.size() - offset);
                if (written < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        queue.erase(queue.begin(), queue.begin() + static_cast<std::ptrdiff_t>(chunkSize));
                        queue.insert(queue.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(offset), bytes.end());
                        return false;
                    }
                    return false;
                }
                offset += static_cast<std::size_t>(written);
            }

            queue.erase(queue.begin(), queue.begin() + static_cast<std::ptrdiff_t>(chunkSize));
        }
        return true;
    }

    int IpcSessionChannel::readInputChar() {
        std::unique_lock lock(mutex_);
        inputAvailable_.wait(lock, [this] { return closed_ || !inputQueue_.empty(); });
        if (inputQueue_.empty()) {
            return std::char_traits<char>::eof();
        }
        const std::uint8_t value = inputQueue_.front();
        inputQueue_.pop_front();
        return static_cast<int>(value);
    }

    void IpcSessionChannel::writeOutputChar(const int ch, const bool diagnostic) {
        const std::lock_guard lock(mutex_);
        if (ch == std::char_traits<char>::eof()) {
            return;
        }
        if (diagnostic) {
            diagnosticQueue_.push_back(static_cast<std::uint8_t>(ch));
        } else {
            outputQueue_.push_back(static_cast<std::uint8_t>(ch));
        }
        hasPendingOutput_ = true;
    }

    void IpcSessionChannel::writeOutputBytes(const char *data, const std::streamsize count, const bool diagnostic) {
        const std::lock_guard lock(mutex_);
        if (diagnostic) {
            diagnosticQueue_.insert(diagnosticQueue_.end(), reinterpret_cast<const std::uint8_t *>(data),
                                    reinterpret_cast<const std::uint8_t *>(data + count));
        } else {
            outputQueue_.insert(outputQueue_.end(), reinterpret_cast<const std::uint8_t *>(data),
                                reinterpret_cast<const std::uint8_t *>(data + count));
        }
        hasPendingOutput_ = true;
    }

    void IpcSessionChannel::markPendingOutput() {
        const std::lock_guard lock(mutex_);
        hasPendingOutput_ = true;
    }
} // namespace PickCore
