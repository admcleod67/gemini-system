#include "GeminiSession.h"

#include <iostream>

namespace PickShell {
    GeminiSession::GeminiSession() : shell_(runtime_) { bindIoToShellAndRuntime(); }

    void GeminiSession::setInputStream(std::istream *const in) {
        inputStream_ = in;
        bindIoToShellAndRuntime();
    }

    void GeminiSession::setOutputStream(std::ostream *const out) {
        outputStream_ = out;
        bindIoToShellAndRuntime();
    }

    void GeminiSession::setDiagnosticStream(std::ostream *const err) {
        diagnosticStream_ = err;
    }

    std::istream &GeminiSession::input() const {
        return inputStream_ != nullptr ? *inputStream_ : std::cin;
    }

    std::ostream &GeminiSession::output() const {
        return outputStream_ != nullptr ? *outputStream_ : std::cout;
    }

    std::ostream &GeminiSession::diagnostic() const {
        return diagnosticStream_ != nullptr ? *diagnosticStream_ : std::cerr;
    }

    void GeminiSession::bindIoToShellAndRuntime() {
        shell_.setInputStream(inputStream_);
        shell_.setOutputStream(outputStream_);
        runtime_.setInputStream(inputStream_);
        runtime_.setOutputStream(outputStream_);
    }
} // namespace PickShell
