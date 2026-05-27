#pragma once

#include "RHI/IRHIResource.h"

#include <cstring>

class ICommandList;

class IBuffer : public IRHIResource
{
public:
	virtual ~IBuffer() = default;

	virtual void* Map()   = 0;
	virtual void  Unmap() = 0;

	void Upload(const void* data, size_t size)
	{
		void* ptr = Map();
		std::memcpy(ptr, data, size);
		Unmap();
	}
};
