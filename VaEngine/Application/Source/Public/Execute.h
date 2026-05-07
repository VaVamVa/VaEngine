#pragma once

#include "Interfaces/IExecute.h"
#include <memory>
#include <vector>

class IRenderDevice;
class ICommandQueue;
class IFence;
class ISwapChain;
class ICommandAlloc;
class ICommandList;

class CubeRenderer;
#include "RHI/Agent/CubeRenderer.h"

class Execute : public IExecute {
public:
    ~Execute() override;

    void OnInitialize(NativeDisplayInfo displayInfo) override;
    void OnDestroy() override;

    void OnLoop() override;
    void OnSuspend() override;
    void OnResume() override;
    
protected:
    void OnRelease() override;

    void OnPreUpdate() override;
    void OnUpdate() override;
	void OnPreRender() override;
    void OnRender() override;
    void OnPostRender() override;
    
private:
	std::unique_ptr<IRenderDevice>      renderDevice;
    std::unique_ptr<ICommandQueue>      commandQueue;
    std::unique_ptr<IFence>             frameFence;
	uint64_t                            currentFenceValue = 0;
    std::unique_ptr<ISwapChain>         swapChain;
    std::unique_ptr<ICommandAlloc>      commandAllocator;
    std::unique_ptr<ICommandList>       commandList;

	// TODO: 별도의 SceneManager 클래스를 만들어서 관리 예정
    std::unique_ptr<CubeRenderer>       cubeRenderer;
};