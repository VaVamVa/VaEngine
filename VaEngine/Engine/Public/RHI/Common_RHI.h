#pragma once

#include <cstdint>

struct NativeDisplayInfo
{
	enum class EPlatform { None, Windows, Android, iOS, Max };

	EPlatform		platform;
	
	union
	{
		// HWND(Windows) || ANativeWindow(Android)
		void*		Handle;
		uintptr_t	RawData;
	};

	// HINSTANCE(Windows) || nullptr(Android)
	void*			Display;
};

enum class EPixelFormat : uint32_t
{
	Unknown					= 0,
	R32G32B32A32_FLOAT		= 2,
	R32G32B32_FLOAT			= 6,
	R32G32_FLOAT			= 16,
	R32_FLOAT				= 41,
	R8G8B8A8_UNORM			= 28,
	D24_UNORM_S8_UINT		= 45,
	B8G8R8A8_UNORM			= 87,
	Max
};

struct SwapChainDesc
{
	NativeDisplayInfo		displayInfo;
	class ICommandQueue*	RegisteredQueue;
	uint32_t				bufferCount;
	uint32_t				width;
	uint32_t				height;
	EPixelFormat			pixelFormat;
	bool					bIsWindowed;
};

// GPU가 명령을 처리하는 방식에 대한 분류
enum class ECommandQueueType : uint32_t
{
	None		= 0,
	Graphics	= 1,
	Compute		= 2,
	Copy		= 3,
	Max
};

struct CommandQueueDesc
{
	ECommandQueueType type;
	uint32_t flags;
};

struct CommandListDesc
{
	ECommandQueueType type;
	uint32_t flags;
};

struct CommandAllocDesc
{
	ECommandQueueType type;
	uint32_t flags;
};

enum class EResourceState : uint32_t
{
	Common						= 0,
	VertexBuffer				= 1 << 0,
	IndexBuffer					= 1 << 1,
	RenderTarget				= 1 << 2,
	UnorderedAccess				= 1 << 3,
	DepthWrite					= 1 << 4,
	DepthRead					= 1 << 5,
	NonPixelShaderResource		= 1 << 6,
	PixelShaderResource			= 1 << 7,
	StreamOut					= 1 << 8,
	IndirectArgument			= 1 << 9,
	CopyDest					= 1 << 10,
	CopySource					= 1 << 11,
	ResolveDest					= 1 << 12,
	ResolveSource				= 1 << 13,
	RaytracingAcceleration		= 1 << 14,
	Present						= 1 << 15
};

extern "C++" {
	inline EResourceState operator | (EResourceState f, EResourceState b) { return EResourceState(static_cast<uint32_t>(f) | static_cast<uint32_t>(b)); }
	inline EResourceState operator |= (EResourceState & f, EResourceState b) { f = EResourceState(static_cast<uint32_t>(f) | static_cast<uint32_t>(b)); return f; }
	inline EResourceState operator & (EResourceState f, EResourceState b) { return EResourceState(static_cast<uint32_t>(f) & static_cast<uint32_t>(b)); }
	inline EResourceState operator &= (EResourceState& f, EResourceState b) { f = EResourceState(static_cast<uint32_t>(f) & static_cast<uint32_t>(b)); return f; }
}

enum class EResourceViewType : uint32_t
{
	Unknown = 0,
	RenderTargetView,		// RTV(DX12), Attachment(Vulkan)
	DepthStencilView,		// DSV(DX12)
	ShaderResourceView,		// SRV(DX12), SampledImage/UniformBuffer(Vulkan)
	UnorderedAccessView,	// UAV(DX12), StorageImage/StorageBuffer(Vulkan)
	Max
};

struct ResourceViewDesc
{
	EResourceViewType type	= EResourceViewType::Unknown;
	uint32_t mipLevels		= 0;
	uint32_t arraySize		= 0;
};


struct ResourceBarrier
{
	// TODO: Transition, Aliasing, UAV 등 다양한 Barrier 유형을 지원하기 위해 구조체 확장 필요
	class IRHIResource*		resource;
	// ResourceBarrier는 명령 리스트에 기록되는 시점에 리소스의 상태가 beforeState에서 afterState로 전환된다고 가정합니다.
	EResourceState			beforeState;
	// afterState는 beforeState와 다른 상태여야 합니다. (예: RenderTarget -> Present)
	EResourceState			afterState;
};

// GPU 버퍼의 용도
enum class EBufferUsage : uint32_t
{
	None			= 0,
	VertexBuffer	= 1 << 0,
	IndexBuffer		= 1 << 1,
	ConstantBuffer	= 1 << 2,
	UnorderedAccess	= 1 << 3,
};

// CPU/GPU 메모리 접근 방식
enum class EMemoryAccess : uint32_t
{
	Default		= 0,	// GPU 전용 (D3D12_HEAP_TYPE_DEFAULT / Device-local)
	Upload		= 1,	// CPU 쓰기, GPU 읽기 (D3D12_HEAP_TYPE_UPLOAD / HOST_VISIBLE)
	Readback	= 2,	// GPU 쓰기, CPU 읽기 (D3D12_HEAP_TYPE_READBACK)
};

struct ResourceBufferDesc
{
	uint64_t		size	= 0;
	EBufferUsage	usage	= EBufferUsage::None;
	EMemoryAccess	access	= EMemoryAccess::Default;
	uint32_t		stride	= 0;	// 정점 크기(VertexBuffer) 또는 원소 크기
};

enum class EIndexFormat : uint32_t
{
	UInt16 = 0,
	UInt32 = 1,
};