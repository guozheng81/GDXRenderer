#include "Common.hlsli"

QuadVS_Output VSMain(QuadVS_Input Input)
{
	QuadVS_Output Output;
	Output.Pos = Input.Pos;
	Output.Uv = Input.Uv;
	return Output;
}

Texture2D<float4> SceneColor : register(t0);
SamplerState LinearSampler : register(s0);
SamplerState PointSampler : register(s1);

float4 PSMain(QuadVS_Output Input) : SV_TARGET
{
	float4 Color =  SceneColor.Sample(LinearSampler, Input.Uv);
	return Color;
//	float Z = mProjection._m32 / (Color.r - mProjection._m22);
}

static const float A = 0.15;
static const float B = 0.50;
static const float C = 0.10;
static const float D = 0.20;
static const float E = 0.02;
static const float F = 0.30;
static const float W = 15.0f;

float3 Uncharted2Tonemap(float3 x)
{
	return ((x*(A*x + C*B) + D*E) / (x*(A*x + B) + D*F)) - E / F;
}

float3 ACESFilm(float3 x)
{
	float a = 2.51f;
	float b = 0.03f;
	float c = 2.43f;
	float d = 0.59f;
	float e = 0.14f;
	return saturate((x*(a*x + b)) / (x*(c*x + d) + e));
}

StructuredBuffer<float>	InAverageLuminance : register(t1);

float4 TonemapPS(QuadVS_Output Input) : SV_TARGET
{
	float4 Color = SceneColor.Sample(LinearSampler, Input.Uv);

	if (LightingControl.w < 0.5f)
	{
		return Color;
	}

	float	Lum = 1.0f;

	if (LightingControl.w > 1.5f)
	{
		Lum = InAverageLuminance[0];
	}

	float	Exposure = 1.0f / Lum * LightParams.w;
	Color.rgb *= Exposure;
	//Color.rgb = ACESFilm(Color.rgb);

	Color.rgb = Uncharted2Tonemap(Color.rgb) / Uncharted2Tonemap(W);
	return Color;
}

Texture2D<float4>	InHDRBuffer : register(t0);
RWStructuredBuffer<uint>	OutHistogram : register(u0);

#define HISTOGRAM_BIN_NUM 64
static const float histogram_min_lum = -8.0f;
static const float histogram_max_lum = 6.0f;

[numthreads(16, 16, 1)]
void HistogramCS(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	if (any(DTid.xy >= uint2(ViewSizeAndInv.xy)))
	{
		return;
	}

	float4 Color = InHDRBuffer[DTid.xy];
	float Luminance = (Color.r*0.3f + Color.g*0.59f + Color.b*0.11f);
	float LumLog = log2(Luminance);
	uint	BinIdx = (uint)(saturate((LumLog - histogram_min_lum) / (histogram_max_lum - histogram_min_lum))*HISTOGRAM_BIN_NUM);
	BinIdx = min(BinIdx, HISTOGRAM_BIN_NUM-1);

	InterlockedAdd(OutHistogram[BinIdx], 1);
}

StructuredBuffer<uint>	InHistogram : register(t0);
RWStructuredBuffer<float>	OutLuminance : register(u0);

[numthreads(1, 1, 1)]
void AverageLuminance(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	if (DTid.x == 0 && DTid.y == 0 && DTid.z == 0)
	{
		float	TotalValue = 0;
		for (int k = 0; k < HISTOGRAM_BIN_NUM; ++k)
		{
			TotalValue += (float)InHistogram[k];
		}

		float	minRemain = (float)TotalValue*0.8f;
		float	maxRemain = (float)TotalValue*0.983f;
		float	LuminanceSum = 0.0f;
		float	TotalValueInRange = 0.0f;

		for (int i = 0; i < HISTOGRAM_BIN_NUM; ++i)
		{
			float CurValue = (float)InHistogram[i];

			float Sub = min(CurValue, minRemain);
			CurValue -= Sub;
			minRemain -= Sub;
			maxRemain -= Sub;

			CurValue = min(CurValue, maxRemain);
			maxRemain -= CurValue;

			float	Lum = exp2(((float)i / (HISTOGRAM_BIN_NUM)*(histogram_max_lum - histogram_min_lum) + histogram_min_lum));

			LuminanceSum += CurValue*Lum;
			TotalValueInRange += CurValue;
		}

		float	NewLum = LuminanceSum / max(TotalValueInRange, 0.00001f);
		float	CurLum = OutLuminance[0];
		OutLuminance[0] = lerp(CurLum, NewLum, 0.02f);
	}
}

