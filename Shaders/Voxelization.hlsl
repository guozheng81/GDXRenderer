#include "Common.hlsli"
#include "VoxelConeTracer.hlsli"

struct VS_OUTPUT
{
	float3 Normal		: NORMAL;
	float2 Texcoord	: TEXCOORD0;
	float3 Tangent		: TEXCOORD1;
	float4 WorldPos : TEXCOORD2;
	float4 Position	: SV_POSITION;
};

[maxvertexcount(3)]
void VoxelGs(triangle VS_OUTPUT input[3], inout TriangleStream<VS_OUTPUT> TriStream)
{
	// calculate the face normal
	float3 faceEdgeA = input[1].WorldPos - input[0].WorldPos;
	float3 faceEdgeB = input[2].WorldPos - input[0].WorldPos;
	float3 crossP = cross(faceEdgeA, faceEdgeB);
	float3 p = abs(crossP);

	VS_OUTPUT output[3];

	[unroll] for (int i = 0; i < 3; i++)
	{
		float3 NormalizedPos = (input[i].WorldPos - SceneBoundMin.xyz) / (SceneBoundMax.xyz - SceneBoundMin.xyz) * VoxelTexScale + VexelTexBias;
		NormalizedPos = (NormalizedPos*2.0f - 1.0f);

		if (p.z > p.x && p.z > p.y) {
			output[i].Position = float4(NormalizedPos.x, NormalizedPos.y, 0, 1);
		}
		else if (p.x > p.y && p.x > p.z) {
			output[i].Position = float4(NormalizedPos.y, NormalizedPos.z, 0, 1);
		}
		else {
			output[i].Position = float4(NormalizedPos.x, NormalizedPos.z, 0, 1);
		}

		output[i].Normal = input[i].Normal;
		output[i].WorldPos = input[i].WorldPos;
		output[i].Texcoord = input[i].Texcoord;
		output[i].Tangent = input[i].Tangent;
	}

	float2 side0N = normalize(output[1].Position.xy - output[0].Position.xy);
	float2 side1N = normalize(output[2].Position.xy - output[1].Position.xy);
	float2 side2N = normalize(output[0].Position.xy - output[2].Position.xy);
	const float texelSize = 1.0f / 64.0f;
	output[0].Position.xy += normalize(-side0N + side2N)*texelSize;
	output[1].Position.xy += normalize(side0N - side1N)*texelSize;
	output[2].Position.xy += normalize(side1N - side2N)*texelSize;

	[unroll] for (int j = 0; j < 3; j++)
	{
		TriStream.Append(output[j]);
	}

	TriStream.RestartStrip();
}

#define MIPMAP_OFFSET 1
#define MIPMAP_NUM_THREADS 8
groupshared float4 mipmapShared[512];

Texture3D<float4> InputMipMapTexture	: register(t0);
RWTexture3D<float4> OutputMipMapTexture : register(u0);

cbuffer MipMapBuffer : register(b2)
{
	float4       g_MipMapSize;
}

[numthreads(8, 8, 8)]
void DownSampleMipMap(uint3 Gid : SV_GroupID, uint3 GI : SV_GroupThreadID)
{
	int3 coord = GI - MIPMAP_OFFSET + Gid*(MIPMAP_NUM_THREADS - MIPMAP_OFFSET * 2);

	if (all(coord >= int3(0, 0, 0)) && all(coord < int3(g_MipMapSize.xyz)))
	{
		mipmapShared[GI.x*MIPMAP_NUM_THREADS*MIPMAP_NUM_THREADS + GI.y*MIPMAP_NUM_THREADS + GI.z] = InputMipMapTexture[coord];
	}
	else
	{
		mipmapShared[GI.x*MIPMAP_NUM_THREADS*MIPMAP_NUM_THREADS + GI.y*MIPMAP_NUM_THREADS + GI.z] = float4(0.0f, 0.0f, 0.0f, 0.0f);
	}

	coord = clamp(coord, int3(0, 0, 0), int3((int)g_MipMapSize.x - 1, (int)g_MipMapSize.y - 1, (int)g_MipMapSize.z - 1));

	GroupMemoryBarrierWithGroupSync();

	if (all(GI >= MIPMAP_OFFSET) && all(GI < (MIPMAP_NUM_THREADS - MIPMAP_OFFSET)) && all(coord % 2 == 0))
	{
		float4 OutputColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
		for (int i = -MIPMAP_OFFSET; i <= MIPMAP_OFFSET; ++i)
		{
			for (int j = -MIPMAP_OFFSET; j <= MIPMAP_OFFSET; ++j)
			{
				for (int k = -MIPMAP_OFFSET; k <= MIPMAP_OFFSET; ++k)
				{
					uint x = (uint)(GI.x + i);
					uint y = (uint)(GI.y + j);
					uint z = (uint)(GI.z + k);
					OutputColor += mipmapShared[x*MIPMAP_NUM_THREADS*MIPMAP_NUM_THREADS + y*MIPMAP_NUM_THREADS + z];
				}
			}
		}
		OutputMipMapTexture[coord / 2] = OutputColor / 27.0f;
	}
}

