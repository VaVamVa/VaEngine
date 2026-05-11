#pragma once

#include "RHI/Common_RHI.h"

class IShader;
class IBindingLayout;

// Compute PSO 생성 서술자.
// Graphics PSO와 달리 정점입력/RTV/DSV/Topology 등이 없으므로 별도 구조체로 둔다.
struct ComputePipelineStateDesc
{
	IShader*        shader        = nullptr;
	IBindingLayout* bindingLayout = nullptr;  // isCompute=true 로 생성된 BindingLayout이어야 함
};
