#pragma once

#include "Event.h"

#include <sstream>

namespace AstralEngine {

class MouseMovedEvent : public Event
{
public:
MouseMovedEvent(const float x, const float y)
: m_MouseX(x), m_MouseY(y) {}

float GetX() const { return m_MouseX; }
float GetY() const { return m_MouseY; }

std::string ToString() const override
{
std::stringstream ss;
ss << "MouseMovedEvent: " << m_MouseX << ", " << m_MouseY;
return ss.str();
}

std::type_index GetType() const override { return typeid(MouseMovedEvent); }
static std::type_index GetStaticType() { return typeid(MouseMovedEvent); }
const char* GetName() const override { return "MouseMoved"; }
int GetCategoryFlags() const override { return EventCategoryMouse | EventCategoryInput; }
private:
float m_MouseX, m_MouseY;
};

class MouseScrolledEvent : public Event
{
public:
MouseScrolledEvent(const float xOffset, const float yOffset)
: m_XOffset(xOffset), m_YOffset(yOffset) {}

float GetXOffset() const { return m_XOffset; }
float GetYOffset() const { return m_YOffset; }

std::string ToString() const override
{
std::stringstream ss;
ss << "MouseScrolledEvent: " << GetXOffset() << ", " << GetYOffset();
return ss.str();
}

std::type_index GetType() const override { return typeid(MouseScrolledEvent); }
static std::type_index GetStaticType() { return typeid(MouseScrolledEvent); }
const char* GetName() const override { return "MouseScrolled"; }
int GetCategoryFlags() const override { return EventCategoryMouse | EventCategoryInput; }
private:
float m_XOffset, m_YOffset;
};

class MouseButtonEvent : public Event
{
public:
int GetMouseButton() const { return m_Button; }

int GetCategoryFlags() const override { return EventCategoryMouse | EventCategoryInput | EventCategoryMouseButton; }
protected:
MouseButtonEvent(const int button)
: m_Button(button) {}

int m_Button;
};

class MouseButtonPressedEvent : public MouseButtonEvent
{
public:
MouseButtonPressedEvent(const int button)
: MouseButtonEvent(button) {}

std::string ToString() const override
{
std::stringstream ss;
ss << "MouseButtonPressedEvent: " << m_Button;
return ss.str();
}

std::type_index GetType() const override { return typeid(MouseButtonPressedEvent); }
static std::type_index GetStaticType() { return typeid(MouseButtonPressedEvent); }
const char* GetName() const override { return "MouseButtonPressed"; }
};

class MouseButtonReleasedEvent : public MouseButtonEvent
{
public:
MouseButtonReleasedEvent(const int button)
: MouseButtonEvent(button) {}

std::string ToString() const override
{
std::stringstream ss;
ss << "MouseButtonReleasedEvent: " << m_Button;
return ss.str();
}

std::type_index GetType() const override { return typeid(MouseButtonReleasedEvent); }
static std::type_index GetStaticType() { return typeid(MouseButtonReleasedEvent); }
const char* GetName() const override { return "MouseButtonReleased"; }
};

}