struct DOF_QuadVS_Output
{
	float4 Pos : SV_POSITION;
	float2 Uv : TEXCOORD0;
	float4 FarColor : TEXCOORD1;
	float4 NearColor : TEXCOORD2;
};

Texture2D<float4>	InCocTexture : register(t0);

DOF_QuadVS_Output Bokeh_VS(uint VID : SV_VertexID, uint IID : SV_InstanceID)
{
	DOF_QuadVS_Output Output;

	float2 Pos = float2(VID % 2, (VID / 2));
	Output.Uv = Pos;

	uint	ViewWidth = (uint)(ViewSizeAndInv.x)/2;
	uint	ViewHeight = (uint)(ViewSizeAndInv.y)/2;

	float2  screenPos = float2(IID%ViewWidth, IID / ViewWidth);

	float4  downSampleColor = InCocTexture.SampleLevel(LinearSampler, screenPos*(ViewSizeAndInv.zw * 2.0f), 0);
	float	Coc = downSampleColor.w*2.0f - 1.0f;
	float	CocSize = abs(Coc)*DOFParams.w;
	Pos -= 0.5f;
	Pos *= CocSize;
	Pos += 0.5f;

	screenPos += Pos;
	screenPos *= (ViewSizeAndInv.zw * 2.0f);
	screenPos.x = screenPos.x*2.0f - 1.0f;
	screenPos.y = 1.0f - screenPos.y*2.0f;

	Output.Pos = float4(screenPos, 0.0f, 1.0f);
	if (CocSize < 1.0f)
	{
		Output.Pos.w = -1.0f;
	}

	float a = 1.0f / max(CocSize*CocSize, 1.0f);

	Output.FarColor = Coc > 0.0f ? float4(downSampleColor.rgb, a) : float4(0.0f, 0.0f, 0.0f, 0.0f);
	Output.NearColor = Coc < 0.0f ? float4(downSampleColor.rgb, a) : float4(0.0f, 0.0f, 0.0f, 0.0f);

	return Output;
}

struct DOF_PS_OUTPUT
{
	float4 FarColor : SV_TARGET0;
	float4 NearColor : SV_TARGET1;
};


DOF_PS_OUTPUT Bokeh_PS(DOF_QuadVS_Output Input)
{
	DOF_PS_OUTPUT Output;

	float	len = length(Input.Uv*2.0f - 1.0f);
	float	BokehShape = 1.0f - smoothstep(0.9f, 0.95f, len);
	
	Input.FarColor.rgb *= Input.FarColor.a;
	Output.FarColor = Input.FarColor*BokehShape;
	Input.NearColor.rgb *= Input.NearColor.a;
	Output.NearColor = Input.NearColor*BokehShape;

	return Output;
}

Texture2D ColorBuffer : register(t0);
Texture2D DepthBuffer : register(t1);

float4 COC_PS(QuadVS_Output Input) : SV_TARGET
{
	float4	Color = ColorBuffer.SampleLevel(LinearSampler, Input.Uv, 0);
	float4	Depth = DepthBuffer.GatherRed(PointSampler, Input.Uv);

	float4 LinearZ4 = mProjection._m32 / (Depth - mProjection._m22);
	float	LinearZ = dot(LinearZ4, float4(1.0f, 1.0f, 1.0f, 1.0f)) / 4.0f;

	float	COC = 0.0f;
	if (LinearZ > DOFParams.y)
	{
		COC = (LinearZ - DOFParams.y) / (DOFParams.z - DOFParams.y);
	}
	else
	{
		COC = (LinearZ - DOFParams.y) / (DOFParams.y - DOFParams.x);
	}

	COC = saturate((COC + 1.0f)*0.5f);

	return float4(Color.rgb, COC);
}

