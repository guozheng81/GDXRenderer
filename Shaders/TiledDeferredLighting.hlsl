#include "Common.hlsli"

struct PointLightInfo
{
	float4 Position;
	float4 Color;
};

struct LightLinkNode
{
	int	LightIndex;
	int	NextAddress;
};

StructuredBuffer<PointLightInfo>	AllLights : register(t0);
RWBuffer<int>		LightListHeader	: register(u0);
RWStructuredBuffer<LightLinkNode>		LightList : register(u1);

#define TILE_SIZE 20

float	DistanceToFrustumPlane(float2 ScreenPos1, float2 ScreenPos2, float3 InPos)
{
	ScreenPos1.x = ScreenPos1.x*2.0f - 1.0f;
	ScreenPos1.y = 1.0f - ScreenPos1.y*2.0f;

	ScreenPos2.x = ScreenPos2.x*2.0f - 1.0f;
	ScreenPos2.y = 1.0f - ScreenPos2.y*2.0f;

	float3 CameraRay = (CameraXAxis.xyz*ScreenPos1.x + CameraYAxis.xyz*ScreenPos1.y + CameraZAxis.xyz);
	float3 CameraRay2 = (CameraXAxis.xyz*ScreenPos2.x + CameraYAxis.xyz*ScreenPos2.y + CameraZAxis.xyz);
	float LinearZ = mProjection._m32 / (1.0f - mProjection._m22);
	float3 Pos1_Eye = CameraRay*LinearZ;
	float3 Pos2_Eye = CameraRay2*LinearZ;

	float3 N = cross(Pos1_Eye, Pos2_Eye);
	N = normalize(N);
	return	dot(InPos - CameraOrigin.xyz, N);
}

#define THREAD_COUNT 32

[numthreads(THREAD_COUNT, THREAD_COUNT, 1)]
void FillTiledLightList(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	uint LightNum = 0;
	uint StructStride = 0;
	AllLights.GetDimensions(LightNum, StructStride);

	uint2	OutDimensions = (uint2)ViewSizeAndInv.xy / TILE_SIZE;

	float2 LeftTopScreenPos = (float2)Gid.xy / (float2)OutDimensions;
	float2 RightTopScreenPos = (float2)(Gid.xy + uint2(1, 0)) / (float2)OutDimensions;
	float2 RightBtmScreenPos = (float2)(Gid.xy+ uint2(1, 1)) / (float2)OutDimensions;
	float2 LeftBtmScreenPos = (float2)(Gid.xy + uint2(0, 1)) / (float2)OutDimensions;

	uint2	TileUv = Gid.xy;
	uint	TileIdx = Gid.y*OutDimensions.x + Gid.x;

	if (GTid.x == 0 && GTid.y == 0)
	{
		LightListHeader[TileIdx] = -1;
	}

	GroupMemoryBarrierWithGroupSync();

	int i = GTid.y * THREAD_COUNT + GTid.x;
	if( i < LightNum)
	{
		float4 LightPosition = AllLights[i].Position;
		//float4 LightPosition = float4(i / 5 * 155.0f - 500.0f , 110.0f, i % 5 * 155.0f, 100.0f);
		float  LightRadius = LightPosition.w + 50.0f;

		bool	bIsInFrustum = (dot(LightPosition.xyz - CameraOrigin.xyz, CameraZAxis.xyz) > 0.0f)
			&& (DistanceToFrustumPlane(LeftTopScreenPos, RightTopScreenPos, LightPosition.xyz) + LightRadius >= 0.0f)
			&& (DistanceToFrustumPlane(RightTopScreenPos, RightBtmScreenPos, LightPosition.xyz) + LightRadius >= 0.0f)
			&& (DistanceToFrustumPlane(RightBtmScreenPos, LeftBtmScreenPos, LightPosition.xyz) + LightRadius >= 0.0f)
			&& (DistanceToFrustumPlane(LeftBtmScreenPos, LeftTopScreenPos, LightPosition.xyz) + LightRadius >= 0.0f);

		if (bIsInFrustum)
		{
			int	NewNodeAddress = LightList.IncrementCounter();
			
			int	PreNodeAddress = 0;
			InterlockedExchange(LightListHeader[TileIdx], NewNodeAddress, PreNodeAddress);

			LightList[NewNodeAddress].LightIndex = i;
			LightList[NewNodeAddress].NextAddress = PreNodeAddress;
		}
	}

}

#include "PBR.hlsli"
#include "Utils.hlsli"

Texture2D GBufferA : register(t0);
Texture2D GBufferB : register(t1);
Texture2D DepthBuffer : register(t2);
StructuredBuffer<PointLightInfo>	InAllLights : register(t3);
Buffer<int>		InLightListHeader	: register(t4);
StructuredBuffer<LightLinkNode>		InLightList : register(t5);

SamplerState LinearSampler: register(s0);
SamplerState PointSampler : register(s1);

float4 TiledDeferredLighting_PS(QuadVS_Output Input) : SV_TARGET
{
	float4 Albedo = GBufferA.Sample(PointSampler, Input.Uv);
	float4 Normal = GBufferB.Sample(PointSampler, Input.Uv);

	float	Depth = DepthBuffer.Sample(PointSampler, Input.Uv).r;

	float3 N = Normal.xyz*2.0f - 1.0f;
	N = normalize(N);

	float4	WldPos = GetWorldPositionFromDepth(Depth, Input.Uv);

	float3	V = normalize(CameraOrigin.xyz - WldPos.xyz);

	float	roughness = Albedo.a;
	float	metal = Normal.a;

	uint2	TiledDimensions = (uint2)ViewSizeAndInv.xy / TILE_SIZE;
	uint2	TileUv = (uint2)(Input.Uv*ViewSizeAndInv.xy / TILE_SIZE);
	TileUv.x = clamp(TileUv.x, 0, TiledDimensions.x - 1);
	TileUv.y = clamp(TileUv.y, 0, TiledDimensions.y - 1);
	uint	TileIdx = TileUv.y*TiledDimensions.x + TileUv.x;
	
	int		LightAddress = InLightListHeader[TileIdx];

	float4 FinalColor = float4(0.0f, 0.0f, 0.0f, 1.0f);
	int		LightCount = 0;
	while (LightAddress != -1)
	{
		int LightIdx = InLightList[LightAddress].LightIndex;

		//float4 LightPosition = float4(LightIdx / 5 * 155.0f - 500.0f, 110.0f, LightIdx % 5 * 155.0f, 100.0f);
		float4 LightPosition = InAllLights[LightIdx].Position;
		float4 LightColor = InAllLights[LightIdx].Color;

		float3	L = (LightPosition.xyz - WldPos.xyz);
		float	dist = length(L);
		float	distRatio = dist / LightPosition.w;
		float	distMask = clamp(1.0f - distRatio*distRatio, 0.0f, 1.0f);
		distMask *= distMask;
		float	atten = 1.0f / (dist*dist/100000.0f + 1.0f)*distMask;

		FinalColor.rgb += CalculatePBR(normalize(L), N, V, roughness, metal, Albedo.rgb, LightColor.w)*LightColor.rgb*atten;

		LightCount++;
		LightAddress = InLightList[LightAddress].NextAddress;
	}

	return FinalColor;

	//float Debug = (float)LightCount / 3.0f;
	//return float4(Debug, 0.0f, 0.0f, 1.0f);
}
