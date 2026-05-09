#pragma once

#include <cassert>

template<typename T>
class Locator
{
public:
	static	void	Register(T* instance)	{ Locator::instance = instance; }
	static	void	Unregister()			{ Locator::instance = nullptr;	}
	static	T&		Get() 					{ assert(instance && typeid(T).name()); return *instance; }

private:
	inline static T* instance = nullptr;
};