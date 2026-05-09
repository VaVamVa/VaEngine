#pragma once

struct ShaderDesc
{
	const wchar_t* vsPath  = nullptr;
	const wchar_t* psPath  = nullptr;
	const char*    vsEntry = "VSMain";
	const char*    psEntry = "PSMain";
};

class IShader
{
public:
	virtual ~IShader() = default;
};
