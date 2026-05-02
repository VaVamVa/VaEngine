#pragma once

#include <memory>
#include "Interfaces/IRenderDevice.h"

namespace RHI
{
	std::unique_ptr<IRenderDevice> CreateRenderDevice();
}