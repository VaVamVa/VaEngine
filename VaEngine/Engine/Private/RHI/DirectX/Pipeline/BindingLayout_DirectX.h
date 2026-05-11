#pragma once

#include "RHI/Pipeline/IBindingLayout.h"
#include "RHI/DirectX/Common_DirectX.h"

class BindingLayout_DirectX : public IBindingLayout
{
public:
	void Create(ID3D12Device* device, const BindingEntry* entries, uint32_t count, bool isCompute = false);

	ComPtr<ID3D12RootSignature> GetRootSignature() const { return rootSignature; }
	bool                        IsCompute()        const { return isCompute; }

private:
	ComPtr<ID3D12RootSignature> rootSignature;
	bool                        isCompute = false;
};
