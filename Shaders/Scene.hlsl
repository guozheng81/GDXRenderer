#include "Common.hlsli"
#include "CommonHelper.hlsli"

#ifndef SKY_BOX
#define SKY_BOX 0
#endif

//--------------------------------------------------------------------------------------
// Input / Output structures
//--------------------------------------------------------------------------------------
struct VS_INPUT
{
	float4 Position	: POSITION;
	float3 Normal		: NORMAL;
	float3 Tangent		: TANGENT;
	float2 Texcoord	: TEXCOORD0;
};

struct VS_OUTPUT
{
	float3 Normal		: NORMAL;
	float2 Texcoord	: TEXCOORD0;
	float3 Tangent		: TEXCOORD1;
	float4 WorldPos : TEXCOORD2;
	float4 Position	: SV_POSITION;
};

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
VS_OUTPUT VSMain( VS_INPUT Input )
{
	VS_OUTPUT Output;

	float4 LocalPos = Input.Position;
	#if SKY_BOX
	LocalPos.xyz = Input.Position.xyz*4500.0f + CameraOrigin;
	#endif
	
	float4x4 mWorldViewProjection = mul(mWorld, mViewProjection);
	Output.Position = mul(LocalPos, mWorldViewProjection);

	Output.Normal = mul( Input.Normal, (float3x3)mWorld );
	Output.Tangent = mul(Input.Tangent, (float3x3)mWorld);
	Output.Texcoord = Input.Texcoord;
	Output.WorldPos = mul(float4(Input.Position.xyz, 1.0f), mWorld);
	
	return Output;
}

VS_OUTPUT RSMVSMain(VS_INPUT Input)
{
	VS_OUTPUT Output;

	float4 LocalPos = Input.Position;

	Output.Position = mul(LocalPos, mRSMViewProjection);

	Output.Normal = mul(Input.Normal, (float3x3)mWorld);
	Output.Tangent = mul(Input.Tangent, (float3x3)mWorld);
	Output.Texcoord = Input.Texcoord;
	Output.WorldPos = mul(float4(Input.Position.xyz, 1.0f), mWorld);

	return Output;
}


Texture2D	DiffuseTexture : register(t0);
Texture2D	NormalTexture : register(t1);

SamplerState LinearSampler : register(s0);
SamplerState PointSampler : register(s1);
SamplerState LinearSamplerWrap : register(s2);
SamplerState PointSamplerWrap : register(s3);

struct PS_INPUT
{
	float3 Normal		: NORMAL;
	float2 Texcoord	: TEXCOORD0;
	float3 Tangent		: TEXCOORD1;
	float4 WorldPos : TEXCOORD2;
};

#include "PBR.hlsli"
#include "Utils.hlsli"

TextureCube<float4> SkyCubeMap: register(t0);

float4 PSSky(PS_INPUT Input) : SV_TARGET
{
	float3 Dir = normalize(Input.WorldPos.xyz);
	return SkyCubeMap.SampleLevel(LinearSampler, Dir, 0);
}

struct PS_OUTPUT
{
	float4 Albedo : SV_TARGET0;
	float4 Normal : SV_TARGET1;
};

#include "VoxelConeTracer.hlsli"

PS_OUTPUT PSMain(PS_INPUT Input)
{
	float4 Albedo = DiffuseTexture.Sample(LinearSamplerWrap, Input.Texcoord);

	if (Albedo.a < 0.5f)
	{
		discard;
	}

	float3 NormalColor = NormalTexture.Sample(LinearSamplerWrap, Input.Texcoord).rgb;
	NormalColor = (NormalColor*2.0f - 1.0f);

	float3 B = cross(Input.Tangent, Input.Normal);
	float3 N = Input.Tangent*NormalColor.x + B*NormalColor.y + Input.Normal*NormalColor.z;
	N = normalize(N);

	PS_OUTPUT Output;
	Albedo.a = MaterialParams.r;
	Output.Albedo = Albedo;
	Output.Normal = float4((N + 1.0f)*0.5f, MaterialParams.g);
	return Output;
}

struct GS_OUTPUT
{
	float4 ScreenPos : SV_POSITION;
	uint	RTIdx : SV_RenderTargetArrayIndex;
};

