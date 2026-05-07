#pragma once

#include "RHI/Texture/ITexture.h"
#include "RHI/DirectX/Common_DirectX.h"

class Texture_DirectX : public ITexture
{
public:
	void LoadFromFile(IRenderDevice* device, const wchar_t* path) override;
	void LoadFromMemory(IRenderDevice* device, const void* pixels, uint32_t width, uint32_t height) override;
	void Bind(ICommandList* cmdList, uint32_t slot) override;

private:
	void Upload(ID3D12Device* device, const void* pixels, uint32_t width, uint32_t height);

	ComPtr<ID3D12Resource>       textureResource;
	ComPtr<ID3D12DescriptorHeap> srvHeap;
};
