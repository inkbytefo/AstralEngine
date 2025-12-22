#pragma once

#include "EditorPanel.h"
#include "../../Core/Logger.h"
#include <vector>
#include <string>

namespace AstralEngine {

class OutputLogPanel : public EditorPanel {
public:
    OutputLogPanel();
    virtual ~OutputLogPanel() override;

    virtual void OnDraw() override;

    void AddLog(Logger::LogLevel level, const std::string& message);
    void Clear();

private:
    struct LogEntry {
        Logger::LogLevel level;
        std::string message;
        std::string timestamp;
    };

    std::vector<LogEntry> m_logs;
    bool m_autoScroll = true;
    bool m_showFilters = true;
    
    // Filters
    bool m_showInfo = true;
    bool m_showWarn = true;
    bool m_showErr = true;
    bool m_showDebug = true;

    char m_searchBuffer[256] = {0};
};

} // namespace AstralEngine