[maxvertexcount(9)]
void ShadowGS(triangle VS_OUTPUT Input[3], inout TriangleStream<GS_OUTPUT> OutputStream)
{
	GS_OUTPUT Output;
	for (uint i = 0; i < 3; ++i)
	{
		Output.RTIdx = i;

		for (uint k = 0; k < 3; ++k)
		{
			Output.ScreenPos = mul(Input[k].WorldPos, ShadowViewProj[i]);
			OutputStream.Append(Output);
		}
		OutputStream.RestartStrip();
	}
}

Texture2D GBufferA : register(t0);
Texture2D GBufferB : register(t1);
Texture2D DepthBuffer : register(t2);
StructuredBuffer<SH_RGB> skyLightDiffuse : register(t3);
TextureCube	skyLightSpecular : register(t4);
Texture2D	skyLightSpecularIntegration : register(t5);
Texture2DArray	ShadowBuffers : register(t6);
Texture2D	AOBuffer : register(t7);

float3	CalculateSkyLight(float4 Albedo, float3 N, float3 V, float roughness, float metal, float Shadow)
{
	// sky light
	float3 SkyLight;

	float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), Albedo.rgb, metal);

	// Skylight.cpp, total 5 levels from [0.05f, 1.0f]
	float	roughness_mipmap_level = clamp((roughness - 0.05f) / (1.0f - 0.05f) / 0.25f, 0.0f, 4.0f);

	float2 Integration = skyLightSpecularIntegration.SampleLevel(LinearSampler, float2(max(abs(dot(N, V)), 0.001f), roughness), 0).rg;
	float3 Specular = skyLightSpecular.SampleLevel(LinearSampler, reflect(-V, N), roughness_mipmap_level).rgb*(Integration.x*F0 + saturate(50.0f*F0.g)*Integration.y)/ 3.1415f;

	float4	SHTransfer = SHCosTransfer(N);
	SkyLight.r = dot(skyLightDiffuse[0].R, SHTransfer);
	SkyLight.g = dot(skyLightDiffuse[0].G, SHTransfer);
	SkyLight.b = dot(skyLightDiffuse[0].B, SHTransfer);

	SkyLight.rgb *= Albedo.rgb;
	SkyLight.rgb += Specular*Shadow;

	return SkyLight;
}

#include "LPVHelper.hlsli"

float4 PSDirAndSkyLight(QuadVS_Output Input) : SV_TARGET
{
	float4 Albedo = GBufferA.Sample(PointSampler, Input.Uv);
	float4 Normal = GBufferB.Sample(PointSampler, Input.Uv);

	float	Depth = DepthBuffer.Sample(PointSampler, Input.Uv).r;

	float3 N = Normal.xyz*2.0f - 1.0f;
	if (length(N) < 0.001f)
	{
		discard;
	}
	N = normalize(N);

	float4	WldPos = GetWorldPositionFromDepth(Depth, Input.Uv);

	float3	L = normalize(LightParams.xyz);
	float3	V = normalize(CameraOrigin.xyz - WldPos.xyz);

	float	roughness = Albedo.a;
	float	metal = Normal.a;

/*	float4	ViewSpacePos = mul(WldPos, mView);

	const int CascadeCount = 3;
	int		CascadeIdx = 0;
	float	ViewDepth = ViewSpacePos.z;
	for (CascadeIdx = 0; CascadeIdx < CascadeCount-1; ++CascadeIdx)
	{
		if (ViewDepth <= CascadeShadowDistances[CascadeIdx])
		{
			break;
		}
	}
*/

//	float test = ShadowBuffers.SampleLevel(LinearSampler, float3(Input.Uv.x, Input.Uv.y, 2), 0).r;
//	return float4(test, 0.0f, 0.0f, 1.0f);

	float4 AOColor = AOBuffer.Sample(PointSampler, Input.Uv);
	float Shadow = AOColor.b;

/*
	if (CascadeIdx < CascadeCount - 1)
	{
		float TransitDist = CascadeShadowDistances[3];
		if(ViewDepth > CascadeShadowDistances[CascadeIdx] - TransitDist && ViewDepth <CascadeShadowDistances[CascadeIdx])
		{
			float Transit = clamp((ViewDepth - (CascadeShadowDistances[CascadeIdx] - TransitDist)) / TransitDist, 0.0f, 1.0f);
			float nextShadow = GetCascadeShadow(CascadeIdx + 1, WldPos, Input.Uv, 0.002f);
			Shadow = Shadow*(1.0f - Transit) + nextShadow*Transit;
		}
	}
*/
	float4 FinalColor;
	FinalColor.rgb = CalculatePBR(L, N, V, roughness, metal, Albedo.rgb, LightingControl.x)*Shadow;

	// sky light
	float3 SkyLight = CalculateSkyLight(Albedo, N, V, roughness, metal, Shadow);
	FinalColor.rgb += SkyLight.rgb*LightingControl.y;

	float	ao = AOColor.r;
	FinalColor.rgb *= ao;

	if (LightingControl.z > 1.5f)
	{
		float4 LPVColor = SampleVpl(LinearSampler, WldPos, N);
		FinalColor.rgb += LPVColor.rgb*Albedo.rgb;
	}
	else if(LightingControl.z > 0.5f)
	{
		float4  ConeTracedColor = TraceVoxelHemisphere(WldPos, N, LinearSampler);
		FinalColor.rgb += ConeTracedColor*2.5f*Albedo.rgb;

		// cone traced ao
		//ao = ConeTracedColor.a;
	}

	//return float4(ao, ao, ao, 1.0f);

	FinalColor.a = 1.0f;
	return FinalColor;
}

