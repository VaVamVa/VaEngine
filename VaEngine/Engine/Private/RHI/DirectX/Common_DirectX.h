#pragma once

#include <cassert>
#include <stdexcept>
#include <cstdint>

#include "RHI/IRHIResource.h"

// ImmediateSubmit 내부에서 로컬 ID3D12Resource*를 IRHIResource*로 임시 래핑할 때 사용.
struct RawResource final : IRHIResource
{
    explicit RawResource(void* p) : ptr(p) {}
    void* GetNativeResource() const override { return ptr; }
    void* ptr;
};

// ComPtr(WindowsRuntimeLibrary)
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

// IDXGIFactory6, IDXGIAdapter4
#include <dxgi1_6.h>

// ID3D12Device, ID3D12CommandQueue, ID3D12CommandAllocator, ID3D12GraphicsCommandList, D3D12_RESOURCE_BARRIER, D3D12_VIEWPORT, D3D12_RECT
#include <directx/d3d12.h>

// D3D12 helper library
#include <directx/d3dx12.h>
