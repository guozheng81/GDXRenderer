float4 GetWorldPositionFromDepth(float Depth, float2 ScreenUV)
{
	float LinearZ = mProjection._m32 / (Depth - mProjection._m22);

	float2 ScreenPos;
	ScreenPos.x = ScreenUV.x*2.0f - 1.0f;
	ScreenPos.y = 1.0f - ScreenUV.y*2.0f;
	ScreenPos.xy -= TemporalJitterParams.xy;

	float3 CameraRay = (CameraXAxis.xyz*ScreenPos.x + CameraYAxis.xyz*ScreenPos.y + CameraZAxis.xyz);

	float4 WldPos;
	WldPos.xyz = CameraOrigin.xyz + CameraRay*LinearZ;
	WldPos.w = 1.0f;

	return WldPos;
}

int		GetCascadeIndex(float4 WldPos)
{
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
	
	return CascadeIdx;
}

float	GetCascadeShadowSimple(Texture2DArray	InShadowBuffers, SamplerState InSampler, int CascadeIdx, float4 InWldPos, float InBias)
{
	float4 ShadowProjPos = 0.0f;

	ShadowProjPos = mul(InWldPos, ShadowViewProj[CascadeIdx]);

	ShadowProjPos.x = saturate((ShadowProjPos.x + 1.0f)*0.5f);
	ShadowProjPos.y = saturate((1.0f - ShadowProjPos.y)*0.5f);

	ShadowProjPos.z -= InBias;

	float Shadow = 1.0f;
	float2 ShadowUv = ShadowProjPos.xy;
	float ShadowDepth = InShadowBuffers.SampleLevel(InSampler, float3(ShadowUv.x, ShadowUv.y, CascadeIdx), 0).r;
	if (ShadowProjPos.z  > ShadowDepth) { Shadow = 0.0f; }

	return Shadow;
}


float	GetCascadeShadow(Texture2DArray	InShadowBuffers, SamplerState InSampler, int CascadeIdx, float4 InWldPos, float2 InScreenUv, float InBias)
{
	float4 ShadowProjPos = 0.0f;

	ShadowProjPos = mul(InWldPos, ShadowViewProj[CascadeIdx]);

	ShadowProjPos.x = saturate((ShadowProjPos.x + 1.0f)*0.5f);
	ShadowProjPos.y = saturate((1.0f - ShadowProjPos.y)*0.5f);

	ShadowProjPos.z -= InBias;

	float Shadow = 0.0f;

	float2 poissonDisk[4] = {
		float2(-0.94201624f, -0.39906216f),
		float2(0.94558609f, -0.76890725f),
		float2(-0.094184101f, -0.92938870f),
		float2(0.34495938f, 0.29387760f)
	};

	float RandRotation = frac(sin(dot(InScreenUv, float2(12.9898f, 78.233f))) * (43758.5453f));
	float theta = RandRotation * 3.1415926f*2.0f;
	float2x2 randomRotationMatrix = float2x2(float2(cos(theta), -sin(theta)),
		float2(sin(theta), cos(theta)));

	for (int i = 0; i < 4; ++i)
	{
		float2 ShadowUv = ShadowProjPos.xy + mul(poissonDisk[i], randomRotationMatrix) * 1.0f / 2048.0f;

		float ShadowDepth = InShadowBuffers.SampleLevel(InSampler, float3(ShadowUv.x, ShadowUv.y, CascadeIdx), 0).r;
		if (ShadowProjPos.z  > ShadowDepth) { Shadow += 1.0f; }
	}

	return 1.0f - Shadow / 4.0f;
}

