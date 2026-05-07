#pragma once

#include <cstddef>
#include "RHI/Common_RHI.h"

class IRHIResource;

class IResourceBuffer
{
public:
	virtual ~IResourceBuffer() = default;

	virtual void Upload(const void* data, size_t size) = 0;
	virtual IRHIResource* GetResource() const = 0;
};
