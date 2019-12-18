#include "Common.hlsli"
#include "CommonHelper.hlsli"
#include "Utils.hlsli"

Texture2D GBufferB : register(t0);
Texture2D DepthBuffer : register(t1);
Texture2DArray	ShadowBuffers : register(t2);

SamplerState LinearSampler : register(s0);
SamplerState PointSampler : register(s1);



float4 SSAO_PS(QuadVS_Output Input) : SV_TARGET
{
	float4 Normal = GBufferB.Sample(PointSampler, Input.Uv);

	float	Depth = DepthBuffer.Sample(PointSampler, Input.Uv).r;

	float3 N = Normal.xyz*2.0f - 1.0f;
	N = normalize(N);

	float LinearZ = mProjection._m32 / (Depth - mProjection._m22);

	float3 UpDir = abs(N.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
	float3 T = normalize(cross(UpDir, N));
	float3 B = cross(N, T);

	float2 ScreenPos;
	ScreenPos.x = Input.Uv.x*2.0f - 1.0f;
	ScreenPos.y = 1.0f - Input.Uv.y*2.0f;
	ScreenPos.xy -= TemporalJitterParams.xy;

	float4 WldPos;
	float3 CameraRay = (CameraXAxis.xyz*ScreenPos.x + CameraYAxis.xyz*ScreenPos.y + CameraZAxis.xyz);

	WldPos.xyz = CameraOrigin.xyz + CameraRay*LinearZ;
	WldPos.w = 1.0f;

	const float Radius = 35.0f;

	float ScreenRadius = Radius*mProjection._m00;
	float Occluded = 0.0f;
	for (uint i = 0; i < 25; ++i)
	{
		float2	random2D = hammersley2d(i, 25);
		float	Scale = frac(sin(dot(Input.Uv + random2D, float2(12.9898f, 78.233f))) * (43758.5453f));
		float3	SampleOnHemisphere = hemisphereSample_cos(random2D.x, random2D.y);
		float3	SamplePoint = SampleOnHemisphere*Scale;

		float3 SamplePos = WldPos.xyz + N*0.05f + T*SamplePoint.x + B*SamplePoint.y + N*SamplePoint.z;
		float4 SampleProjPos = mul(float4(SamplePos, 1.0f), mViewProjection);
		SampleProjPos.xyz /= SampleProjPos.w;

		SampleProjPos.x = (1.0f + SampleProjPos.x)*0.5f;
		SampleProjPos.y = (1.0f - SampleProjPos.y)*0.5f;

		float	SampleDepth = DepthBuffer.Sample(PointSampler, SampleProjPos.xy).r;
		float LinearSampleZ = mProjection._m32 / (SampleDepth - mProjection._m22);

		if (SampleProjPos.w > LinearSampleZ)
		{
			float weight = smoothstep(0.0f, 1.0f, Radius / (SampleProjPos.w - LinearSampleZ));
			Occluded += weight;
		}
	}

	float AO = 1.0f - Occluded / 25.0f;

	float4	ViewSpacePos = mul(WldPos, mView);

	const int CascadeCount = 3;
	int		CascadeIdx = 0;
	float	ViewDepth = ViewSpacePos.z;
	for (CascadeIdx = 0; CascadeIdx < CascadeCount - 1; ++CascadeIdx)
	{
		if (ViewDepth <= CascadeShadowDistances[CascadeIdx])
		{
			break;
		}
	}

	float Shadow = GetCascadeShadow(ShadowBuffers, LinearSampler, CascadeIdx, WldPos, Input.Uv, 0.002f);

	return float4(AO, AO, Shadow, 1.0f);
}