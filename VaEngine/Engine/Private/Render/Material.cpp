#include "Render/Material.h"
#include "RHI/IRenderDevice.h"
#include "RHI/Common_RHI.h"

static uint16_t s_nextMaterialID = 0;

Material::Material()
    : id(s_nextMaterialID++)
{
}

void Material::Initialize(IRenderDevice* device)
{
    BufferDesc desc;
    desc.size   = sizeof(MaterialData);
    desc.usage  = EBufferUsage::ConstantBuffer;
    desc.access = EMemoryAccess::Upload;
    buffer = device->CreateBuffer(desc);
    dirty = true;
}

void Material::UpdateBufferIfDirty()
{
    if (!dirty || !buffer) return;
    buffer->Upload(&data, sizeof(MaterialData));
    dirty = false;
}

void Material::SetAlbedo(float r, float g, float b, float a)
{
    data.albedo[0] = r; data.albedo[1] = g; data.albedo[2] = b; data.albedo[3] = a;
    dirty = true;
}

void Material::SetEmissive(float r, float g, float b)
{
    data.emissive[0] = r; data.emissive[1] = g; data.emissive[2] = b;
    dirty = true;
}

void Material::SetRoughness(float v) { data.roughness = v; dirty = true; }
void Material::SetMetallic (float v) { data.metallic  = v; dirty = true; }
void Material::SetAO       (float v) { data.ao        = v; dirty = true; }
