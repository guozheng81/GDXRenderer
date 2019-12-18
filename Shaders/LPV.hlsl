#include "CommonHelper.hlsli"
#include "Common.hlsli"
#include "LPVHelper.hlsli"
#include "Utils.hlsli"

Texture2D	DiffuseTexture : register(t0);
Texture2D	NormalTexture : register(t1);

SamplerState LinearSampler : register(s0);
SamplerState PointSampler : register(s1);
SamplerState LinearSamplerWrap : register(s2);
SamplerState PointSamplerWrap : register(s3);

Texture2DArray	ShadowBuffers : register(t6);

struct PS_INPUT
{
	float3 Normal		: NORMAL;
	float2 Texcoord	: TEXCOORD0;
	float3 Tangent		: TEXCOORD1;
	float4 WorldPos : TEXCOORD2;
};

struct SH_VPL
{
	float4 R;
	float4 G;
	float4 B;
	int	Next;
};

struct SH_GV
{
	float4 Occlusion;
	int	Next;
};

RWTexture3D<uint>    OutputVplAddressTexture	: register(u1);
RWStructuredBuffer<SH_VPL>	OutVplBuffer : register(u2);

RWTexture3D<uint>    OutputGvAddressTexture	: register(u3);
RWStructuredBuffer<SH_GV>	OutGvBuffer : register(u4);

float4 RSMPs(PS_INPUT Input) : SV_TARGET
{
	float4 Albedo = DiffuseTexture.Sample(LinearSamplerWrap, Input.Texcoord);

	if (Albedo.a < 0.5f)
	{
		discard;
	}

	float3 N = normalize(Input.Normal);

	float4	WldPos = Input.WorldPos;

	float3	L = normalize(LightParams.xyz);
	float3	V = normalize(CameraOrigin.xyz - WldPos.xyz);

	float4 FinalColor;
	float	SimpleLighting = max(dot(N, L), 0.0f);
	FinalColor.rgb = Albedo.rgb*SimpleLighting;
	FinalColor.a = 1.0f;
	FinalColor.rgb = clamp(FinalColor.rgb, 0.0f, 1.0f);

	float4 SHTransfer = SHCosTransfer(-N);

	int	CascadeIdx = GetCascadeIndex(WldPos);
	float Shadow = GetCascadeShadowSimple(ShadowBuffers, PointSampler, CascadeIdx, WldPos, 0.002f);
	if (Shadow < 0.001f)
	{
		discard;
	}
	float N_offset = (SceneBoundMax.z - SceneBoundMin.z) / LPVSize.z;
	float3 NormalizedPos = (WldPos.xyz + N*10.0f - SceneBoundMin.xyz) / (SceneBoundMax.xyz - SceneBoundMin.xyz) * LPVTexScale + LPVTexBias;
	if (all(NormalizedPos > 0.0f) && all(NormalizedPos < 1.0f))
	{
		NormalizedPos = saturate(NormalizedPos)*LPVSize;
		uint3	VoxelPos = uint3(NormalizedPos);

		uint	NewNodeAddress = OutVplBuffer.IncrementCounter();
		NewNodeAddress += 1;

		uint	PreNodeAddress = 0;
		InterlockedExchange(OutputVplAddressTexture[VoxelPos], NewNodeAddress, PreNodeAddress);

		SH_VPL	newVPL;
		newVPL.R = SHTransfer*FinalColor.r*LightingControl.x;
		newVPL.G = SHTransfer*FinalColor.g*LightingControl.x;
		newVPL.B = SHTransfer*FinalColor.b*LightingControl.x;
		newVPL.Next = PreNodeAddress;

		OutVplBuffer[NewNodeAddress - 1] = newVPL;
	}
	
	// gv volume
	NormalizedPos = ((WldPos - SceneBoundMin.xyz) / (SceneBoundMax.xyz - SceneBoundMin.xyz) + LPVTexBias)* LPVTexScale + LPVTexBias;
	if (all(NormalizedPos > 0.0f) && all(NormalizedPos < 1.0f))
	{
		NormalizedPos = saturate(NormalizedPos)*LPVSize;
		uint3	VoxelPos = uint3(NormalizedPos);

		uint	NewNodeAddress = OutGvBuffer.IncrementCounter();
		NewNodeAddress += 1;

		uint	PreNodeAddress = 0;
		InterlockedExchange(OutputGvAddressTexture[VoxelPos], NewNodeAddress, PreNodeAddress);

		SH_GV	newGv;
		newGv.Occlusion = SHTransfer;
		newGv.Next = PreNodeAddress;

		OutGvBuffer[NewNodeAddress - 1] = newGv;
	}
	
	return FinalColor;
}

