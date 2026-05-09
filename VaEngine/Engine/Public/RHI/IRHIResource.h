#pragma once

/*
하위 클래스에서 Template 사용 권장

*/
struct IRHIResource
{
public:
	virtual ~IRHIResource() = default;

	virtual void* GetNativeResource() const = 0;
};

#include <memory>

template<typename ResourceType>
class TRHIResource : public IRHIResource
{
public:
	TRHIResource() { resource.reset(); }
	TRHIResource(ResourceType* inResource) { resource.reset(inResource); }
	template<typename Deleter>
	TRHIResource(ResourceType* inResource, Deleter deleter) { resource.reset(inResource, deleter); }

	virtual ~TRHIResource() = default;

	virtual ResourceType* GetResource() const { return resource.get(); }
	virtual ResourceType& operator*() const { return *resource; }

	virtual void* GetNativeResource() const override
	{
		return resource.get();
	}

protected:
	std::shared_ptr<ResourceType> resource;
};