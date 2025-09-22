#include "ConsoleWindow.h"
#include "../../../Core/Engine.h"
#include "../../../Core/Logger.h"
#include <imgui.h>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace AstralEngine {

ConsoleWindow::ConsoleWindow() {
    // Builtin komutları kaydet
    ConsoleCommand helpCmd;
    helpCmd.name = "help";
    helpCmd.description = "Shows available commands and their descriptions";
    helpCmd.execute = [this](const std::vector<std::string>&) {
        RenderCommandHelp();
    };
    m_commands["help"] = helpCmd;

    ConsoleCommand clearCmd;
    clearCmd.name = "clear";
    clearCmd.description = "Clears the console output";
    clearCmd.execute = [this](const std::vector<std::string>&) {
        m_logMessages.clear();
        LogInfo("Console cleared", "Console");
    };
    m_commands["clear"] = clearCmd;

    ConsoleCommand listCmd;
    listCmd.name = "list";
    listCmd.description = "Lists all available commands";
    listCmd.execute = [this](const std::vector<std::string>&) {
        LogInfo("Available commands:", "Console");
        for (const auto& [name, command] : m_commands) {
            LogInfo("  " + name + " - " + command.description, "Console");
        }
    };
    m_commands["list"] = listCmd;

    ConsoleCommand varsCmd;
    varsCmd.name = "vars";
    varsCmd.description = "Lists all available variables";
    varsCmd.execute = [this](const std::vector<std::string>&) {
        LogInfo("Available variables:", "Console");
        for (const auto& [name, variable] : m_variables) {
            LogInfo("  " + name + " (" + (variable.readOnly ? "read-only" : "writable") + ") - " + variable.description, "Console");
        }
    };
    m_commands["vars"] = varsCmd;

    ConsoleCommand setCmd;
    setCmd.name = "set";
    setCmd.description = "Sets the value of a variable";
    setCmd.parameters = {"<variable>", "<value>"};
    setCmd.execute = [this](const std::vector<std::string>& args) {
        if (args.size() >= 2) {
            const std::string& varName = args[0];
            const std::string& valueStr = args[1];
            
            auto it = m_variables.find(varName);
            if (it != m_variables.end()) {
                if (it->second.readOnly) {
                    LogError("Variable '" + varName + "' is read-only", "Console");
                    return;
                }

                // Değer atama mantığı
                if (it->second.type == typeid(int)) {
                    try {
                        int value = std::stoi(valueStr);
                        SetVariableValue(varName, value);
                    } catch (...) {
                        LogError("Invalid integer value: " + valueStr, "Console");
                    }
                } else if (it->second.type == typeid(float)) {
                    try {
                        float value = std::stof(valueStr);
                        SetVariableValue(varName, value);
                    } catch (...) {
                        LogError("Invalid float value: " + valueStr, "Console");
                    }
                } else if (it->second.type == typeid(bool)) {
                    bool value = (valueStr == "true" || valueStr == "1" || valueStr == "yes");
                    SetVariableValue(varName, value);
                } else if (it->second.type == typeid(std::string)) {
                    SetVariableValue(varName, valueStr);
                } else {
                    LogError("Unsupported variable type", "Console");
                }
            } else {
                LogError("Unknown variable: " + varName, "Console");
            }
        } else {
            LogError("Usage: set <variable> <value>", "Console");
        }
    };
    m_commands["set"] = setCmd;

    ConsoleCommand getCmd;
    getCmd.name = "get";
    getCmd.description = "Gets the value of a variable";
    getCmd.parameters = {"<variable>"};
    getCmd.execute = [this](const std::vector<std::string>& args) {
        if (args.size() >= 1) {
            const std::string& varName = args[0];
            auto it = m_variables.find(varName);
            if (it != m_variables.end()) {
                std::string valueStr;
                if (it->second.type == typeid(int)) {
                    valueStr = std::to_string(std::any_cast<int>(it->second.value));
                } else if (it->second.type == typeid(float)) {
                    valueStr = std::to_string(std::any_cast<float>(it->second.value));
                } else if (it->second.type == typeid(bool)) {
                    valueStr = std::any_cast<bool>(it->second.value) ? "true" : "false";
                } else if (it->second.type == typeid(std::string)) {
                    valueStr = std::any_cast<std::string>(it->second.value);
                } else {
                    valueStr = "[unsupported type]";
                }
                LogInfo(varName + " = " + valueStr, "Console");
            } else {
                LogError("Unknown variable: " + varName, "Console");
            }
        } else {
            LogError("Usage: get <variable>", "Console");
        }
    };
    m_commands["get"] = getCmd;
}

void ConsoleWindow::OnInitialize() {
    LogInfo("ConsoleWindow initialized", "Console");
}

