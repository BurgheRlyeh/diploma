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

cbuffer ModelBuffers : register(b1)
{
    ModelBuffer modelBuffers[LIMIT_I - 1];
};

struct VSOutput
{
    float4 pos : SV_Position;
    nointerpolation unsigned int instId : SV_InstanceID;
};

float4 main(VSOutput pixel) : SV_TARGET {
    return modelBuffers[pixel.instId].cl;
}