#pragma once

#include "RHI/Pipeline/IBindingLayout.h"
#include "RHI/DirectX/Common_DirectX.h"

class BindingLayout_DirectX : public IBindingLayout
{
public:
	void Create(ID3D12Device* device, const BindingEntry* entries, uint32_t count);

	ComPtr<ID3D12RootSignature> GetRootSignature() const { return rootSignature; }

private:
	ComPtr<ID3D12RootSignature> rootSignature;
};
