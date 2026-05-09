#pragma once

#include <cassert>

template<typename T>
class Singleton
{

protected:
    Singleton() = default;
    virtual ~Singleton() = default;
    virtual void InitializeInstance() {}
    virtual void DestroyInstance() {}

public:
    static T* Get()
    {
        if (prevent)
        {
			assert(prevent && "Get Singleton instance is prevented");
            return nullptr;
        }

        if (!instance)
        {
            instance = new T();
            instance->InitializeInstance();
        }
        return instance;
    }

    static void Destroy()
    {
        if (instance)
        {
            instance->DestroyInstance();
            delete instance;
            instance = nullptr;
        }
    }

    explicit Singleton(const Singleton&) = delete;
    explicit Singleton(Singleton&&) = delete;
    explicit Singleton(const T&) = delete;
    explicit Singleton(T&&) = delete;
    Singleton& operator=(const Singleton&) = delete;
    Singleton& operator=(Singleton&&) = delete;
    Singleton& operator=(const T&) = delete;
    Singleton& operator=(T&&) = delete;

private:
    inline static T* instance = nullptr;

protected:
    inline static bool prevent = false;
};

/*template<typename T>
T* Singleton<T>::instance = nullptr;
template<typename T>
bool Singleton<T>::prevent = false;*/