void ConsoleWindow::OnUpdate(float deltaTime) {
    m_timeSinceLastUpdate += deltaTime;
    if (m_timeSinceLastUpdate >= m_updateInterval) {
        m_timeSinceLastUpdate = 0.0f;
        // Periyodik güncellemeler burada yapılacak
    }
}

void ConsoleWindow::OnRender() {
    if (!m_enabled) return;

    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(m_windowAlpha);
    
    if (ImGui::Begin("Console", &m_enabled)) {
        RenderConsole();
    }
    ImGui::End();
}

void ConsoleWindow::OnShutdown() {
    LogInfo("ConsoleWindow shutdown", "Console");
}

void ConsoleWindow::LoadSettings(const std::string& settings) {
    // Ayarları yükle - basit implementasyon
    LogInfo("Console settings loaded", "Console");
}

std::string ConsoleWindow::SaveSettings() const {
    // Ayarları kaydet - basit implementasyon
    return "{}";
}

void ConsoleWindow::RegisterCommand(const ConsoleCommand& command) {
    m_commands[command.name] = command;
    LogInfo("Command registered: " + command.name, "Console");
}

void ConsoleWindow::UnregisterCommand(const std::string& commandName) {
    auto it = m_commands.find(commandName);
    if (it != m_commands.end()) {
        m_commands.erase(it);
        LogInfo("Command unregistered: " + commandName, "Console");
    }
}

void ConsoleWindow::ExecuteCommand(const std::string& command) {
    if (command.empty()) return;
    
    // Komut geçmişine ekle
    m_commandHistory.push_back(command);
    if (m_commandHistory.size() > static_cast<size_t>(m_historySize)) {
        m_commandHistory.erase(m_commandHistory.begin());
    }
    m_commandHistoryIndex = -1;
    
    ProcessCommand(command);
}

void ConsoleWindow::ExecuteCommand(const std::string& commandName, const std::vector<std::string>& args) {
    auto it = m_commands.find(commandName);
    if (it != m_commands.end()) {
        it->second.execute(args);
    } else {
        LogError("Unknown command: " + commandName, "Console");
    }
}

void ConsoleWindow::UnregisterVariable(const std::string& name) {
    auto it = m_variables.find(name);
    if (it != m_variables.end()) {
        m_variables.erase(it);
        LogInfo("Variable unregistered: " + name, "Console");
    }
}

void ConsoleWindow::Log(const std::string& message, ConsoleMessage::Level level, const std::string& source) {
    ConsoleMessage msg;
    msg.level = level;
    msg.message = message;
    msg.timestamp = std::chrono::system_clock::now();
    msg.source = source;
    
    m_logMessages.push_back(msg);
    
    // Log mesajlarını sınırla
    if (m_logMessages.size() > m_maxLogMessages) {
        m_logMessages.erase(m_logMessages.begin());
    }
    
    m_scrollToBottom = true;
}

void ConsoleWindow::LogInfo(const std::string& message, const std::string& source) {
    Log(message, ConsoleMessage::Level::Info, source);
}

void ConsoleWindow::LogWarning(const std::string& message, const std::string& source) {
    Log(message, ConsoleMessage::Level::Warning, source);
}

void ConsoleWindow::LogError(const std::string& message, const std::string& source) {
    Log(message, ConsoleMessage::Level::Error, source);
}

void ConsoleWindow::LogDebug(const std::string& message, const std::string& source) {
    Log(message, ConsoleMessage::Level::Debug, source);
}

std::vector<std::string> ConsoleWindow::GetCommandSuggestions(const std::string& input) const {
    std::vector<std::string> suggestions;
    for (const auto& [name, command] : m_commands) {
        if (name.find(input) == 0) {
            suggestions.push_back(name);
        }
    }
    return suggestions;
}

std::vector<std::string> ConsoleWindow::GetVariableSuggestions(const std::string& input) const {
    std::vector<std::string> suggestions;
    for (const auto& [name, variable] : m_variables) {
        if (name.find(input) == 0) {
            suggestions.push_back(name);
        }
    }
    return suggestions;
}

const ConsoleWindow::ConsoleCommand* ConsoleWindow::GetCommand(const std::string& name) const {
    auto it = m_commands.find(name);
    return it != m_commands.end() ? &it->second : nullptr;
}

const ConsoleWindow::ConsoleVariable* ConsoleWindow::GetVariable(const std::string& name) const {
    auto it = m_variables.find(name);
    return it != m_variables.end() ? &it->second : nullptr;
}