Texture2D<float4> InSceneColor : register(t0);
Texture2D<float4> InCoc : register(t1);
Texture2D<float4> InFarColor : register(t2);
Texture2D<float4> InNearColor : register(t3);

float4 DOF_PS(QuadVS_Output Input) : SV_TARGET
{
	float4	sceneColor = InSceneColor.SampleLevel(LinearSampler, Input.Uv, 0);

	float4  downSampleColor = InCoc.SampleLevel(LinearSampler, Input.Uv, 0);
	float	Coc = downSampleColor.w*2.0f - 1.0f;
	float	CocSize = Coc*DOFParams.w;

	float4	farColor = InFarColor.SampleLevel(LinearSampler, Input.Uv, 0);
	farColor.rgb /= max(farColor.a, 0.0001f);
	float4	nearColor = InNearColor.SampleLevel(LinearSampler, Input.Uv, 0);
	nearColor.rgb /= max(nearColor.a, 0.0001f);

	float4  combineColor = lerp(sceneColor, farColor, saturate(CocSize - 1.0f));

	float blendNear = saturate(-CocSize - 1.0f);
	combineColor = lerp(combineColor, nearColor, blendNear*blendNear);
	return combineColor;
}

Texture2D<float4>	InAOColor : register(t0);
float4  AOBlur0_PS(QuadVS_Output Input) : SV_TARGET
{
	const float W[9] = { 0.0162162162f, 0.0540540541f,0.1216216216f, 0.1945945946f, 0.2270270270f, 0.1945945946f,0.1216216216f, 0.0540540541f, 0.0162162162f };
	const float Offsets[9] = { -4.0f, -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f, 4.0f };

	float4 OutColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float	TotalW = 0.0f;
	float	Depth = DepthBuffer.Sample(PointSampler, Input.Uv).r;
	float	LinearZ = mProjection._m32 / (Depth - mProjection._m22);

	for (int i = 0; i < 9; ++i)
	{
		float2 PixelOffset = ViewSizeAndInv.zw*float2(Offsets[i], 0.0f);

		float2	SampleUV = Input.Uv + PixelOffset;
		float	CurDepth = DepthBuffer.Sample(PointSampler, SampleUV).r;
		float	CurLinearZ = mProjection._m32 / (CurDepth - mProjection._m22);
		float	Wi = W[i]*step(abs(CurLinearZ - LinearZ), 15.0f);

		OutColor += InAOColor.SampleLevel(LinearSampler, Input.Uv + PixelOffset, 0)*Wi;
		TotalW += Wi;
	}
	OutColor.rgb /= TotalW;
	OutColor.a = 1.0f;

	return OutColor;
}

float4  AOBlur1_PS(QuadVS_Output Input) : SV_TARGET
{
	const float W[9] = { 0.0162162162f, 0.0540540541f,0.1216216216f, 0.1945945946f, 0.2270270270f, 0.1945945946f,0.1216216216f, 0.0540540541f, 0.0162162162f };
	const float Offsets[9] = { -4.0f, -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f, 4.0f };

	float4 OutColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float	TotalW = 0.0f;
	float	Depth = DepthBuffer.Sample(PointSampler, Input.Uv).r;
	float	LinearZ = mProjection._m32 / (Depth - mProjection._m22);

	for (int i = 0; i < 9; ++i)
	{
		float2 PixelOffset = ViewSizeAndInv.zw*float2(0.0f, Offsets[i]);

		float2	SampleUV = Input.Uv + PixelOffset;
		float	CurDepth = DepthBuffer.Sample(PointSampler, SampleUV).r;
		float	CurLinearZ = mProjection._m32 / (CurDepth - mProjection._m22);

		float	Wi = W[i] * step(abs(CurLinearZ - LinearZ), 15.0f);
		OutColor += InAOColor.SampleLevel(LinearSampler, Input.Uv + PixelOffset, 0)*Wi;
		TotalW += Wi;
	}
	OutColor.rgb /= TotalW;
	OutColor.a = 1.0f;

	return OutColor;
}