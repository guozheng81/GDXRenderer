#include "CommonHelper.hlsli"

cbuffer SkyLightBuffer : register(b2)
{
	float4       SkyCubeSize;
	float4		MaterialParams;
}

TextureCube<float4>		SkyCubeMap : register(t0);
RWStructuredBuffer<SH_RGB>	OutAccum : register(u0);

SamplerState SamplerPoint : register(s1);

groupshared float4 GroupAccum[3][256];

float3 GetCubeMapSampleDirection(float x, float y, uint z)
{
	x = 2.0f*x - 1.0f;
	y = 1.0f - y*2.0f;

	float3 Dir = 0;
	switch (z)
	{
	case 0: 
		Dir.z = - x;
		Dir.y = y;
		Dir.x = 1.0f;
		break;

	case 1: // Negative X
		Dir.z = x;
		Dir.y = y;
		Dir.x = -1.0f;
		break;

	case 2: // Positive Y
		Dir.z = -y;
		Dir.y = 1.0f;
		Dir.x = x;
		break;

	case 3: // Negative Y
		Dir.z = y;
		Dir.y = -1.0f;
		Dir.x = x;
		break;

	case 4: // Positive Z
		Dir.z = 1.0f;
		Dir.y = y;
		Dir.x = x;
		break;

	case 5: // Negative Z
		Dir.z = -1.0f;
		Dir.y = y;
		Dir.x = -x;
		break;
	}

	return normalize(Dir);
}


[numthreads(16, 16, 1)]
void SHConvert(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	uint	groupDimension = (uint)SkyCubeSize.x / 16;
	uint	OutIdx = Gid.y*groupDimension + Gid.x;

	uint Idx = GTid.x * 16 + GTid.y;
	GroupAccum[0][Idx] = 0.0f;
	GroupAccum[1][Idx] = 0.0f;
	GroupAccum[2][Idx] = 0.0f;

	float x = Gid.x * groupDimension + GTid.x;
	float y = Gid.y * groupDimension + GTid.y;
	float2 NormalizedXY = float2(x, y)*SkyCubeSize.zw;
	float2 Uv = NormalizedXY*2.0f - 1.0f;
	float UvDot = 1.0f + dot(Uv, Uv);
	float Weight = 4.0f / (sqrt(UvDot)*UvDot);

	[loop]
	for (uint face = 0; face < 6; face++)
	{
		float3 Dir = GetCubeMapSampleDirection(NormalizedXY.x, NormalizedXY.y, face);

		float4 SampleColor = SkyCubeMap.SampleLevel(SamplerPoint, Dir, 0);
		float4 SH = SHBasisFunc(Dir);

		[loop]
		for (uint i = 0; i < 3; ++i)
		{
			GroupAccum[i][Idx] += (SH*SampleColor[i] * Weight);
		}
	}

	GroupMemoryBarrierWithGroupSync();

	[loop]
	for (uint i = 0; i < 3; ++i)
	{
		if (Idx < 128)
		{
			GroupAccum[i][Idx] += GroupAccum[i][Idx + 128];
		}
		GroupMemoryBarrierWithGroupSync();

		if (Idx < 64)
		{
			GroupAccum[i][Idx] += GroupAccum[i][Idx + 64];
		}
		GroupMemoryBarrierWithGroupSync();

		if (Idx < 32)
		{
			GroupAccum[i][Idx] += GroupAccum[i][Idx + 32];
		}
		GroupMemoryBarrierWithGroupSync();
		
		if (Idx < 16)
		{
			GroupAccum[i][Idx] += GroupAccum[i][Idx + 16];
		}
		GroupMemoryBarrierWithGroupSync();
		
		if (Idx < 8)
		{
			GroupAccum[i][Idx] += GroupAccum[i][Idx + 8];
		}
		GroupMemoryBarrierWithGroupSync();

		if (Idx == 0)
		{
			for (uint k = 1; k < 8; ++k)
			{
				GroupAccum[i][Idx] += GroupAccum[i][Idx + k];
			}

			if (i == 0)
			{
				OutAccum[OutIdx].R = GroupAccum[i][0];
			}
			if (i == 1)
			{
				OutAccum[OutIdx].G = GroupAccum[i][0];
			}
			if (i == 1)
			{
				OutAccum[OutIdx].B = GroupAccum[i][0];
			}
		}
	}
}


StructuredBuffer<SH_RGB> In1DSHBuffer : register(t0);
RWStructuredBuffer<SH_RGB> OutSHValue : register(u0);

groupshared SH_RGB Group1DArray[128];