Texture3D<uint> InVplAddressTex              : register(t0);
StructuredBuffer<SH_VPL>	InVplBuffer		: register(t1);

RWStructuredBuffer<SH_RGB>	OutAccumVpl		: register(u0);

[numthreads(4, 4, 4)]
void AccumulateVpl(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	uint NodeAddress = InVplAddressTex[DTid];

	float4	AccumR = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float4	AccumG = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float4	AccumB = float4(0.0f, 0.0f, 0.0f, 0.0f);

	int Count = 0;
	while (NodeAddress != 0)
	{
		SH_VPL	CurVpl = InVplBuffer[NodeAddress - 1];
		AccumR += CurVpl.R;
		AccumG += CurVpl.G;
		AccumB += CurVpl.B;

		NodeAddress = (uint)(CurVpl.Next);
		Count++;
	}

	SH_RGB	FinalSH;
	FinalSH.R = AccumR;
	FinalSH.G = AccumG;
	FinalSH.B = AccumB;

	OutAccumVpl[DTid.z*LPVSize.x*LPVSize.y + DTid.y*LPVSize.x + DTid.x] = FinalSH;
}

Texture3D<uint> InGvAddressTex              : register(t0);
StructuredBuffer<SH_GV>	InGvBuffer		: register(t1);

RWStructuredBuffer<float4>	OutAccumGv		: register(u0);

[numthreads(4, 4, 4)]
void AccumulateGv(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	uint NodeAddress = InGvAddressTex[DTid];

	float4	Accum = float4(0.0f, 0.0f, 0.0f, 0.0f);

	int Count = 0;
	while (NodeAddress != 0)
	{
		SH_GV	CurGv = InGvBuffer[NodeAddress - 1];
		Accum += CurGv.Occlusion;

		NodeAddress = (uint)(CurGv.Next);
		Count++;
	}

	OutAccumGv[DTid.z*LPVSize.x*LPVSize.y + DTid.y*LPVSize.x + DTid.x] = Accum;
}


StructuredBuffer<SH_RGB>	InAccumVpl		: register(t0);
StructuredBuffer<float4>	InAccumGv		: register(t1);

groupshared SH_RGB	SharedAccumVpl[512];

SH_RGB SampleVplVolume(int3 pos)
{
	if (all(pos >= int3(0, 0, 0)) && all(pos < int3(LPVSize)))
	{
		return	InAccumVpl[pos.z*LPVSize.x*LPVSize.y + pos.y*LPVSize.x + pos.x];
	}

	SH_RGB Sample_SH;
	Sample_SH.R = float4(0.0f, 0.0f, 0.0f, 0.0f);
	Sample_SH.G = float4(0.0f, 0.0f, 0.0f, 0.0f);
	Sample_SH.B = float4(0.0f, 0.0f, 0.0f, 0.0f);

	/*
	int Count = 0;
	for (int i = -1; i <= 1; i++)
	{
		for (int j = -1; j <= 1; j++)
		{
			for (int k = -1; k <= 1; k++)
			{
				int3 curPos = pos + int3(i, j, k);

				if (all(curPos >= int3(0, 0, 0)) && all(curPos < int3(LPVSize)))
				{
					SH_RGB Cur_SH = InAccumVpl[curPos.z*LPVSize.x*LPVSize.y + curPos.y*LPVSize.x + curPos.x];
					Sample_SH.R += Cur_SH.R;
					Sample_SH.G += Cur_SH.G;
					Sample_SH.B += Cur_SH.B;
					Count++;
				}
			}
		}
	}

	if (Count > 0)
	{
		Sample_SH.R /= Count;
		Sample_SH.G /= Count;
		Sample_SH.B /= Count;
	}
	*/

	return Sample_SH;
}

// also used for neighbor offset lookup
static const int3 FaceOffsets[6] = {
	int3(0, 0, 1),
	int3(1, 0, 0),
	int3(0, 0, -1),
	int3(-1, 0, 0),
	int3(0, 1, 0),
	int3(0, -1, 0) };

static const int3 GVOffsets[8] = {
	int3(0, 0, 0),
	int3(-1, 0, 0),
	int3(0, 0, -1),
	int3(-1, 0, -1),
	int3(0, -1, 0),
	int3(-1, -1, 0),
	int3(0, -1, -1),
	int3(-1, -1, -1)
};

