# /**********************************************************************
#  * AstralEngine DevTools Subsystem CMake Configuration
#  * Purpose: DevTools subsystem için build yapılandırması
#  * Author: Astral Engine Development Team
#  * Version: 0.2.0-alpha
#  * Date: 2025-09-22
#  **********************************************************************/

# DevTools subsystem kaynak dosyaları
set(DEVTOOLS_SOURCES
    DevToolsSubsystem.cpp
    Interfaces/IDeveloperTool.cpp
    Interfaces/IDataProvider.cpp
    Interfaces/IDataConsumer.cpp
    Events/DevToolsEvent.cpp
    DataBinding/DataBindingSystem.cpp
    Profiling/ProfilingDataCollector.cpp
    Profiling/SubsystemProfiler.cpp
    Profiling/GPUProfiler.cpp
    Profiler/ProfilerWindow.cpp
    DebugRenderer/DebugRenderer.cpp
    Console/ConsoleWindow.cpp
    MaterialEditor/MaterialEditorWindow.cpp
)

# DevTools subsystem başlık dosyaları
set(DEVTOOLS_HEADERS
    DevToolsSubsystem.h
    Interfaces/IDeveloperTool.h
    Interfaces/IDataProvider.h
    Interfaces/IDataConsumer.h
    Common/DevToolsTypes.h
    Events/DevToolsEvent.h
    DataBinding/DataBindingSystem.h
    Profiling/ProfilingDataCollector.h
    Profiling/SubsystemProfiler.h
    Profiling/GPUProfiler.h
    Profiler/ProfilerWindow.h
    DebugRenderer/DebugRenderer.h
    Console/ConsoleWindow.h
    MaterialEditor/MaterialEditorWindow.h
)

# DevTools subsystem'i ana kütüphaneye ekle
target_sources(AstralEngine PRIVATE
    ${DEVTOOLS_SOURCES}
    ${DEVTOOLS_HEADERS}
)

# DevTools için include dizinleri
target_include_directories(AstralEngine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

message(STATUS "DevTools Subsystem: ${DEVTOOLS_SOURCES} kaynak dosyası eklendi")
message(STATUS "DevTools Subsystem: ${DEVTOOLS_HEADERS} başlık dosyası eklendi")