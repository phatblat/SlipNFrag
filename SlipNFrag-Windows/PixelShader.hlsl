struct PixelShaderInput
{
	float4 position : SV_POSITION;
	float2 texCoords : TEXCOORD0;
};

Texture2D<float1> screen : register(t0);

SamplerState screenSampler : register(s0);

Texture1D colorTable : register(t1);

SamplerState colorTableSampler : register(s1);

float4 main(PixelShaderInput input) : SV_TARGET
{
	return colorTable.Sample(colorTableSampler, screen.Gather(screenSampler, input.texCoords).x);
}
