#pragma once

#include "Common_RHI.h"

// value type handle을 넘기는 rtv 특성상 공통 Interface인 Register은 사용 불가.
// 각 생성자에서 필요한 정보를 직접 받아서 초기화하도록 구현합니다.
class IResourceView
{
public:
	virtual ~IResourceView() = default;

	virtual			IRHIResource*		GetResource() const = 0;
	virtual			EResourceViewType	GetType() const = 0;
	virtual const	ResourceViewDesc&	GetDesc() const = 0;
};