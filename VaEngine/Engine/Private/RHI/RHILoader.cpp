#include "RHI/RHILoader.h"
#include <stdexcept>

#if USE_DEBUGGING
	#include "DirectX/RenderDevice_DirectX.h"
	#include "Vulkan/RenderDevice_Vulkan.h"

#elif USE_DIRECTX
	#include "DirectX/RenderDevice_DirectX.h"

#elif USE_VULKAN
	#include "Vulkan/RenderDevice_Vulkan.h"

// TODO: Add other RHI headers here
// #elif USE_METAL
// #include "Metal/RenderDevice_Metal.h"

#endif


std::unique_ptr<IRenderDevice> RHI::CreateRenderDevice()
{
#if USE_DEBUGGING
#pragma message("RHI Debugging. Can't Execute Engine")
	{ [[maybe_unused]] auto device = std::make_unique<RenderDevice_DirectX>(); }
	{ [[maybe_unused]] auto device = std::make_unique<RenderDevice_Vulkan>(); }

#elif USE_DIRECTX
	return std::make_unique<RenderDevice_DirectX>();

#elif USE_VULKAN
	return std::make_unique<RenderDevice_Vulkan>();

//TODO: Add other RHI implementations here
//#elif USE_METAL

#endif

	return nullptr;
}