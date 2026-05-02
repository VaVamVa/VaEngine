#pragma once

#include <cstdint>

class IFence
{
public:
	virtual ~IFence() = default;

	virtual void		Signal(uint64_t value) = 0;
	virtual void		Wait(uint64_t value) = 0;
	virtual uint64_t	GetCompletedValue() const = 0;
};