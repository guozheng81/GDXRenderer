#include "Common.hlsli"
#include "Utils.hlsli"

Texture2D DepthBuffer : register(t0);

SamplerState LinearSampler: register(s0);
SamplerState PointSampler : register(s1);

float4 Velocity_PS(QuadVS_Output Input) : SV_TARGET
{
	float	Depth = DepthBuffer.Sample(PointSampler, Input.Uv).r;
	float4 WldPos = GetWorldPositionFromDepth(Depth, Input.Uv);

	float4	PreScreenPos = mul(WldPos, mPreViewProjection);
	PreScreenPos.xy /= PreScreenPos.w;
	PreScreenPos.xy -= TemporalJitterParams.zw;
	PreScreenPos.x = (1.0f + PreScreenPos.x)*0.5f;
	PreScreenPos.y = (1.0f - PreScreenPos.y)*0.5f;

	float2 UnjitteredUv = Input.Uv;
	UnjitteredUv.x -= TemporalJitterParams.x*0.5f;
	UnjitteredUv.y += TemporalJitterParams.y*0.5f;

	float4 FinalVelocity = float4(0.0f, 0.0f, 0.0f, 0.0f);
	FinalVelocity.xy = (UnjitteredUv - PreScreenPos.xy)*5.0f;
	return FinalVelocity;
}

Texture2D SceneColor : register(t0);
Texture2D VelocityBuffer : register(t1);


float4 MotionBlur_PS(QuadVS_Output Input) : SV_TARGET
{
	float2	Velocity = VelocityBuffer.Sample(PointSampler, Input.Uv).rg;
	float4	FinalColor = SceneColor.Sample(PointSampler, Input.Uv);
	float	w = 1.0f;

	for (int i = 1; i < 20; ++i)
	{
		float w_i = 1.0f - i / 20.0f;
		float2 SampleUV = Input.Uv - Velocity*i;

		if (SampleUV.x < 0.0f || SampleUV.x > 1.0f || SampleUV.y < 0.0f || SampleUV.y > 1.0f)
		{
			break;
		}

		float4 SampleColor = SceneColor.Sample(PointSampler, SampleUV)*w_i;
		FinalColor += SampleColor;
		w += w_i;
	}

	FinalColor /= w;
	return FinalColor;
}

Texture2D InDepthBuffer : register(t2);
Texture2D NeighborMaxVBuffer : register(t3);

float SoftDepthCmp(float Z_a, float Z_b)
{
	return clamp(1.0f - (Z_a - Z_b) / 0.001f, 0.0f, 1.0f);
}

float Cone(float InDist, float VLen, float minStepLen)
{
	return clamp( 1.0f - InDist/max(VLen, minStepLen), 0.0f, 1.0f);
}

float Cylinder(float InDist, float VLen)
{
	return 1.0f - smoothstep(0.95f*VLen, 1.05f*VLen, InDist);
}

// Epic
float RandFast(uint2 PixelPos, float Magic = 3571.0)
{
	float2 Random2 = (1.0 / 4320.0) * PixelPos + float2(0.25, 0.0);
	float Random = frac(dot(Random2 * Random2, Magic));
	Random = frac(Random * Random * (2 * Magic));
	return Random;
}

#define TILE_SIZE 10

float4 MotionBlurFilter_PS(QuadVS_Output Input) : SV_TARGET
{
	float2	Velocity = VelocityBuffer.Sample(PointSampler, Input.Uv).rg;
	float4	FinalColor = SceneColor.Sample(PointSampler, Input.Uv);

	float	Depth = InDepthBuffer.Sample(PointSampler, Input.Uv).r;

//	float LinearZ = mProjection._m32 / (Depth - mProjection._m22);

	// tile jitter reference Epic motionblur shader
	uint2 PixelPos;
	PixelPos.x = (uint)(Input.Uv.x*ViewSizeAndInv.x);
	PixelPos.y = (uint)(Input.Uv.y*ViewSizeAndInv.y);
	float Random = RandFast(PixelPos);
	float Random1 = RandFast(PixelPos, 1251);
	float Random2 = RandFast(PixelPos, 5521);

	float2 TileJitter = (float2(Random1, Random2) - 0.5f)*0.5f;
	float2	MaxVelocityTextureSize = (ViewSizeAndInv.xy / TILE_SIZE);

//	float2 TileUv = clamp( Input.Uv.xy + TileJitter / MaxVelocityTextureSize, 0.0f, 1.0f);
	float2 TileUv = clamp((PixelPos/ TILE_SIZE + 0.5f + TileJitter) / MaxVelocityTextureSize, 0.0f, 1.0f);

	float2  NeighborMaxV = NeighborMaxVBuffer.Sample(LinearSampler, TileUv).rg;
	float	MaxVLen = length(NeighborMaxV);
	if (MaxVLen < ViewSizeAndInv.z/2.0f)
	{
		return FinalColor;
	}

	int StepCount = 21;

	float	lenVelocity = length(Velocity);
	float	TotalW = 0.0f;
	float4	TotalColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float	Sample_i = 0.0f;
	float	minStepLen = ViewSizeAndInv.z / 10.0f;// MaxVLen / (float)StepCount;

	for (int i = 0; i < StepCount; ++i)
	{
		Sample_i = ((float)i + Random - 0.5f) / (float)(StepCount-1);
		Sample_i = lerp(-1.0f, 1.0f, Sample_i);
		float2 SampleUv = Input.Uv + NeighborMaxV*Sample_i;

		float4 SampleColor = SceneColor.SampleLevel(LinearSampler, SampleUv, 0);

		float	SampleDepth = InDepthBuffer.SampleLevel(PointSampler, SampleUv, 0).r;
//		float	SampleLinearZ = mProjection._m32 / (SampleDepth - mProjection._m22);

//		float f = SoftDepthCmp(LinearZ, SampleLinearZ);
//		float b = SoftDepthCmp(SampleLinearZ, LinearZ);
		float f = SoftDepthCmp(Depth, SampleDepth);
		float b = SoftDepthCmp(SampleDepth, Depth);

		float	Dist = abs(MaxVLen*Sample_i);
		float2	SampleVelocity = VelocityBuffer.SampleLevel(PointSampler, SampleUv, 0).rg;
		float	lenSampleVelocity = length(SampleVelocity);

		float w = f*Cone(Dist, lenSampleVelocity, minStepLen) + b*Cone(Dist, lenVelocity, minStepLen)
					+Cylinder(Dist, lenVelocity)*Cylinder(Dist, lenSampleVelocity)*2.0f;

		if (w > 0.0001f)
		{
			TotalW += w;
			TotalColor += SampleColor*w;
		}
	}

	FinalColor = lerp(FinalColor, TotalColor / max(TotalW, 0.0001f), saturate(TotalW));
	FinalColor.a = 1.0f;
	return FinalColor;
}

