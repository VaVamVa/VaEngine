#pragma once

#include "RHI/IRHIResource.h"
#include "RHI/Common_RHI.h"

class IRenderDevice;
class ICommandList;
class IResourceView;

class IColorBuffer : public IRHIResource
{
public:
	virtual ~IColorBuffer() = default;

	virtual void Create(IRenderDevice* device,
	                    EPixelFormat   format,
	                    uint32_t       width,
	                    uint32_t       height) = 0;

	virtual IResourceView* GetRTV() const = 0;
	virtual void BindSRV(ICommandList* cmdList, uint32_t slot, bool isCompute) = 0;
};