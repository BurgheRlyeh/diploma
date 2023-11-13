struct VSOutput {
	float4 pos : SV_Position;
	float2 uv : TEXCOORD;
};

Texture2D colorTexture : register (t0);

SamplerState colorSampler : register(s0);

float4 main(VSOutput pixel) : SV_Target0{
	return colorTexture.Sample(colorSampler, pixel.uv);
}
