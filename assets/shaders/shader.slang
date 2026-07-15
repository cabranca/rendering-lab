struct VertexInput {
	float3 inPosition;
	float2 inTexCoords;
}

struct UniformBuffer {
	float4x4 model;
	float4x4 view;
	float4x4 proj;
};

struct VertexOutput {
	float4 pos : SV_Position;
	float2 texCoords;
};

ConstantBuffer<UniformBuffer> ubo;
Sampler2D texture;

[shader("vertex")]
VertexOutput vertMain(VertexInput input) {
	VertexOutput output;
	output.pos = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.inPosition, 1.0))));
	output.texCoords = input.inTexCoords;
	return output;
}

[shader("fragment")]
float4 fragMain(VertexOutput inVert) : SV_Target {
	return texture.Sample(inVert.texCoords);
}
