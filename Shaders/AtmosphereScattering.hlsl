#include "Common.hlsli"
#include "Utils.hlsli"
#include "AtmosphereScatteringHelper.hlsli"
#include "Noise.hlsli"

RWTexture3D<float4>	LightScatteringTex : register(u0);
Texture2DArray	ShadowBuffers : register(t0);

SamplerState LinearSampler : register(s0);
SamplerState PointSampler : register(s1);

cbuffer AtmosphereScatteringBuffer : register(b2)
{
	float4	CurrentFramePixel;
}

#define USE_REPROJECTION_FOR_INTEGRATION_VOL 0
#define RENDER_CLOUDSCAPES 1

float GetCloudDensity(float3 WldPos, float3 boundSize, float3 center)
{
	float3 normalizedPos = (WldPos - center) / boundSize;

	float Density = max((fBm(WldPos*0.001f, 4, 2.5f, 0.7f))*0.8f + (1.0f - sqrt(dot(normalizedPos, normalizedPos))), 0.0f);

	return Density;
}

float PowderTerm(float TotalDensity, float cosTheta)
{
	float powder = 1.0 - exp(-TotalDensity * 2.0);
	return lerp(1.0, powder, smoothstep(0.5, -0.5, cosTheta));
}

static const float	JitterSamples[4][4] = { { 0.0f, 0.5f, 0.125f, 0.625f } ,
{ 0.75f, 0.25f, 0.875f, 0.375f },
{ 0.1875f, 0.6875f, 0.0625f, 0.5625 } ,
{ 0.9375f, 0.4375f, 0.8125f, 0.3125 } };

[numthreads(8, 8, 1)]
void LightScattering(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
#if USE_REPROJECTION_FOR_INTEGRATION_VOL
	DTid.xy *= 2;
	uint2 currentSampleOffset = (uint2)(CurrentFramePixel.yz);
	DTid.xy += currentSampleOffset;
#endif

	if (any(DTid.xy >= uint2(FrustumSize.xy)))
	{
		return;
	}

	float	JitterValue = JitterSamples[DTid.x % 4][DTid.y % 4];

	float4 WldPos = GetWorldPosByFrustumPos(DTid, JitterValue);

	float3 V = CameraOrigin.xyz - WldPos.xyz;
	V = normalize(V);

	float3	L = normalize(LightParams.xyz);

	float VoL = dot(V, L);
//	float PhaseScattering = PhaseFunction(0.2f, VoL);
	float PhaseScattering = TwoLobePhaseFunction(0.8f, -0.5f, VoL);

	float3	DirLightColor = float3(1.0f, 1.0f, 1.0f)*LightingControl.x;

	float3	LightScattering = float3(0.0f, 0.0f, 0.0f);
	float	Density = 0.0f;
	float	scatterCoefficient = 0.1f;
	float	absorptionCoefficient = 0.2f;
	float	LightExtinction = 0.0f;

	if (all(WldPos.xyz >= SceneBoundMin.xyz) && all(WldPos.xyz <= SceneBoundMax.xyz))
	{
		int	CascadeIdx = GetCascadeIndex(WldPos);
		float Shadow = GetCascadeShadowSimple(ShadowBuffers, PointSampler, CascadeIdx, WldPos, 0.002f);

		Density = 0.004f;
		LightScattering = DirLightColor*PhaseScattering*Density*scatterCoefficient*Shadow;
		LightExtinction = Density*absorptionCoefficient*Shadow;
	}
	else
	{
		scatterCoefficient = 0.3f;
		absorptionCoefficient = 0.02f;

		float3 boundSize = SceneBoundMax.xyz - SceneBoundMin.xyz;

#if RENDER_CLOUDSCAPES
		float2	heightMinMax = float2(SceneBoundMax.y + 50.0f, SceneBoundMax.y + boundSize.y*1.5f);
		Density = GetCloudDensity_HZD(WldPos.xyz, heightMinMax);
#else
		float3 center = (SceneBoundMin.xyz + SceneBoundMax.xyz)*0.5f;
		center.y += boundSize.y;
		boundSize *= (0.8f*0.5f);		// scale down a a bit

		Density = GetCloudDensity(WldPos.xyz, boundSize, center);
#endif

		if (Density > 0.0001f)
		{
			const int maxStep = 10;
			const float stepSize = 60.0f;
			float	TotalDensity = 0.0f;

			float T = 1.0f;
			for (int i = 0; i < maxStep && T > 0.00001f; i++)
			{
				float3 CurPos = WldPos.xyz + L.xyz*i*stepSize;
#if RENDER_CLOUDSCAPES
				float SampleDensity = GetCloudDensity_HZD(CurPos, heightMinMax);
#else
				float SampleDensity = GetCloudDensity(CurPos, boundSize, center);
#endif

				if (SampleDensity > 0.0001f)
				{
					TotalDensity += SampleDensity*stepSize;
					float T_i = exp(-absorptionCoefficient*SampleDensity*stepSize);
					T *= T_i;
				}
			}

			LightScattering = DirLightColor*PhaseScattering*Density*scatterCoefficient*T*PowderTerm(TotalDensity, VoL);
			LightExtinction = Density*absorptionCoefficient;
		}
	}

	LightScatteringTex[DTid] = float4(LightScattering, LightExtinction);
}

