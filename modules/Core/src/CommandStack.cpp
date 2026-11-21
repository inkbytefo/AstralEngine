#include "Astral/Core/CommandStack.hpp"

namespace Astral {

void CommandStack::PushAndExecute(std::unique_ptr<ICommand> command) {
    if (!command) return;

    command->Execute();
    m_UndoStack.push_back(std::move(command));

    if (m_UndoStack.size() > m_MaxHistory) {
        m_UndoStack.erase(m_UndoStack.begin());
    }

    m_RedoStack.clear();
}

void CommandStack::Undo() {
    if (!CanUndo()) return;

    auto cmd = std::move(m_UndoStack.back());
    m_UndoStack.pop_back();

    cmd->Undo();
    m_RedoStack.push_back(std::move(cmd));
}

void CommandStack::Redo() {
    if (!CanRedo()) return;

    auto cmd = std::move(m_RedoStack.back());
    m_RedoStack.pop_back();

    cmd->Execute();
    m_UndoStack.push_back(std::move(cmd));
}

bool CommandStack::CanUndo() const noexcept {
    return !m_UndoStack.empty();
}

bool CommandStack::CanRedo() const noexcept {
    return !m_RedoStack.empty();
}

std::string CommandStack::GetUndoName() const {
    return CanUndo() ? m_UndoStack.back()->GetName() : "";
}

std::string CommandStack::GetRedoName() const {
    return CanRedo() ? m_RedoStack.back()->GetName() : "";
}

void CommandStack::Clear() {
    m_UndoStack.clear();
    m_RedoStack.clear();
}

} // namespace Astral