// neighbor's opposite side 4 samples on GV 
static const int4 NeighborIdxToGVOffsetIdx[6] = {
	int4(2, 3, 6, 7), 
	int4(1, 3, 5, 7),

	int4(0, 1, 4, 5),
	int4(0, 2, 4, 6),

	int4(4, 5, 6, 7),
	int4(0, 1, 2, 3)
};

float	SampleGv(int3 pos, int neighbor_idx, float4 SH_dir)
{
	float4 Gv_SH = float4(0.0f, 0.0f, 0.0f, 0.0f);
	int		Count = 0;
	for (int i = 0; i < 4; i++)
	{
		int4 GVOffsetIdx = NeighborIdxToGVOffsetIdx[neighbor_idx];
		int3 GVPosOffset = GVOffsets[GVOffsetIdx[i]];

		int3 GVSamplePos = pos + GVPosOffset;

		if (all(GVSamplePos >= int3(0, 0, 0)) && all(GVSamplePos < int3(LPVSize)))
		{
			Gv_SH += InAccumGv[GVSamplePos.z*LPVSize.x*LPVSize.y + GVSamplePos.y*LPVSize.x + GVSamplePos.x];
			Count++;
		}
	}

	if (Count > 0)
	{
		Gv_SH /= Count;
	}

	return	max((1.0f - dot(Gv_SH, SH_dir)), 0.0f);
}

[numthreads(4, 4, 4)]
void PropagateVpl(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	float4 FaceSHTransfer[6];
	for (int k = 0; k < 6; ++k)
	{
		FaceSHTransfer[k] = SHCosTransfer(FaceOffsets[k]);
	}

	SH_RGB Cur_SH;
	Cur_SH.R = float4(0.0f, 0.0f, 0.0f, 0.0f);
	Cur_SH.G = float4(0.0f, 0.0f, 0.0f, 0.0f);
	Cur_SH.B = float4(0.0f, 0.0f, 0.0f, 0.0f);

	for (int i = 0; i < 6; ++i)
	{
		float3 NeighborRelPos = FaceOffsets[i];
		int3 NeighborPos = DTid.xyz + NeighborRelPos;

		SH_RGB NeighborSH = SampleVplVolume(NeighborPos);

		for (int face_idx = 0; face_idx < 6; face_idx++)
		{
			float3 face_center = (float3)FaceOffsets[face_idx] * 0.5f;

			float3 neighbor_to_face = face_center - NeighborRelPos;
			float  len = length(neighbor_to_face);
			neighbor_to_face /= len;

			if (len > 0.9f)
			{
				float solid_angle = (len > 1.4f ? 22.95668f / (4 * 180.0f) : 24.26083f / (4 * 180.0f));

				float4 SH_dir = SHBasisFunc(-neighbor_to_face);

				float Gv_weight = SampleGv(DTid.xyz, i, SH_dir);

				float propagated_R = max(dot(NeighborSH.R, SH_dir), 0.0f)*solid_angle*Gv_weight;
				float propagated_G = max(dot(NeighborSH.G, SH_dir), 0.0f)*solid_angle*Gv_weight;
				float propagated_B = max(dot(NeighborSH.B, SH_dir), 0.0f)*solid_angle*Gv_weight;

				Cur_SH.R += FaceSHTransfer[face_idx] * propagated_R * 1.8f;
				Cur_SH.G += FaceSHTransfer[face_idx] * propagated_G * 1.8f;
				Cur_SH.B += FaceSHTransfer[face_idx] * propagated_B * 1.8f;
			}
		}
	}

	OutAccumVpl[DTid.z*LPVSize.x*LPVSize.y + DTid.y*LPVSize.x + DTid.x] = Cur_SH;
}

RWTexture3D<float4>	OutResolvedVplR		: register(u0);
RWTexture3D<float4>	OutResolvedVplG		: register(u1);
RWTexture3D<float4>	OutResolvedVplB		: register(u2);

[numthreads(4, 4, 4)]
void ResolveVpl(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	SH_RGB InSH = InAccumVpl[DTid.z*LPVSize.x*LPVSize.y + DTid.y*LPVSize.x + DTid.x];

	OutResolvedVplR[DTid] = InSH.R;
	OutResolvedVplG[DTid] = InSH.G;
	OutResolvedVplB[DTid] = InSH.B;
}