//RWTexture3D<uint>    OutputVoxelTexture	: register(u1);
RWTexture3D<uint>    OutputVoxelAddressTexture	: register(u1);
RWStructuredBuffer<float4>	OutVoxelBuffer : register(u2);

float4 VoxelPs(PS_INPUT Input) : SV_TARGET
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

	float	roughness = MaterialParams.r;
	float	metal = MaterialParams.g;

	int	CascadeIdx = GetCascadeIndex(WldPos);
	float Shadow = GetCascadeShadowSimple(ShadowBuffers, PointSampler, CascadeIdx, WldPos, 0.002f);

	float4 FinalColor;
	float	SimpleLighting = max(dot(N, L), 0.0f)*Shadow*LightingControl.x;
	FinalColor.rgb = Albedo.rgb*SimpleLighting;
//	FinalColor.rgb = CalculatePBR(L, N, V, roughness, metal, Albedo.rgb, 10.0f)*Shadow;
	FinalColor.a = 1.0f;
	FinalColor.rgb = clamp(FinalColor.rgb, 0.0f, 1.0f);

/*
	uint	r = (uint)(FinalColor.r*255.0f);
	uint	g = (uint)(FinalColor.g*255.0f);
	uint	b = (uint)(FinalColor.b*255.0f);

	uint	Lum = (uint)(clamp((FinalColor.r*0.3f + FinalColor.g*0.59f + FinalColor.b*0.11f), 0.0f, 1.0f)*63.0f);
//	uint	Lum = (uint)(SimpleLighting*63.0f);
	uint	packed_value = (Lum << 25) | (1 << 24) | (r << 16) | (g << 8) | b;
*/

	float3 NormalizedPos = (WldPos - SceneBoundMin.xyz) / (SceneBoundMax.xyz - SceneBoundMin.xyz) * VoxelTexScale + VexelTexBias;
	if (all(NormalizedPos > 0.0f) && all(NormalizedPos < 1.0f))
	{
		NormalizedPos = saturate(NormalizedPos)*VCTVolumeSize.xyz;
		uint3	VoxelPos = uint3(NormalizedPos);

		uint	NewNodeAddress = OutVoxelBuffer.IncrementCounter();
		NewNodeAddress += 1;

		uint	PreNodeAddress = 0;
		InterlockedExchange(OutputVoxelAddressTexture[VoxelPos], NewNodeAddress, PreNodeAddress);

		OutVoxelBuffer[NewNodeAddress-1] = float4(FinalColor.rgb, PreNodeAddress);

		//	InterlockedMax(OutputVoxelTexture[VoxelPos], packed_value);
	}


	return FinalColor;
}