Texture3D<float4> InLightScatteringTex              : register(t0);
RWTexture3D<float4> OutLightIntegrationTex              : register(u0);

[numthreads(8, 8, 1)]
void LightIntegration(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
#if USE_REPROJECTION_FOR_INTEGRATION_VOL
	DTid.xy *= 2;
	uint2 currentSampleOffset = (uint2)(CurrentFramePixel.yz);
	DTid.xy += currentSampleOffset;
#endif

	if (any(DTid.xy >= uint2(FrustumSize.xy)))
	{
		return;
	}

	float T = 1.0f;
	float preZ = 0.0f;
	float4	FinalColor = float4(0.0f, 0.0f, 0.0f, 1.0f);

	float	JitterValue = JitterSamples[DTid.x % 4][DTid.y % 4];

	for (uint i = 0; i < (uint)(FrustumSize.z); i++)
	{
		float3	NormalizedPos = float3(DTid.x, DTid.y, ((float)(i)+JitterValue))/ FrustumSize;
		NormalizedPos.z = saturate(NormalizedPos.z);
		NormalizedPos *= FrustumTexScale;
		NormalizedPos += FrustumTexBias;

		float z = pow(NormalizedPos.z, Z_SLICE_POW)*SCATTERING_Z_RANGE;

		float stepSize = z - preZ;
		preZ = z;

		{
			float4 LightScattering = InLightScatteringTex.SampleLevel(LinearSampler, NormalizedPos, 0);
			float	Absorption = LightScattering.w;

			if (Absorption > 0.0f)
			{
				float T_i = exp(-Absorption*stepSize);
				T *= T_i;

				//FinalColor.rgb += T*stepSize*LightScattering.rgb;
				FinalColor.rgb += T*LightScattering.rgb*(1.0f - T_i) / Absorption;
				FinalColor.a = T;

			}
		}

		OutLightIntegrationTex[uint3(DTid.xy, i)] = FinalColor;
	}
}

Texture3D<float4> InLightIntegrationTex              : register(t0);
Texture2D DepthBuffer : register(t1);

Texture3D<float4> InLightIntegrationHistory0Tex              : register(t1);
RWTexture3D<float4> OutLightIntegrationHistory1Tex              : register(u0);

