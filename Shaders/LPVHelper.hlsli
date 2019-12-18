
static const float3 LPVSize = float3(32.0f, 16.0f, 32.0f);
static const float3 LPVTexScale = (LPVSize - float3(1.0f, 1.0f, 1.0f)) / LPVSize;
static const float3 LPVTexBias = float3(0.5f, 0.5f, 0.5f) / LPVSize;

Texture3D	InVplR : register(t12);
Texture3D	InVplG : register(t13);
Texture3D	InVplB : register(t14);

float4 SampleVpl(SamplerState InSampler, float3 WldPos, float3 InN)
{
	float3 NormalizedPos = (WldPos - SceneBoundMin.xyz) / (SceneBoundMax.xyz - SceneBoundMin.xyz) * LPVTexScale + LPVTexBias;
	if (all(NormalizedPos > 0.0f) && all(NormalizedPos < 1.0f))
	{
		float4 SH_R = InVplR.Sample(InSampler, NormalizedPos);
		float4 SH_G = InVplG.Sample(InSampler, NormalizedPos);
		float4 SH_B = InVplB.Sample(InSampler, NormalizedPos);

		float4 SH_Transfer = SHCosTransfer(InN);

		return float4(dot(SH_R, SH_Transfer), dot(SH_G, SH_Transfer), dot(SH_B, SH_Transfer), 1.0f);
	}
	else
	{
		return float4(0.0f, 0.0f, 0.0f, 0.0f);
	}
}