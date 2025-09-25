#pragma once

#include "Event.h"
#include "Subsystems/Platform/KeyCode.h"

#include <sstream>

namespace AstralEngine {

	class KeyEvent : public Event
	{
	public:
KeyCode GetKeyCode() const { return m_KeyCode; }

int GetCategoryFlags() const override { return EventCategoryKeyboard | EventCategoryInput; }
protected:
KeyEvent(const KeyCode keycode)
: m_KeyCode(keycode) {}

KeyCode m_KeyCode;
};

	class KeyPressedEvent : public KeyEvent
	{
	public:
		KeyPressedEvent(const KeyCode keycode, bool isRepeat = false)
			: KeyEvent(keycode), m_IsRepeat(isRepeat) {}

		bool IsRepeat() const { return m_IsRepeat; }

std::string ToString() const override
{
std::stringstream ss;
ss << "KeyPressedEvent: " << static_cast<int>(m_KeyCode) << " (repeat = " << m_IsRepeat << ")";
return ss.str();
}

std::type_index GetType() const override { return typeid(KeyPressedEvent); }
static std::type_index GetStaticType() { return typeid(KeyPressedEvent); }
const char* GetName() const override { return "KeyPressed"; }
private:
bool m_IsRepeat;
};

	class KeyReleasedEvent : public KeyEvent
	{
	public:
		KeyReleasedEvent(const KeyCode keycode)
			: KeyEvent(keycode) {}

std::string ToString() const override
{
std::stringstream ss;
ss << "KeyReleasedEvent: " << static_cast<int>(m_KeyCode);
return ss.str();
}

std::type_index GetType() const override { return typeid(KeyReleasedEvent); }
static std::type_index GetStaticType() { return typeid(KeyReleasedEvent); }
const char* GetName() const override { return "KeyReleased"; }
};

	class KeyTypedEvent : public KeyEvent
	{
	public:
		KeyTypedEvent(const KeyCode keycode)
			: KeyEvent(keycode) {}

std::string ToString() const override
{
std::stringstream ss;
ss << "KeyTypedEvent: " << static_cast<int>(m_KeyCode);
return ss.str();
}

std::type_index GetType() const override { return typeid(KeyTypedEvent); }
static std::type_index GetStaticType() { return typeid(KeyTypedEvent); }
const char* GetName() const override { return "KeyTyped"; }
};
}
