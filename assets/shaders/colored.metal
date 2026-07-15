#include <metal_stdlib>

using namespace metal;

struct VertexIn {
    float2 position;
    float2 texCoords;
};

struct VertexOut {
    float4 position [[position]];
    float2 texCoords;
};

vertex VertexOut vertexShader(uint vertexID [[vertex_id]], constant VertexIn* vertexData [[buffer(0)]]) {
    VertexOut out;
    out.position = float4(vertexData[vertexID].position, 0.0, 1.0);
    out.texCoords = vertexData[vertexID].texCoords;

    return out;
}

fragment float4 fragmentShader(VertexOut vertexData [[stage_in]], texture2d<half> colorTexture [[ texture(0) ]]) {
    constexpr sampler textureSampler (mag_filter::linear, min_filter::linear);
    const half4 colorSample = colorTexture.sample(textureSampler, vertexData.texCoords);
    return float4(colorSample);
}