groupshared float2 GroupVelocities[400];

Texture2D<float2>	InVelocityBuffer : register(t0);
RWTexture2D<float2>	MaxVelocityBuffer : register(u0);

[numthreads(TILE_SIZE, TILE_SIZE, 1)]
void MaxVelocity(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	uint	ThreadIdx = GTid.y * TILE_SIZE + GTid.x;
	uint	Width;
	uint	Height;
	InVelocityBuffer.GetDimensions(Width, Height);

	if (DTid.x >= Width || DTid.y >= Height)
	{
		GroupVelocities[ThreadIdx] = float2(0.0f, 0.0f);
	}
	else
	{
		GroupVelocities[ThreadIdx] = InVelocityBuffer[DTid.xy].xy;
	}

	GroupMemoryBarrierWithGroupSync();

	if (ThreadIdx < TILE_SIZE)
	{
		float	V_len = length(GroupVelocities[ThreadIdx]);

		for (uint y = 1; y < TILE_SIZE; ++y)
		{
			float Cur_len = length(GroupVelocities[y * TILE_SIZE + ThreadIdx]);
			if (Cur_len > V_len)
			{
				GroupVelocities[ThreadIdx] = GroupVelocities[y * TILE_SIZE + ThreadIdx];
				V_len = Cur_len;
			}
		}
	}

	GroupMemoryBarrierWithGroupSync();

	if (ThreadIdx == 0)
	{
		float	V_len = length(GroupVelocities[0]);
		for (uint x = 1; x < TILE_SIZE; ++x)
		{
			float Cur_len = length(GroupVelocities[x]);
			if (Cur_len > V_len)
			{
				GroupVelocities[0] = GroupVelocities[x];
				V_len = Cur_len;
			}
		}

		MaxVelocityBuffer[Gid.xy] = GroupVelocities[0];
	}

}

Texture2D<float2>	InMaxVelocityBuffer : register(t0);
RWTexture2D<float2>	OutMaxVelocityBuffer : register(u0);

#define VELOCITY_GROUP_COUNT_X 16
#define VELOCITY_GROUP_COUNT_Y 16

groupshared float2 MaxSharedVelocities[1024];

[numthreads(VELOCITY_GROUP_COUNT_X, VELOCITY_GROUP_COUNT_Y, 1)]
void NeighborMaxVelocity(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	uint	Width;
	uint	Height;
	InMaxVelocityBuffer.GetDimensions(Width, Height);

	uint2 coord = GTid.xy - 1 + Gid.xy*(uint2(VELOCITY_GROUP_COUNT_X, VELOCITY_GROUP_COUNT_Y) - 2);

	if (all(coord >= uint2(0, 0)) && all(coord < uint2(Width, Height)))
	{
		MaxSharedVelocities[GI] = InMaxVelocityBuffer[coord].xy;
	}
	else
	{
		MaxSharedVelocities[GI] = float2(0.0f, 0.0f);
	}

	GroupMemoryBarrierWithGroupSync();

	if (all(GTid.xy >= 1) && all(GTid.xy < (uint2(VELOCITY_GROUP_COUNT_X, VELOCITY_GROUP_COUNT_Y) - 1)))
	{
		float2	MaxV = MaxSharedVelocities[GI];
		float	MaxVLen = length(MaxV);
		for (uint i = -1; i <= 1; ++i)
		{
			for (uint j = -1; j <= 1; ++j)
			{
				float2 V = MaxSharedVelocities[(GTid.y + j)*VELOCITY_GROUP_COUNT_X + GTid.x + i];
				float VLen = length(V);
				if (VLen > MaxVLen)
				{
					MaxVLen = VLen;
					MaxV = V;
				}
			}
		}

		if (all(coord >= uint2(0, 0)) && all(coord < uint2(Width, Height)))
		{
			OutMaxVelocityBuffer[coord] = MaxV;
		}
	}

}
