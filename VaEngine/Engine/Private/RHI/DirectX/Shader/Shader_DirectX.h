#pragma once

#include "RHI/Shader/IShader.h"
#include "RHI/DirectX/Common_DirectX.h"

class Shader_DirectX : public IShader
{
public:
	void      Load(const ShaderDesc& desc);
	ID3DBlob* GetVS() const { return vs.Get(); }
	ID3DBlob* GetPS() const { return ps.Get(); }

private:
	ComPtr<ID3DBlob> vs;
	ComPtr<ID3DBlob> ps;
};
