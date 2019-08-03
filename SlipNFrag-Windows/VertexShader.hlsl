cbuffer ModelViewProjectionConstantBuffer : register(b0)
{
	matrix modelViewProjection;
};

struct VertexShaderInput
{
	float3 position : POSITION;
	float2 texCoords : TEXCOORD0;
};

struct PixelShaderInput
{
	float4 position : SV_POSITION;
	float2 texCoords : TEXCOORD0;
};

PixelShaderInput main(VertexShaderInput input)
{
	PixelShaderInput output;
	float4 position = float4(input.position, 1.0f);
	position = mul(position, modelViewProjection);
	output.position = position;
	output.texCoords = input.texCoords;
	return output;
}
