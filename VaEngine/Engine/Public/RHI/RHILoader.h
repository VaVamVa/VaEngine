#pragma once

#include <memory>
#include "RHI/IRenderDevice.h"

namespace RHI
{
	std::unique_ptr<IRenderDevice> CreateRenderDevice();
}