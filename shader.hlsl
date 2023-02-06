struct VertexInput {
    float2 pos : POSITION;
    float2 uv : TEXCOORD;
    float4 col : COLOR;
};

struct PixelInput {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    float4 col : COLOR;
};

// vertex shader
cbuffer MatrixBuffer : register(b0) {
    matrix proj;
};

PixelInput VS(VertexInput input) {
    PixelInput output;
    output.pos = mul(float4(input.pos, 0.0, 1.0), proj);
    output.uv  = input.uv;
    output.col = input.col;
    return output;
}

// fragment shader

Texture2D diffuse_texture;
SamplerState Sampler0;

float4 PS(PixelInput input) : SV_Target {
    float4 colour = input.col * diffuse_texture.Sample(Sampler0, input.uv);
    // if (colour.a < 0.1)
    //    discard;
    return colour;
}
