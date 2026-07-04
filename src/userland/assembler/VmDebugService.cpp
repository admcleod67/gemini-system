#include "VmDebugService.h"

#include "GeminiSession.h"
#include "InstructionPrint.h"

#include <algorithm>
#include <stdexcept>

namespace PickShell {
    VmDebugService::VmDebugService(GeminiSession &session) : session_(session) {}

    bool VmDebugService::hasProgramLoaded() const {
        return session_.programImageLoaded();
    }

    void VmDebugService::setTrace(const bool traceEnabled) {
        session_.trace_ = traceEnabled;
    }

    bool VmDebugService::trace() const {
        return session_.trace_;
    }

    void VmDebugService::addBreakpoint(const std::size_t index) {
        session_.breakpoints_.insert(index);
    }

    bool VmDebugService::removeBreakpoint(const std::size_t index) {
        return session_.breakpoints_.erase(index) != 0U;
    }

    void VmDebugService::clearBreakpoints() {
        session_.breakpoints_.clear();
    }

    std::vector<std::size_t> VmDebugService::sortedBreakpoints() const {
        std::vector<std::size_t> sorted(session_.breakpoints_.begin(), session_.breakpoints_.end());
        std::sort(sorted.begin(), sorted.end());
        return sorted;
    }

    void VmDebugService::endProgramContext() {
        session_.lastLoaded_.reset();
        session_.suspended_ = false;
        session_.resumePastBreakpointIp_.reset();
        session_.runtime().loadProgram({});
    }

    void VmDebugService::dumpProgram(std::ostream &out) const {
        if (!session_.programImageLoaded()) {
            out << "No program loaded\n";
            return;
        }
        for (std::size_t i = 0; i < session_.lastLoaded_->program.size(); ++i) {
            out << PickVM::formatInstructionLine(i, session_.lastLoaded_->program[i], &*session_.lastLoaded_) << '\n';
        }
    }

    void VmDebugService::dumpLabels(std::ostream &out) const {
        if (!session_.programImageLoaded()) {
            out << "No program loaded\n";
            return;
        }
        if (session_.lastLoaded_->labels.empty()) {
            out << "No labels\n";
            return;
        }

        std::vector<std::pair<std::string, int>> pairs(session_.lastLoaded_->labels.begin(), session_.lastLoaded_->labels.end());
        std::sort(pairs.begin(), pairs.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
        for (const auto &p: pairs) {
            out << p.first << " -> " << p.second << '\n';
        }
    }

    VmDebugService::StepResult VmDebugService::stepVm(std::ostream &out) {
        if (!session_.programImageLoaded()) {
            return {StepOutcome::NoProgramLoaded, {}};
        }
        if (session_.runtime().instructionPointer() >= session_.lastLoaded_->program.size()) {
            return {StepOutcome::Halted, {}};
        }

        session_.suspended_ = false;
        session_.resumePastBreakpointIp_.reset();
        const std::size_t ip = session_.runtime().instructionPointer();
        const PickVM::Instruction &instr = session_.lastLoaded_->program[ip];
        out << PickVM::formatInstructionLine(ip, instr, &*session_.lastLoaded_) << '\n';
        session_.runtime().setOutputStream(&out);
        const StepResult result = stepRuntime(false, out, &*session_.lastLoaded_);
        session_.runtime().setOutputStream(nullptr);
        return result;
    }

    VmDebugService::StepResult VmDebugService::stepRuntime(const bool traceEnabled,
                                                           std::ostream &out,
                                                           const PickVM::LoadedBytecode *loadedView) const {
        const std::size_t ip = session_.runtime().instructionPointer();
        if (traceEnabled && loadedView != nullptr && ip < loadedView->program.size()) {
            out << PickVM::formatInstructionLine(ip, loadedView->program[ip], loadedView) << '\n';
        }
        try {
            if (!session_.runtime().step()) {
                return {StepOutcome::Halted, {}};
            }
            return {StepOutcome::Advanced, {}};
        } catch (const PickVM::UserInterrupt &) {
            return {StepOutcome::Interrupted, {}};
        } catch (const std::runtime_error &e) {
            return {StepOutcome::RuntimeError, e.what()};
        }
    }
} // namespace PickShell
