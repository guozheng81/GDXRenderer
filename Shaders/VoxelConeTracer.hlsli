
// need match c++ code
static const float3 VCTVolumeSize = float3(256.0f, 128.0f, 256.0f);
static const float3 VoxelTexScale = (VCTVolumeSize - float3(1.0f, 1.0f, 1.0f)) / VCTVolumeSize;
static const float3 VexelTexBias = float3(0.5f, 0.5f, 0.5f) / VCTVolumeSize;

Texture3D<float4> InputVoxelTexture : register(t8);
Texture3D<float4> InputVoxelTexture1 : register(t9);
Texture3D<float4> InputVoxelTexture2 : register(t10);
Texture3D<float4> InputVoxelTexture3 : register(t11);


float4 TraceVoxelCone(float4 InWldPos, float3 InTraceDir, float InMaxDistance, float ConeDiameterDistanceRatio, float VoxelSize, SamplerState InSampler)
{
	float4 AccumulatedColor = float4(0.0f, 0.0f, 0.0f, 1.0f);

	float Dist = VoxelSize/2.0f;
	float StepSize = VoxelSize;
	int	 level = 0;
	while (Dist < InMaxDistance && AccumulatedColor.a > 0.00001f)
	{
		float3	CurPos = InWldPos.xyz + InTraceDir*Dist;
		float3 NormalizedPos = (CurPos.xyz - SceneBoundMin.xyz) / (SceneBoundMax.xyz - SceneBoundMin.xyz) * VoxelTexScale + VexelTexBias;

		float CurDiameter = ConeDiameterDistanceRatio*Dist;
		// skip level 0 volume since it is not filtered, and only has 12 cone samples over hemisphere, for indirect diffuse lighting it is not smooth enough
		level = clamp((int)log2(CurDiameter / VoxelSize), 1, 4) -1;		
		StepSize = VoxelSize*exp2(level)*1.414f;

		float4 SampledColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
		if (level == 0)
		{
			SampledColor = InputVoxelTexture.SampleLevel(InSampler, NormalizedPos, 0);
		}
		if (level == 1)
		{
			SampledColor = InputVoxelTexture1.SampleLevel(InSampler, NormalizedPos, 0);
		}
		if (level == 2)
		{
			SampledColor = InputVoxelTexture2.SampleLevel(InSampler, NormalizedPos, 0);
		}
		if (level == 3)
		{
			SampledColor = InputVoxelTexture3.SampleLevel(InSampler, NormalizedPos, 0);
		}

//		AccumulatedColor += (1.0f - AccumulatedColor.a)*SampledColor;
		SampledColor.a *= 0.85f;
		AccumulatedColor.rgb += AccumulatedColor.a*SampledColor.rgb;
		AccumulatedColor.a *= saturate(1.0f - SampledColor.a);

//		Dist += CurDiameter;
		Dist += StepSize;
	}

	return AccumulatedColor;
}

float3 orthogonal(float3 u) {
	float3 v = float3(0.99146f, 0.11664f, 0.05832f);
	return abs(dot(u, v)) > 0.99999f ? cross(u, float3(0.0f, 1.0f, 0.0f)) : cross(u, v);
}

float4 TraceVoxelHemisphere(float4 InWldPos, float3 N, SamplerState InSampler)
{
	N = normalize(N);
	float3 T = orthogonal(N);
	T = normalize(T);
	float3 B = cross(T, N);

	// cone angle 45
	const float4 ConeDirections[12] = {
		float4(0.0f, 1.0f, 0.414f, 1.0f),
		float4(0.0f, -1.0f,  0.414f, 1.0f),
		float4(-1.0f, 0.0f,  0.414f, 1.0f),
		float4(1.0f, 0.0f,  0.414f, 1.0f),

		float4(0.0f, 1.0f, 2.414f, 1.0f),
		float4(0.0f, -1.0f,  2.414f, 1.0f),
		float4(-1.0f, 0.0f,  2.414f, 1.0f),
		float4(1.0f, 0.0f,  2.414f, 1.0f),

		float4(1.0f, 1.0f, 0.59f, 1.0f),
		float4(1.0f, -1.0f, 0.59f, 1.0f),
		float4(-1.0f, 1.0f, 0.59f, 1.0f),
		float4(-1.0f, -1.0f, 0.59f, 1.0f),
	};

	// assume cube voxel
	float VoxelSize = (SceneBoundMax.y - SceneBoundMin.y) / VCTVolumeSize.y;

	float4 TotalColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
	for (int i = 0; i < 12; ++i)
	{
		float3 SampleD = ConeDirections[i].xyz;
		float W = ConeDirections[i].w;
		float3 Dir = SampleD.z*N + SampleD.x*T + SampleD.y*B;
		Dir = normalize(Dir);

		float4 WldPos = InWldPos;
		WldPos.xyz += N *VoxelSize;
		TotalColor += TraceVoxelCone(WldPos, Dir, 800.0f, 1.0f, VoxelSize, InSampler)*W;
	}

	return TotalColor / 12.0f;
}