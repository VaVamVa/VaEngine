#include "RHI/RHILoader.h"

#if USE_DX12
	#include "DirectX/RenderDevice_DirectX.h"
#elif USE_VULKAN
	#include "Vulkan/RenderDevice_Vulkan.h"
// TODO: Add other RHI headers here
#endif

std::unique_ptr<IRenderDevice> RHI::CreateRenderDevice()
{
#if USE_DX12
	return std::make_unique<RenderDevice_DirectX>();
#elif USE_VULKAN
	return std::make_unique<RenderDevice_Vulkan>();

//TODO: Add other RHI implementations here
#else
	return nullptr;
#endif
}