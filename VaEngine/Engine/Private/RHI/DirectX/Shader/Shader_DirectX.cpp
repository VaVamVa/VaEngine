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

	loadBlob(desc.vsPath, vs);
	loadBlob(desc.psPath, ps);
}
