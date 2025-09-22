#pragma once
#include "../Interfaces/IDeveloperTool.h"
#include "../Common/DevToolsTypes.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <chrono>

namespace AstralEngine {

class Engine;

class ConsoleWindow : public IDeveloperTool {
public:
    struct ConsoleCommand {
        std::string name;
        std::string description;
        std::vector<std::string> parameters;
        std::function<void(const std::vector<std::string>&)> execute;
        std::function<std::vector<std::string>(const std::string&)> autoComplete;
    };
    
    struct ConsoleVariable {
        std::string name;
        std::string description;
        std::any value;
        std::type_index type;
        bool readOnly;
        std::function<void(const std::any&)> onChange;
    };
    
    struct ConsoleMessage {
        enum class Level { Info, Warning, Error, Debug };
        
        Level level;
        std::string message;
        std::chrono::system_clock::time_point timestamp;
        std::string source;
    };
    
    ConsoleWindow();
    ~ConsoleWindow() = default;
    
    // IDeveloperTool implementasyonu
    void OnInitialize() override;
    void OnUpdate(float deltaTime) override;
    void OnRender() override;
    void OnShutdown() override;
    
    const std::string& GetName() const override { return m_name; }
    bool IsEnabled() const override { return m_enabled; }
    void SetEnabled(bool enabled) override { m_enabled = enabled; }
    
    void LoadSettings(const std::string& settings) override;
    std::string SaveSettings() const override;
    
    // Komut yönetimi
    void RegisterCommand(const ConsoleCommand& command);
    void UnregisterCommand(const std::string& commandName);
    void ExecuteCommand(const std::string& command);
    void ExecuteCommand(const std::string& commandName, const std::vector<std::string>& args);
    
    // Değişken yönetimi
    template<typename T>
    void RegisterVariable(const std::string& name, T* value, const std::string& description = "", 
                         bool readOnly = false, std::function<void(const T&)> onChange = nullptr);
    
    void UnregisterVariable(const std::string& name);
    template<typename T>
    bool GetVariableValue(const std::string& name, T& value) const;
    template<typename T>
    bool SetVariableValue(const std::string& name, const T& value);
    
    // Loglama
    void Log(const std::string& message, ConsoleMessage::Level level = ConsoleMessage::Level::Info, 
             const std::string& source = "");
    void LogInfo(const std::string& message, const std::string& source = "");
    void LogWarning(const std::string& message, const std::string& source = "");
    void LogError(const std::string& message, const std::string& source = "");
    void LogDebug(const std::string& message, const std::string& source = "");
    
    // Yardımcı metodlar
    std::vector<std::string> GetCommandSuggestions(const std::string& input) const;
    std::vector<std::string> GetVariableSuggestions(const std::string& input) const;
    const ConsoleCommand* GetCommand(const std::string& name) const;
    const ConsoleVariable* GetVariable(const std::string& name) const;
    
private:
    // Render metodları
    void RenderConsole();
    void RenderCommandHistory();
    void RenderCommandInput();
    void RenderVariableInspector();
    void RenderLogMessages();
    void RenderCommandHelp();
    
    // Komut işleme
    void ProcessCommand(const std::string& command);
    void SplitCommand(const std::string& command, std::string& commandName, std::vector<std::string>& args);
    void ExecuteBuiltinCommand(const std::string& commandName, const std::vector<std::string>& args);
    
    // Değişken işleme
    void RenderVariable(ConsoleVariable& variable);
    void RenderVariableValue(ConsoleVariable& variable);
    template<typename T>
    void RenderVariableValueImpl(ConsoleVariable& variable);
    
    // Auto-complete
    void UpdateAutoComplete();
    void ApplyAutoCompleteSuggestion(const std::string& suggestion);
    
    // Log entegrasyonu
    void OnLogMessage(const std::string& message, int level);
    
    std::string m_name = "Console";
    bool m_enabled = true;
    
    // Komut sistemi
    std::unordered_map<std::string, ConsoleCommand> m_commands;
    std::vector<std::string> m_commandHistory;
    int m_commandHistoryIndex = -1;
    
    // Değişken sistemi
    std::unordered_map<std::string, ConsoleVariable> m_variables;
    
    // Log mesajları
    std::vector<ConsoleMessage> m_logMessages;
    size_t m_maxLogMessages = 1000;
    
    // UI durumu
    std::string m_currentInput;
    bool m_scrollToBottom = true;
    bool m_autoCompleteEnabled = true;
    std::vector<std::string> m_autoCompleteSuggestions;
    int m_selectedSuggestion = -1;
    
    // Filtreleme
    bool m_showInfoMessages = true;
    bool m_showWarningMessages = true;
    bool m_showErrorMessages = true;
    bool m_showDebugMessages = false;
    std::string m_logFilter = "";
    
    // Ayarlar
    bool m_showConsole = true;
    bool m_showVariableInspector = true;
    bool m_showCommandHelp = false;
    float m_windowAlpha = 0.9f;
    int m_historySize = 100;
    
    // Performans
    float m_timeSinceLastUpdate = 0.0f;
    float m_updateInterval = 0.1f;
};

// Template implementasyonları
template<typename T>
void ConsoleWindow::RegisterVariable(const std::string& name, T* value, const std::string& description, 
                                   bool readOnly, std::function<void(const T&)> onChange) {
    ConsoleVariable variable;
    variable.name = name;
    variable.description = description;
    variable.value = *value;
    variable.type = typeid(T);
    variable.readOnly = readOnly;
    
    if (onChange) {
        variable.onChange = [onChange](const std::any& val) {
            onChange(std::any_cast<T>(val));
        };
    }
    
    m_variables[name] = variable;
}

template<typename T>
bool ConsoleWindow::GetVariableValue(const std::string& name, T& value) const {
    auto it = m_variables.find(name);
    if (it != m_variables.end() && it->second.type == typeid(T)) {
        value = std::any_cast<T>(it->second.value);
        return true;
    }
    return false;
}

template<typename T>
bool ConsoleWindow::SetVariableValue(const std::string& name, const T& value) {
    auto it = m_variables.find(name);
    if (it != m_variables.end() && it->second.type == typeid(T) && !it->second.readOnly) {
        it->second.value = value;
        if (it->second.onChange) {
            it->second.onChange(value);
        }
        LogInfo("Variable '" + name + "' set to: " + std::to_string(value), "Console");
        return true;
    }
    return false;
}

} // namespace AstralEngine