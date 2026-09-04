#pragma once

#include <string>
#include <vector>
#include <memory>
#include <utility>

namespace Astral {

/// Komut Deseni (Command Pattern) tabanli soyut geri alinabilir eylem arayuzu.
class ICommand {
public:
    virtual ~ICommand() = default;

    /// Komutu calistir
    virtual void Execute() = 0;

    /// Komutun etkisini geri al
    virtual void Undo() = 0;

    /// Kullaniciya menude veya bildirimlerde gosterilecek komut aciklamasi
    [[nodiscard]] virtual std::string GetName() const = 0;
};

/// Undo/Redo mekanizmasini yoneten cift-yiginli (Undo & Redo) Komut Yigini.
class CommandStack {
public:
    explicit CommandStack(size_t maxHistory = 50)
        : m_MaxHistory(maxHistory) {}

    ~CommandStack() = default;

    CommandStack(const CommandStack&) = delete;
    CommandStack& operator=(const CommandStack&) = delete;

    CommandStack(CommandStack&&) noexcept = default;
    CommandStack& operator=(CommandStack&&) noexcept = default;

    /// Yeni bir komutu calistirir ve Undo yiginina ekler, Redo yiginini sifirlar.
    void PushAndExecute(std::unique_ptr<ICommand> command);

    /// Son komutun etkisini geri alir ve Redo yiginina tasir.
    void Undo();

    /// Geri alinan komutu tekrar calistirir ve Undo yiginina tasir.
    void Redo();

    /// Geri alinabilir bir komut olup olmadigini dondurur.
    [[nodiscard]] bool CanUndo() const noexcept;

    /// Yeniden yapilabilir bir komut olup olmadigini dondurur.
    [[nodiscard]] bool CanRedo() const noexcept;

    /// Geri alinacak siradaki komutun adini dondurur.
    [[nodiscard]] std::string GetUndoName() const;

    /// Yeniden yapilacak siradaki komutun adini dondurur.
    [[nodiscard]] std::string GetRedoName() const;

    /// Yiginlari temizler
    void Clear();

    [[nodiscard]] size_t GetUndoCount() const noexcept { return m_UndoStack.size(); }
    [[nodiscard]] size_t GetRedoCount() const noexcept { return m_RedoStack.size(); }
    [[nodiscard]] size_t GetMaxHistory() const noexcept { return m_MaxHistory; }

private:
    size_t m_MaxHistory;
    std::vector<std::unique_ptr<ICommand>> m_UndoStack;
    std::vector<std::unique_ptr<ICommand>> m_RedoStack;
};

} // namespace Astral