[numthreads(8, 8, 1)]
void LightIntegrationTemporal(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	if (any(DTid.xy >= uint2(FrustumSize.xy)))
	{
		return;
	}

	uint2 currentSampleOffset = (uint2)(CurrentFramePixel.yz);
	float4 WldPos = GetWorldPosByFrustumPos(DTid, 0.0f);

	float4 PreProjectionPos = mul(WldPos, mPreViewProjection);
	PreProjectionPos.xy /= PreProjectionPos.w;
	float2 PreScreenPos = float2(PreProjectionPos.x + 1.0f, 1.0f - PreProjectionPos.y)*0.5f;
	float	PreLinearZ = PreProjectionPos.w;
	float	PreNormalizedZ = pow(saturate(PreLinearZ / SCATTERING_Z_RANGE), 1.0/ Z_SLICE_POW);
	float3	PreNormalizedPos = float3(PreScreenPos, PreNormalizedZ);
	PreNormalizedPos -= FrustumTexBias;
	PreNormalizedPos /= FrustumTexScale;

	if (DTid.x % 2 == currentSampleOffset.x && DTid.y % 2 == currentSampleOffset.y)
	{
		OutLightIntegrationHistory1Tex[DTid] = InLightIntegrationTex[DTid];
	}
	else
	{
		OutLightIntegrationHistory1Tex[DTid] = InLightIntegrationHistory0Tex.SampleLevel(LinearSampler, PreNormalizedPos, 0);
	}

}

/*
// use integration texture
float4 ApplyScatteringPs(QuadVS_Output Input) : SV_TARGET
{
	float2 UnjitteredUv = Input.Uv;
	UnjitteredUv.x -= TemporalJitterParams.x*0.5f;
	UnjitteredUv.y += TemporalJitterParams.y*0.5f;

	float	Depth = DepthBuffer.Sample(PointSampler, UnjitteredUv).r;
	float LinearZ = mProjection._m32 / (Depth - mProjection._m22);
	float3 NormalizedPos = float3(UnjitteredUv, saturate(pow(LinearZ / SCATTERING_Z_RANGE, 1.0f/Z_SLICE_POW)));
	float4 LightIntegration = InLightIntegrationTex.SampleLevel(LinearSampler, NormalizedPos, 0);
	return LightIntegration;
}
*/

float4 ApplyScatteringPs(QuadVS_Output Input) : SV_TARGET
{
	float2 UnjitteredUv = Input.Uv;
	UnjitteredUv.x -= TemporalJitterParams.x*0.5f;
	UnjitteredUv.y += TemporalJitterParams.y*0.5f;

	float4 FinalColor = float4(0.0f, 0.0f, 0.0f, 0.0f);

	float3	StartPos = CameraOrigin.xyz;

	int maxStep = (int)(FrustumSize.z);
	//	const float stepSize = 2.5f;

	float	Depth = DepthBuffer.Sample(PointSampler, Input.Uv).r;
	float LinearZ = mProjection._m32 / (Depth - mProjection._m22);

	float T = 1.0f;
	float preZ = 0.0f;

	uint2	GridPos = (uint2)(UnjitteredUv*(ViewSizeAndInv.xy / 4.0f));
	float	JitterValue = JitterSamples[GridPos.x % 4][GridPos.y % 4];

	for (int i = 0; i < maxStep && T > 0.00001f; i++)
	{
		float3	NormalizedPos = float3(UnjitteredUv, saturate(((float)i+JitterValue)/ FrustumSize.z));
		NormalizedPos *= FrustumTexScale;
		NormalizedPos += FrustumTexBias;

		float z = pow(NormalizedPos.z, Z_SLICE_POW)*SCATTERING_Z_RANGE;
		//float z = (exp2((1.0f-NormalizedPos.z)*(-10.0f)) - exp2(-10.0f)) / (1.0f - exp2(-10.0f))*SCATTERING_Z_RANGE;

		float stepSize = z - preZ;
		preZ = z;

		if (z > SCATTERING_Z_RANGE || z > LinearZ)
		{
			break;
		}

		float4 LightScattering = InLightScatteringTex.SampleLevel(LinearSampler, NormalizedPos, 0);
		float	Absorption = LightScattering.w;

		if (Absorption > 0.0f)
		{
			float T_i = exp(-Absorption*stepSize);
			T *= T_i;

			//FinalColor.rgb += T*stepSize*LightScattering.rgb;
			FinalColor.rgb += T*LightScattering.rgb*(1.0f - T_i) / Absorption;

			//FinalColor.a += (1.0f - FinalColor.a)*(1.0f - T_i);
		}
	}

	FinalColor = float4(FinalColor.rgb, T);
	return FinalColor;

}