[numthreads(128, 1, 1)]
void SHCombine(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	uint Idx = DTid.x;
	Group1DArray[Idx].R = In1DSHBuffer[Idx].R + In1DSHBuffer[Idx + 128].R;
	Group1DArray[Idx].G = In1DSHBuffer[Idx].G + In1DSHBuffer[Idx + 128].G;
	Group1DArray[Idx].B = In1DSHBuffer[Idx].B + In1DSHBuffer[Idx + 128].B;

	GroupMemoryBarrierWithGroupSync();

	if (Idx < 64)
	{
		Group1DArray[Idx].R = Group1DArray[Idx].R + Group1DArray[Idx + 64].R;
		Group1DArray[Idx].G = Group1DArray[Idx].G + Group1DArray[Idx + 64].G;
		Group1DArray[Idx].B = Group1DArray[Idx].B + Group1DArray[Idx + 64].B;
	}
	GroupMemoryBarrierWithGroupSync();

	if (Idx < 32)
	{
		Group1DArray[Idx].R = Group1DArray[Idx].R + Group1DArray[Idx + 32].R;
		Group1DArray[Idx].G = Group1DArray[Idx].G + Group1DArray[Idx + 32].G;
		Group1DArray[Idx].B = Group1DArray[Idx].B + Group1DArray[Idx + 32].B;
	}
	GroupMemoryBarrierWithGroupSync();

	if (Idx < 16)
	{
		Group1DArray[Idx].R = Group1DArray[Idx].R + Group1DArray[Idx + 16].R;
		Group1DArray[Idx].G = Group1DArray[Idx].G + Group1DArray[Idx + 16].G;
		Group1DArray[Idx].B = Group1DArray[Idx].B + Group1DArray[Idx + 16].B;
	}
	GroupMemoryBarrierWithGroupSync();

	if (Idx == 0)
	{
		for (uint k = 1; k < 16; ++k)
		{
			Group1DArray[Idx].R += Group1DArray[Idx + k].R;
			Group1DArray[Idx].G += Group1DArray[Idx + k].G;
			Group1DArray[Idx].B += Group1DArray[Idx + k].B;
		}

		float	TotalWeight = 3.1415926f * (SkyCubeSize.x*SkyCubeSize.y * 6);
		OutSHValue[0].R = Group1DArray[Idx].R / TotalWeight;
		OutSHValue[0].G = Group1DArray[Idx].G / TotalWeight;
		OutSHValue[0].B = Group1DArray[Idx].B / TotalWeight;
	}
}

float3	ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
	float a = roughness*roughness;

	float phi = 2.0f*3.1415926f*Xi.x;
	float cosTheta = sqrt((1.0f - Xi.y) / (1.0f + (a*a - 1.0f) * Xi.y));
	float sinTheta = sqrt(1.0f - cosTheta*cosTheta);

	// from spherical coordinates to cartesian coordinates
	float3 H;
	H.x = cos(phi) * sinTheta;
	H.y = sin(phi) * sinTheta;
	H.z = cosTheta;

	// from tangent-space vector to world-space sample vector
	float3 up = abs(N.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
	float3 tangent = normalize(cross(up, N));
	float3 bitangent = cross(N, tangent);

	float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
	return normalize(sampleVec);
}

RWTexture2DArray<float4>		SpecularCubeMap : register(u0);

[numthreads(16, 16, 1)]
void SpecularFilter(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	uint	groupDimension = (uint)SkyCubeSize.x / 16;

	float x = DTid.x;
	float y = DTid.y;
	float2 NormalizedXY = float2(x, y)*SkyCubeSize.zw;

	uint	face = DTid.z;
	float3 N = GetCubeMapSampleDirection(NormalizedXY.x, NormalizedXY.y, face);
	float3 V = N;

	float4 FilteredColor = 0.0f;
	float	TotalWeight = 0.0f;

	[loop]
	for (uint i = 0; i < 1024; ++i)
	{
		float2 RandomPos = hammersley2d(i, 1024);
		float3 H = ImportanceSampleGGX(RandomPos, N, MaterialParams.r);

		float3 L = normalize(dot(V, H)*2.0f*H - V);
		float  N_dot_L = dot(N, L);
		if (N_dot_L > 0.0f)
		{
			FilteredColor += SkyCubeMap.SampleLevel(SamplerPoint, N, 0)*N_dot_L;
			TotalWeight += N_dot_L;
		}
	}

	FilteredColor.rgb /= 1024.0f;
	FilteredColor.a = 1.0f;

	SpecularCubeMap[DTid] = FilteredColor;
}

float GeometryEpic(float N_dot_V, float N_dot_L, float roughness)
{
	float k = roughness*roughness/2.0f;


	float G1 = 1.0f/*N_dot_V*/ / (N_dot_V*(1.0f - k) + k);
	float G2 = 1.0f/*N_dot_L*/ / (N_dot_L*(1.0f - k) + k);

	return G1*G2;
}

RWTexture2D<float4>		OutIntegration : register(u0);

[numthreads(16, 16, 1)]
void SpecularIntegration(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	float2 uv = DTid.xy / 1024.0f;

	float roughness = max(uv.y, 0.001f);
	float N_dot_V = uv.x;

	float3 N = float3(0.0f, 0.0f, 1.0f);
	float3 V = float3(sqrt(1.0f - N_dot_V*N_dot_V), 0.0f, N_dot_V);

	float A = 0.0f;
	float B = 0.0f;

	[loop]
	for (uint i = 0; i < 1024; ++i)
	{
		float2 RandomPos = hammersley2d(i, 1024);
		float3 H = ImportanceSampleGGX(RandomPos, N, roughness);
		float3 L = normalize(H*dot(V, H)*2.0f - V);

		float N_dot_L = dot(N, L);
		float N_dot_H = saturate(dot(N, H));
		float V_dot_H = saturate(dot(V, H));

		if (N_dot_L > 0.0f)
		{
			float Geometry = GeometryEpic(N_dot_V, N_dot_L, roughness);
			float G_Vis = Geometry*V_dot_H / max(N_dot_H, 0.0001f) * N_dot_L;
			float F_C = pow(1.0f - V_dot_H, 5.0f);

			A += (1.0f - F_C)*G_Vis;
			B += (F_C)*G_Vis;
		}
	}

	A /= 1024.0f;
	B /= 1024.0f;

	OutIntegration[DTid.xy] = float4(A, B, 0.0f, 1.0f);
}