#pragma once

#include "Interfaces/IExecute.h"

class Execute : public IExecute {
public:
    void OnInitialize(NativeDisplayInfo displayInfo) override;
    void OnDestroy() override;

    void OnLoop() override;
    void OnSuspend() override;
    void OnResume() override;
    
protected:
    void OnUpdate() override;
    void OnRender() override;
    void OnPostRender() override;
    
private:
    HWND hWnd;
    HINSTANCE hInstance;

	std::unique_ptr<class IRenderDevice>    renderDevice;
	std::unique_ptr<class ICommandQueue>    commandQueue;
	std::unique_ptr<class IFence>           frameFence;
	std::unique_ptr<class ISwapChain>       swapChain;
	std::unique_ptr<class ICommandList>     commandList;
};