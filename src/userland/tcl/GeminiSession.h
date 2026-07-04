//
// One running Gemini System session: owns VM runtime and Tcl shell front-end.
//

#ifndef PICK_SYSTEM_TCL_GEMINI_SESSION_H
#define PICK_SYSTEM_TCL_GEMINI_SESSION_H

#pragma once

#include "Shell.h"

#include <iosfwd>

namespace PickShell {
    class GeminiSession {
    public:
        GeminiSession();

        [[nodiscard]] PickVM::Runtime &runtime() noexcept { return runtime_; }
        [[nodiscard]] const PickVM::Runtime &runtime() const noexcept { return runtime_; }

        [[nodiscard]] Shell &shell() noexcept { return shell_; }
        [[nodiscard]] const Shell &shell() const noexcept { return shell_; }

        /// nullptr selects process std::cin.
        void setInputStream(std::istream *in);
        /// nullptr selects process std::cout.
        void setOutputStream(std::ostream *out);
        /// nullptr selects process std::cerr.
        void setDiagnosticStream(std::ostream *err);

        [[nodiscard]] std::istream &input() const;
        [[nodiscard]] std::ostream &output() const;
        [[nodiscard]] std::ostream &diagnostic() const;

        [[nodiscard]] std::istream *inputStream() const noexcept { return inputStream_; }
        [[nodiscard]] std::ostream *outputStream() const noexcept { return outputStream_; }
        [[nodiscard]] std::ostream *diagnosticStream() const noexcept { return diagnosticStream_; }

    private:
        void bindIoToShellAndRuntime();

        PickVM::Runtime runtime_;
        Shell shell_;

        std::istream *inputStream_{nullptr};
        std::ostream *outputStream_{nullptr};
        std::ostream *diagnosticStream_{nullptr};
    };
} // namespace PickShell

#endif // PICK_SYSTEM_TCL_GEMINI_SESSION_H
