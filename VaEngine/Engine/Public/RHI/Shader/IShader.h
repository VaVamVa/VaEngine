#pragma once

struct ShaderDesc
{
	// graphics 셰이더 — 기존 4개 필드 위치 유지 (positional aggregate init 호환)
	const wchar_t* vsPath  = nullptr;
	const wchar_t* psPath  = nullptr;
	const char*    vsEntry = "VSMain";
	const char*    psEntry = "PSMain";

	// compute 셰이더 — csPath 채워 넣으면 compute 모드로 간주
	const wchar_t* csPath  = nullptr;
	const char*    csEntry = "CSMain";
};

class IShader
{
public:
	virtual ~IShader() = default;
};
