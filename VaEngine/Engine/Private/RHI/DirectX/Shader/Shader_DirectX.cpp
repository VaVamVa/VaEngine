#include "Shader_DirectX.h"
#include "Utilities/BinaryLoader.h"

#include <d3dcompiler.h>
#include <cstring>
#include <stdexcept>

void Shader_DirectX::Load(const ShaderDesc& desc)
{
	auto loadBlob = [](const wchar_t* path, ComPtr<ID3DBlob>& blob)
	{
		std::vector<uint8_t> data = LoadBinaryFile(path);
		if (FAILED(D3DCreateBlob(data.size(), &blob)))
			throw std::runtime_error("D3DCreateBlob failed");
		std::memcpy(blob->GetBufferPointer(), data.data(), data.size());
	};

	// csPath가 채워져 있으면 compute 셰이더로 간주, 그 외에는 graphics (vs+ps) 셰이더로 로드
	if (desc.csPath != nullptr)
	{
		loadBlob(desc.csPath, cs);
		return;
	}

	if (desc.vsPath == nullptr || desc.psPath == nullptr)
		throw std::runtime_error("ShaderDesc: graphics shader requires both vsPath and psPath");

	loadBlob(desc.vsPath, vs);
	loadBlob(desc.psPath, ps);
}
