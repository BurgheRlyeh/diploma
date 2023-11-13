struct VSInput {
	unsigned int vertexId : SV_VertexID;
};

struct VSOutput {
	float4 pos : SV_Position;
	float2 uv : TEXCOORD;
};

VSOutput main(VSInput vertex) {
	VSOutput result;

	result.pos = float4(
		vertex.vertexId == 1 ? 3 : -1,
		vertex.vertexId == 2 ? -3 : 1,
		0,
		1
	);
	result.uv = float2(
		result.pos.x * 0.5 + 0.5,
		0.5 - result.pos.y * 0.5
	);

	return result;
}
