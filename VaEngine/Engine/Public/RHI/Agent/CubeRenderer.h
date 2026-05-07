#pragma once

#include <memory>

class IRenderDevice;
class ICommandList;

class CubeRenderer
{
public:
	virtual ~CubeRenderer() = default;

	virtual void Initialize(IRenderDevice* device) = 0;
	virtual void Draw(ICommandList* cmdList) = 0;

	static std::unique_ptr<CubeRenderer> Create();
};
