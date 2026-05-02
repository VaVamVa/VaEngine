#pragma once

#include "Interfaces/IExecute.h"

class Execute : public IExecute {
public:
    void OnInitialize(FNativeDisplayInfo displayInfo) override;
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
};