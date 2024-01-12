struct ModelBuffer {
    float4 bbmin;
    float4 bbmax;
    float4 cl;
};

StructuredBuffer<ModelBuffer> modelBuffers : register(t0);

struct VSOutput {
    float4 pos : SV_Position;
    nointerpolation unsigned int instId : SV_InstanceID;
};

float4 main(VSOutput pixel) : SV_TARGET {
    return modelBuffers[pixel.instId].cl;
}