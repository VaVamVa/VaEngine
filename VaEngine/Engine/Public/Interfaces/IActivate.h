#pragma once

class IActivate
{
public:
    virtual ~IActivate() = default;
    virtual void SetEnabled(bool enabled) = 0;
    virtual bool IsEnabled() const = 0;
};
