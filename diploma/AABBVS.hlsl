#define LIMIT_V 1013
#define LIMIT_I 1107

cbuffer SceneBuffer : register(b0)
{
    float4x4 vp;
    float4 cameraPos;
};

struct ModelBuffer
{
    float4 bbmin;
    float4 bbmax;
    float4 cl;
};

cbuffer ModelBuffers : register(b1) {
    ModelBuffer modelBuffers[LIMIT_I - 1];
};

struct VSInput {
    float3 pos : POSITION;
    unsigned int instId : SV_InstanceID;
};

struct VSOutput {
    float4 pos : SV_Position;
    nointerpolation unsigned int instId : SV_InstanceID;
};

VSOutput main(VSInput vtx) {
    ModelBuffer mb = modelBuffers[vtx.instId];
    
    VSOutput res;
    
    res.pos.x = vtx.pos.x ? mb.bbmax.x : mb.bbmin.x;
    res.pos.y = vtx.pos.y ? mb.bbmax.y : mb.bbmin.y;
    res.pos.z = vtx.pos.z ? mb.bbmax.z : mb.bbmin.z;
    res.pos.w = 1.f;
    
    res.pos = mul(vp, res.pos);
    
    res.instId = vtx.instId;
    
    return res;
}