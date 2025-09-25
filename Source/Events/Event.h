#pragma once

#include <functional>
#include <string>
#include <typeindex>

namespace AstralEngine
{
enum EventCategory
{
None = 0,
EventCategoryApplication = 1 << 0,
EventCategoryInput = 1 << 1,
EventCategoryKeyboard = 1 << 2,
EventCategoryMouse = 1 << 3,
EventCategoryMouseButton = 1 << 4
};

class Event
{
public:
    virtual ~Event() = default;

    bool Handled = false;

    // Use std::type_index for runtime type identification. Derived events should implement GetType()
    virtual std::type_index GetType() const = 0;
    virtual const char* GetName() const = 0;
    virtual int GetCategoryFlags() const = 0;
    virtual std::string ToString() const { return GetName(); }

    bool IsInCategory(EventCategory category)
    {
        return GetCategoryFlags() & category;
    }
};

	class EventDispatcher
	{
	public:
		EventDispatcher(Event& event)
			: m_Event(event)
		{
		}

template<typename T, typename F>
bool Dispatch(const F& func)
{
    if (m_Event.GetType() == typeid(T))
    {
        m_Event.Handled |= func(static_cast<T&>(m_Event));
        return true;
    }
    return false;
}
	private:
		Event& m_Event;
	};

	inline std::ostream& operator<<(std::ostream& os, const Event& e)
	{
		return os << e.ToString();
	}
}
