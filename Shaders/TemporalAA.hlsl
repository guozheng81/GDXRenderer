#include "Common.hlsli"

Texture2D<float4> SceneColor : register(t0);
Texture2D<float2> VelocityTexture : register(t1);
Texture2D<float4> HistoryTexture : register(t2);

SamplerState LinearSampler : register(s0);
SamplerState PointSampler : register(s1);

// https://github.com/playdeadgames/temporal
float3 clip_aabb(float3 aabb_min, float3 aabb_max, float3 q)
{
	// note: only clips towards aabb center (but fast!)
	float3 p_clip = 0.5 * (aabb_max + aabb_min);
	float3 e_clip = 0.5 * (aabb_max - aabb_min) + 0.00001f;

	float3 v_clip = q - p_clip;
	float3 v_unit = v_clip.xyz / e_clip;
	float3 a_unit = abs(v_unit);
	float ma_unit = max(a_unit.x, max(a_unit.y, a_unit.z));

	if (ma_unit > 1.0)
		return p_clip + v_clip / ma_unit;
	else
		return q;// point inside aabb
}

float4 TemporalAAPS(QuadVS_Output Input) : SV_TARGET
{
	float2 UnjitteredUv = Input.Uv;
	UnjitteredUv.x -= TemporalJitterParams.x*0.5f;
	UnjitteredUv.y += TemporalJitterParams.y*0.5f;

	float2 V = VelocityTexture.SampleLevel(LinearSampler, UnjitteredUv, 0).xy / 5.0f;
	float4 HistoryColor = HistoryTexture.SampleLevel(LinearSampler, Input.Uv-V, 0);

	float2	SampleUv = UnjitteredUv;// Input.Uv;

	float4 mtColor = SceneColor.SampleLevel(PointSampler, SampleUv + float2(0.0f, -ViewSizeAndInv.w), 0);
	float4 mbColor = SceneColor.SampleLevel(PointSampler, SampleUv + float2(0.0f, ViewSizeAndInv.w), 0);
	float4 lmColor = SceneColor.SampleLevel(PointSampler, SampleUv + float2(-ViewSizeAndInv.z, 0.0f), 0);
	float4 rmColor = SceneColor.SampleLevel(PointSampler, SampleUv + float2(ViewSizeAndInv.z, 0.0f), 0);
	float4 mmColor = SceneColor.SampleLevel(PointSampler, SampleUv, 0);

	float4 minColor = min(min(min(mtColor, mbColor), min(lmColor, rmColor)), mmColor);
	float4 maxColor = max(max(max(mtColor, mbColor), max(lmColor, rmColor)), mmColor);

	HistoryColor.rgb = clip_aabb(minColor, maxColor, HistoryColor.rgb);

	float4 Color = SceneColor.SampleLevel(LinearSampler, Input.Uv, 0);

	float3	LumWeight = float3(0.299f, 0.587f, 0.114f);
	float	HistoryLum = dot(HistoryColor.rgb, LumWeight);
	float	CurLum = dot(Color.rgb, LumWeight);

	float	HistoryW = 0.9f;
	float	CurW = 1.0f - HistoryW;

	CurW /= (1.0f + CurLum);
	HistoryW /= (1.0f + HistoryLum);

	float	W = HistoryW + CurW;
	CurW /= W;
	HistoryW /= W;

	Color = Color*CurW + HistoryColor*HistoryW;
	return Color;
}
