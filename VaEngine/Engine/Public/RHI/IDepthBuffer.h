#pragma once

class IRHIResource;
class IResourceView;

class IDepthBuffer
{
public:
    virtual ~IDepthBuffer() = default;

    virtual IRHIResource*  GetResource() const = 0;
    virtual IResourceView* GetView()     const = 0;
};