Texture3D<uint> InVoxelVolumeTex              : register(t0);
RWTexture3D<float4> OutVoxelVolumeTex              : register(u0);

[numthreads(8, 8, 8)]
void UnpackVoxel(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	uint	packed_value = InVoxelVolumeTex[DTid];

	float4 SampleColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
	if (packed_value > 0)
	{
		uint	r = (packed_value & 0x00ff0000) >> 16;
		uint	g = (packed_value & 0x0000ff00) >> 8;
		uint	b = packed_value & 0x000000ff;

		SampleColor.rgb = float3(r / 255.0f, g / 255.0f, b / 255.0f);
		SampleColor.a = 1.0f;
	}

	OutVoxelVolumeTex[DTid] = SampleColor;
}

Texture3D<uint> InVoxelAddressTex              : register(t0);
StructuredBuffer<float4>	InVoxelBuffer		: register(t1);

[numthreads(8, 8, 8)]
void ResolveVoxel(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	uint NodeAddress = InVoxelAddressTex[DTid];
	float3	AccColor = float3(0.0f, 0.0f, 0.0f);
	uint	VoxelCount = 0;
	uint	LitVoxelCount = 0;
	while (NodeAddress != 0)
	{
		float4	CurVoxel = InVoxelBuffer[NodeAddress - 1];

		if (length(CurVoxel.rgb) > 0.001f)
		{
			AccColor += CurVoxel.rgb;
			LitVoxelCount++;
		}

		VoxelCount++;
		NodeAddress = (uint)(CurVoxel.a);
	}

	if (VoxelCount > 0)
	{
		if (LitVoxelCount > 0)
		{
			AccColor /= (float)LitVoxelCount;
		}
		OutVoxelVolumeTex[DTid] = float4(AccColor, 1.0f);
	}
	else
	{
		OutVoxelVolumeTex[DTid] = float4(0.0f, 0.0f, 0.0f, 0.0f);
	}
}


Texture3D<float4> VoxelVolumeTex              : register(t0);
SamplerState PointSampler : register(s1);

float4 VisualizeVoxelPs(QuadVS_Output Input) : SV_TARGET
{
	float2 ssCoords = Input.Uv * float2(2, -2) + float2(-1,1);
	float3 worldCamRay = normalize(ssCoords.xxx * CameraXAxis.xyz + ssCoords.yyy * CameraYAxis.xyz + CameraZAxis.xyz);

	float3 worldPos = CameraOrigin.xyz + worldCamRay;

	float4 FinalColor = float4(0.0f, 0.0f, 0.0f, 0.0f);

	for (int i = 0; i < 1500 && FinalColor.a < 0.99f; i++)
	{
		float3 CurPos = CameraOrigin.xyz + worldCamRay*(0.5f + i*2.0f);
		float3 NormalizedPos = (CurPos.xyz - SceneBoundMin.xyz) / (SceneBoundMax.xyz - SceneBoundMin.xyz) * VoxelTexScale + VexelTexBias;

		if (NormalizedPos.x > 0.0f && NormalizedPos.x < 1.0f && NormalizedPos.y > 0.0f && NormalizedPos.y < 1.0f && NormalizedPos.z > 0.0f && NormalizedPos.z < 1.0f)
		{
			float4 SampleColor = VoxelVolumeTex.SampleLevel(PointSampler, NormalizedPos, 0);
			FinalColor += (1.0f - FinalColor.a)*SampleColor;
		}
	}

	return FinalColor;
}
