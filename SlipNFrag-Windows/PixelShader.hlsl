struct PixelShaderInput
{
	float4 position : SV_POSITION;
	float2 texCoords : TEXCOORD0;
};

Texture2D<float1> screen : register(t0);

SamplerState screenSampler : register(s0);

Texture2D<float1> console : register(t1);

SamplerState consoleSampler : register(s1);

Texture1D palette : register(t2);

SamplerState paletteSampler : register(s2);

float4 main(PixelShaderInput input) : SV_TARGET
{
	float entry = console.Gather(consoleSampler, input.texCoords).x;
	return palette.Sample(paletteSampler, (entry < 1 ? entry : screen.Gather(screenSampler, input.texCoords).x));
}