void ConsoleWindow::RenderConsole() {
    // Tab bar
    if (ImGui::BeginTabBar("ConsoleTabs")) {
        if (ImGui::BeginTabItem("Console")) {
            m_showConsole = true;
            ImGui::EndTabItem();
        } else {
            m_showConsole = false;
        }
        
        if (ImGui::BeginTabItem("Variables")) {
            m_showVariableInspector = true;
            ImGui::EndTabItem();
        } else {
            m_showVariableInspector = false;
        }
        
        if (ImGui::BeginTabItem("Help")) {
            m_showCommandHelp = true;
            ImGui::EndTabItem();
        } else {
            m_showCommandHelp = false;
        }
        
        ImGui::EndTabBar();
    }
    
    if (m_showConsole) {
        RenderLogMessages();
        RenderCommandInput();
    }
    
    if (m_showVariableInspector) {
        RenderVariableInspector();
    }
    
    if (m_showCommandHelp) {
        RenderCommandHelp();
    }
}

void ConsoleWindow::RenderLogMessages() {
    // Filtre kontrolleri
    ImGui::Checkbox("Info", &m_showInfoMessages);
    ImGui::SameLine();
    ImGui::Checkbox("Warning", &m_showWarningMessages);
    ImGui::SameLine();
    ImGui::Checkbox("Error", &m_showErrorMessages);
    ImGui::SameLine();
    ImGui::Checkbox("Debug", &m_showDebugMessages);
    
    ImGui::SameLine();
    ImGui::Text("Filter:");
    ImGui::SameLine();
    ImGui::InputText("##LogFilter", &m_logFilter);
    
    // Log mesajları
    ImGui::BeginChild("LogScroll", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), false, ImGuiWindowFlags_HorizontalScrollbar);
    
    for (const auto& msg : m_logMessages) {
        // Filtreleme
        if (!m_logFilter.empty() && msg.message.find(m_logFilter) == std::string::npos) {
            continue;
        }
        
        switch (msg.level) {
            case ConsoleMessage::Level::Info:
                if (!m_showInfoMessages) continue;
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "[INFO] %s", msg.message.c_str());
                break;
            case ConsoleMessage::Level::Warning:
                if (!m_showWarningMessages) continue;
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "[WARN] %s", msg.message.c_str());
                break;
            case ConsoleMessage::Level::Error:
                if (!m_showErrorMessages) continue;
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "[ERROR] %s", msg.message.c_str());
                break;
            case ConsoleMessage::Level::Debug:
                if (!m_showDebugMessages) continue;
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[DEBUG] %s", msg.message.c_str());
                break;
        }
    }
    
    if (m_scrollToBottom) {
        ImGui::SetScrollHereY(1.0f);
        m_scrollToBottom = false;
    }
    
    ImGui::EndChild();
}

void ConsoleWindow::RenderCommandInput() {
    ImGui::Separator();
    
    // Komut input
    ImGui::PushItemWidth(-1);
    
    if (ImGui::InputText("##CommandInput", &m_currentInput, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory)) {
        if (!m_currentInput.empty()) {
            ExecuteCommand(m_currentInput);
            m_currentInput.clear();
        }
        ImGui::SetKeyboardFocusHere(-1);
    }
    
    // Auto-complete
    if (m_autoCompleteEnabled && !m_currentInput.empty()) {
        UpdateAutoComplete();
        
        if (!m_autoCompleteSuggestions.empty()) {
            ImGui::SetNextWindowPos(ImVec2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y));
            ImGui::SetNextWindowSize(ImVec2(ImGui::GetItemRectSize().x, 200));
            
            if (ImGui::Begin("AutoComplete", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) {
                for (int i = 0; i < m_autoCompleteSuggestions.size(); i++) {
                    bool isSelected = (i == m_selectedSuggestion);
                    if (ImGui::Selectable(m_autoCompleteSuggestions[i].c_str(), isSelected)) {
                        ApplyAutoCompleteSuggestion(m_autoCompleteSuggestions[i]);
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
            }
            ImGui::End();
        }
    }
    
    ImGui::PopItemWidth();
}

void ConsoleWindow::RenderVariableInspector() {
    ImGui::Text("Registered Variables:");
    ImGui::Separator();
    
    if (m_variables.empty()) {
        ImGui::Text("No variables registered");
        return;
    }
    
    for (auto& [name, variable] : m_variables) {
        ImGui::PushID(name.c_str());
        RenderVariable(variable);
        ImGui::PopID();
        ImGui::Separator();
    }
}

void ConsoleWindow::RenderVariable(ConsoleVariable& variable) {
    ImGui::Text("%s", variable.name.c_str());
    ImGui::SameLine();
    if (variable.readOnly) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(read-only)");
    }
    
    if (!variable.description.empty()) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", variable.description.c_str());
    }
    
    ImGui::Text("Type: %s", variable.type.name());
    
    ImGui::Text("Value: ");
    ImGui::SameLine();
    RenderVariableValue(variable);
}

