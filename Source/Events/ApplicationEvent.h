#pragma once

#include "Event.h"

#include <sstream>

namespace AstralEngine {

	class WindowResizeEvent : public Event
	{
	public:
		WindowResizeEvent(unsigned int width, unsigned int height)
			: m_Width(width), m_Height(height) {}

		unsigned int GetWidth() const { return m_Width; }
		unsigned int GetHeight() const { return m_Height; }

std::string ToString() const override
{
std::stringstream ss;
ss << "WindowResizeEvent: " << m_Width << ", " << m_Height;
return ss.str();
}

std::type_index GetType() const override { return typeid(WindowResizeEvent); }
static std::type_index GetStaticType() { return typeid(WindowResizeEvent); }
const char* GetName() const override { return "WindowResize"; }
int GetCategoryFlags() const override { return EventCategoryApplication; }
private:
unsigned int m_Width, m_Height;
};

class WindowCloseEvent : public Event
{
public:
WindowCloseEvent() = default;

std::type_index GetType() const override { return typeid(WindowCloseEvent); }
static std::type_index GetStaticType() { return typeid(WindowCloseEvent); }
const char* GetName() const override { return "WindowClose"; }
int GetCategoryFlags() const override { return EventCategoryApplication; }
};

class AppTickEvent : public Event
{
public:
AppTickEvent() = default;

std::type_index GetType() const override { return typeid(AppTickEvent); }
static std::type_index GetStaticType() { return typeid(AppTickEvent); }
const char* GetName() const override { return "AppTick"; }
int GetCategoryFlags() const override { return EventCategoryApplication; }
};

class AppUpdateEvent : public Event
{
public:
AppUpdateEvent() = default;

std::type_index GetType() const override { return typeid(AppUpdateEvent); }
static std::type_index GetStaticType() { return typeid(AppUpdateEvent); }
const char* GetName() const override { return "AppUpdate"; }
int GetCategoryFlags() const override { return EventCategoryApplication; }
};

class AppRenderEvent : public Event
{
public:
AppRenderEvent() = default;

std::type_index GetType() const override { return typeid(AppRenderEvent); }
static std::type_index GetStaticType() { return typeid(AppRenderEvent); }
const char* GetName() const override { return "AppRender"; }
int GetCategoryFlags() const override { return EventCategoryApplication; }
};

    class FileDropEvent : public Event
    {
    public:
        FileDropEvent(const std::string& path, float x, float y)
            : m_Path(path), m_X(x), m_Y(y) {}

        const std::string& GetPath() const { return m_Path; }
        float GetX() const { return m_X; }
        float GetY() const { return m_Y; }

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << "FileDropEvent: " << m_Path << " at (" << m_X << ", " << m_Y << ")";
            return ss.str();
        }

        std::type_index GetType() const override { return typeid(FileDropEvent); }
        static std::type_index GetStaticType() { return typeid(FileDropEvent); }
        const char* GetName() const override { return "FileDrop"; }
        int GetCategoryFlags() const override { return EventCategoryApplication; }

    private:
        std::string m_Path;
        float m_X, m_Y;
    };
}