void ConsoleWindow::RenderVariableValue(ConsoleVariable& variable) {
    if (variable.type == typeid(int)) {
        RenderVariableValueImpl<int>(variable);
    } else if (variable.type == typeid(float)) {
        RenderVariableValueImpl<float>(variable);
    } else if (variable.type == typeid(bool)) {
        RenderVariableValueImpl<bool>(variable);
    } else if (variable.type == typeid(std::string)) {
        RenderVariableValueImpl<std::string>(variable);
    } else {
        ImGui::Text("Unsupported type");
    }
}

template<typename T>
void ConsoleWindow::RenderVariableValueImpl(ConsoleVariable& variable) {
    T value = std::any_cast<T>(variable.value);
    
    if (variable.readOnly) {
        ImGui::Text("%s", std::to_string(value).c_str());
    } else {
        if constexpr (std::is_same_v<T, int>) {
            if (ImGui::InputInt("##value", &value)) {
                variable.value = value;
                if (variable.onChange) {
                    variable.onChange(value);
                }
            }
        } else if constexpr (std::is_same_v<T, float>) {
            if (ImGui::InputFloat("##value", &value)) {
                variable.value = value;
                if (variable.onChange) {
                    variable.onChange(value);
                }
            }
        } else if constexpr (std::is_same_v<T, bool>) {
            if (ImGui::Checkbox("##value", &value)) {
                variable.value = value;
                if (variable.onChange) {
                    variable.onChange(value);
                }
            }
        } else if constexpr (std::is_same_v<T, std::string>) {
            char buffer[256];
            strncpy(buffer, value.c_str(), sizeof(buffer));
            if (ImGui::InputText("##value", buffer, sizeof(buffer))) {
                variable.value = std::string(buffer);
                if (variable.onChange) {
                    variable.onChange(std::string(buffer));
                }
            }
        }
    }
}

void ConsoleWindow::RenderCommandHelp() {
    ImGui::Text("Available Commands:");
    ImGui::Separator();
    
    for (const auto& [name, command] : m_commands) {
        ImGui::Text("%s", name.c_str());
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "- %s", command.description.c_str());
        
        if (!command.parameters.empty()) {
            ImGui::SameLine();
            std::string params;
            for (const auto& param : command.parameters) {
                params += param + " ";
            }
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 1.0f, 1.0f), "[%s]", params.c_str());
        }
    }
}

void ConsoleWindow::ProcessCommand(const std::string& command) {
    std::string commandName;
    std::vector<std::string> args;
    
    SplitCommand(command, commandName, args);
    
    if (commandName.empty()) return;
    
    // Önce builtin komutları kontrol et
    ExecuteBuiltinCommand(commandName, args);
}

void ConsoleWindow::SplitCommand(const std::string& command, std::string& commandName, std::vector<std::string>& args) {
    std::istringstream iss(command);
    std::string token;
    
    if (iss >> commandName) {
        while (iss >> token) {
            args.push_back(token);
        }
    }
}

void ConsoleWindow::ExecuteBuiltinCommand(const std::string& commandName, const std::vector<std::string>& args) {
    auto it = m_commands.find(commandName);
    if (it != m_commands.end()) {
        it->second.execute(args);
    } else {
        LogError("Unknown command: " + commandName, "Console");
    }
}

void ConsoleWindow::UpdateAutoComplete() {
    m_autoCompleteSuggestions.clear();
    
    // Komut önerileri
    auto cmdSuggestions = GetCommandSuggestions(m_currentInput);
    m_autoCompleteSuggestions.insert(m_autoCompleteSuggestions.end(), cmdSuggestions.begin(), cmdSuggestions.end());
    
    // Değişken önerileri
    auto varSuggestions = GetVariableSuggestions(m_currentInput);
    m_autoCompleteSuggestions.insert(m_autoCompleteSuggestions.end(), varSuggestions.begin(), varSuggestions.end());
    
    // Seçimi sıfırla
    m_selectedSuggestion = -1;
}

void ConsoleWindow::ApplyAutoCompleteSuggestion(const std::string& suggestion) {
    m_currentInput = suggestion;
    m_autoCompleteSuggestions.clear();
    m_selectedSuggestion = -1;
}

void ConsoleWindow::OnLogMessage(const std::string& message, int level) {
    ConsoleMessage::Level consoleLevel = ConsoleMessage::Level::Info;
    
    switch (level) {
        case 0: // Trace
        case 1: // Debug
            consoleLevel = ConsoleMessage::Level::Debug;
            break;
        case 2: // Info
            consoleLevel = ConsoleMessage::Level::Info;
            break;
        case 3: // Warning
            consoleLevel = ConsoleMessage::Level::Warning;
            break;
        case 4: // Error
        case 5: // Critical
            consoleLevel = ConsoleMessage::Level::Error;
            break;
    }
    
    Log(message, consoleLevel, "Engine");
}

} // namespace AstralEngine