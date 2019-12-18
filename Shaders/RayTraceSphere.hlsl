#include "Common.hlsli"
#include "CommonHelper.hlsli"
#include "PBR.hlsli"
#include "Utils.hlsli"

//https://github.com/petershirley/raytracinginoneweekend
//https://github.com/aras-p/ToyPathTracer

struct HitableObject
{
	float4 Position;
	float4 Color;
	float4 MaterialParams;
};

struct HitPayload
{
	float	Dist;
	float3 Position;
	float3 Normal;
	float4 Color;
	float4 MaterialParams;
};

struct Ray
{
	float3 Origin;
	float3 Direction;
	float3 DiffuseBRDF;
	float3 SpecularBRDF;
	uint	SampleDiffuse;
	uint	Depth;
	uint  rngState;
	int	  Next;
};

#define OBJECT_NUM 9

StructuredBuffer<HitableObject> InHitableList : register(t0);
Texture2D<float4> HistoryTexture : register(t1);
Buffer<int> InRayHeaderBuffer : register(t2);
StructuredBuffer<Ray>	InRayBuffer : register(t3);

RWTexture2D<float4>	OutputTexture : register(u0);
RWBuffer<int> OutRayHeaderBuffer : register(u1);
RWStructuredBuffer<Ray>	OutRayBuffer : register(u2);

groupshared HitableObject g_HitableList[OBJECT_NUM];

bool sphere_hit(float3 ray_origin, float3 ray_dir, float3 center, float radius, HitableObject hitable_obj, float t_min, float t_max, out HitPayload rec)
{
	float3 oc = ray_origin - center;
	float a = dot(ray_dir, ray_dir);
	float b = dot(oc, ray_dir);
	float c = dot(oc, oc) - radius*radius;
	float discriminant = b*b - a*c;
	if (discriminant > 0) {
		float temp = (-b - sqrt(discriminant)) / a;
		if (temp < t_max && temp > t_min) {
			rec.Dist = temp;
			rec.Position = ray_origin + ray_dir*temp;
			rec.Normal = (rec.Position - center) / radius;
			rec.Color = hitable_obj.Color;
			rec.MaterialParams = hitable_obj.MaterialParams;
			return true;
		}
		temp = (-b + sqrt(discriminant)) / a;
		if (temp < t_max && temp > t_min) {
			rec.Dist = temp;
			rec.Position = ray_origin + ray_dir*temp;
			rec.Normal = (rec.Position - center) / radius;
			rec.Color = hitable_obj.Color;
			rec.MaterialParams = hitable_obj.MaterialParams;
			return true;
		}
	}
	return false;
}

bool sphere_list_hit(float3 ray_origin, float3 ray_dir, float t_min, float t_max, out HitPayload rec)
{

	float	minDist = 1000000.0f;
	bool	HasHit = false;
	for (uint i = 0; i < OBJECT_NUM; ++i)
	{
		HitPayload CurHit;
		if (sphere_hit(ray_origin, ray_dir, g_HitableList[i].Position.xyz, g_HitableList[i].Position.w, g_HitableList[i], t_min, t_max, CurHit))
		{
			HasHit = true;
			if (CurHit.Dist < minDist)
			{
				minDist = CurHit.Dist;
				rec = CurHit;
			}
		}
	}
	return HasHit;
}


float schlick(float cos_angle, float refract_idx)
{
	float r0 = (1.0f - refract_idx) / (1.0f + refract_idx);
	r0 = r0*r0;

	return r0 + (1.0f - r0)*pow((1.0f - cos_angle), 5.0f);
}

bool scatter(float3 ray_origin, float3 ray_dir, HitPayload rec, inout uint rngState, out float3 attenuation, out float3 scattered_dir)
{
	if (rec.MaterialParams.w < 0.5f)
	{
		scattered_dir = normalize(rec.Normal + RandInSphere(rngState));
		attenuation = rec.Color.rgb;
		return true;
	}
	else if (rec.MaterialParams.w < 1.5f)
	{
		scattered_dir = normalize(reflect(ray_dir, rec.Normal) + RandInSphere(rngState)*(1.0f-rec.MaterialParams.y));
		attenuation = rec.Color.rgb;

		return (dot(scattered_dir, rec.Normal) > 0.0f);
	}
	else
	{
		float3	refract_normal = rec.Normal;
		float	refract_idx = 1.0f / rec.MaterialParams.z;
		float3	reflected_dir = normalize(reflect(ray_dir, rec.Normal));
		float	reflect_chance = 1.0f;
		float	VdotN = dot(-ray_dir, rec.Normal);

		attenuation = float3(1.0f, 1.0f, 1.0f);
		if (dot(rec.Normal, ray_dir) > 0.0f)
		{
			refract_normal = -rec.Normal;
			refract_idx = rec.MaterialParams.z;
			VdotN = dot(-ray_dir, -rec.Normal)*refract_idx;
		}

		float3	refracted_dir = refract(ray_dir, refract_normal, refract_idx);
		if (length(refracted_dir) < 0.0001f)
		{
			reflect_chance = 1.0f;
		}
		else
		{
			reflect_chance = schlick(VdotN, refract_idx);
		}

		if (RandomFloat01(rngState) < reflect_chance)
		{
			scattered_dir = normalize(reflect(ray_dir, refract_normal));
		}
		else
		{
			scattered_dir = normalize(refracted_dir);
		}
		return true;
	}

	return false;
}

float4 ray_trace(float3 ray_origin, float3 ray_dir, inout uint rngState)
{
	float3 L = normalize(float3(1.0f, 1.0f, 1.0f));

	float4 finalColor = float4(0.0f, 0.0f, 0.0f, 1.0f);
	float3	currentAttenuation = float3(1.0f, 1.0f, 1.0f);
	float3	accumAttenuation = float3(1.0f, 1.0f, 1.0f);
	for (uint rayDepth = 0; rayDepth < 10; ++rayDepth)
	{
		HitPayload	HitRes;
		if (sphere_list_hit(ray_origin, ray_dir, 0.05f, 1000000.0f, HitRes))
		{
			if (HitRes.MaterialParams.w < 0.5f)
			{
				float3	LightPos = float3(0.0f, 600.0f, 0.0f);
				LightPos.xz += LightParams.xz*500.0f;

				float3 LightDir = normalize(HitRes.Position - LightPos);

				float3	L = (LightPos - HitRes.Position);
				float dist = length(L);
				L = normalize(L);

				HitPayload CurHit;
				if (!sphere_list_hit(HitRes.Position, L, 0.05f, dist, CurHit))
				{
					finalColor.rgb += accumAttenuation*max(dot(HitRes.Normal, L), 0.0f)*25.0f*HitRes.Color.rgb;
				}
			}

			float3	atten = float3(1.0f, 1.0f, 1.0f);
			float3	scattered_dir = ray_dir;
			if (scatter(ray_origin, ray_dir, HitRes, rngState, atten, scattered_dir))
			{
				ray_origin = HitRes.Position;
				ray_dir = scattered_dir;

				currentAttenuation = atten;
				accumAttenuation *= atten;
			}
			else
			{
				break;
			}
		}
		else
		{
			finalColor.rgb += float3(0.4f, 0.4f, 1.0f)*accumAttenuation;
			break;
		}
	}

	return finalColor;
}


#define RayNumPerPixel 4

[numthreads(16, 16, 1)]
void RayTraceCS(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	uint Idx = GTid.x * 16 + GTid.y;
	if (Idx < OBJECT_NUM)
	{
		g_HitableList[Idx] = InHitableList[Idx];
	}

	GroupMemoryBarrierWithGroupSync();

	float2 uv = (DTid.xy + 0.5f) / ViewSizeAndInv.xy;

	float2 ScreenPos;
	ScreenPos.x = uv.x*2.0f - 1.0f;
	ScreenPos.y = 1.0f - uv.y*2.0f;

	uint rngState = wang_hash((DTid.x + DTid.y * (uint)(ViewSizeAndInv.x) + (uint)FrameNum.x*(uint)(ViewSizeAndInv.x*ViewSizeAndInv.y)));

	float4	TotalColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
	for (uint rayIdx = 0; rayIdx < RayNumPerPixel; rayIdx++)
	{
		float subPixelOffsetX = (RandomFloat01(rngState)*2.0f - 1.0f)*ViewSizeAndInv.z;
		float subPixelOffsetY = (RandomFloat01(rngState)*2.0f - 1.0f)*ViewSizeAndInv.w;

		float3 CameraRay = normalize(CameraXAxis.xyz*(ScreenPos.x+ subPixelOffsetX) + CameraYAxis.xyz*(ScreenPos.y+ subPixelOffsetY) + CameraZAxis.xyz);
		TotalColor += ray_trace(CameraOrigin.xyz, CameraRay, rngState);
	}

	float4 HistoryColor = HistoryTexture[DTid.xy];
	float4 CurColor = TotalColor / RayNumPerPixel;
	float	HistoryW = 0.9f;
	OutputTexture[DTid.xy] = CurColor *(1.0f - HistoryW) + HistoryColor*HistoryW;
}

bool scatter_hemisphere(float3 ray_origin, float3 ray_dir, HitPayload rec, inout uint rngState, out float3 scattered_dir, out bool sample_diffuse, out float diffuse_chance)
{
	float3	N = normalize(rec.Normal);

	float3 up = abs(N.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
	float3 T = normalize(cross(up, N));
	float3 B = cross(N, T);

	float3 albedo = rec.Color.rgb;
	float3 specularColor = lerp(float3(0.04f, 0.04f, 0.04f), albedo, rec.MaterialParams.y);

	float lumDiffuse = max(0.01f, dot(albedo, float3(0.299f, 0.587f, 0.114f)));
	float lumSpecular = max(0.01f, dot(specularColor, float3(0.299f, 0.587f, 0.114f)));
	diffuse_chance = lumDiffuse / (lumDiffuse + lumSpecular);

	sample_diffuse = (RandomFloat01(rngState) < diffuse_chance);

	float u = RandomFloat01(rngState);
	float v = RandomFloat01(rngState);

	if (sample_diffuse)
	{
		float3 sample_dir = hemisphereSample_cos(u, v);

		scattered_dir = normalize(sample_dir.z*N + sample_dir.x*T + sample_dir.y*B);

		float pdf = dot(N, scattered_dir) / PI;

	}
	else
	{
		float pdf = 1.0f;
		float3 sample_dir = hemisphereSample_GGX(float2(u, v), rec.MaterialParams.x, pdf);

		float3 H = normalize(sample_dir.z*N + sample_dir.x*T + sample_dir.y*B);
		float3 V = (-ray_dir);
		scattered_dir = normalize(2.f * dot(V, H) * H - V);

	}

	return true;
}

float3	RayMissShading(float3 AccumAttenuation)
{
	return float3(0.0f, 0.0f, 0.0f);
//	return float3(0.4f, 0.4f, 1.0f)*AccumAttenuation;
}

/*
float2 poissonDisk[4] = {
float2(-0.94201624f, -0.39906216f),
float2(0.94558609f, -0.76890725f),
float2(-0.094184101f, -0.92938870f),
float2(0.34495938f, 0.29387760f)
};

float2 poissonDisk[19] =
{
//float2(0.0f, 0.0f),
float2(-0.913637417379f, -0.39201712298f),
float2(0.597109626106f, 0.772134734643f),
float2(0.832961355927f, 0.0999080784288f),
float2(0.0507237968516f, -0.986328460427f),
float2(0.831213944606f, -0.554988666148f),
float2(-0.433961834033f, 0.830242645908f),
float2(-0.374497057982f, -0.722719996686f),
float2(-0.0845827262662f, 0.364290041594f),
float2(-0.758577248372f, 0.0126646127217f),
float2(0.267715036916f, -0.363466095304f),
float2(0.291318297389f, 0.368761920866f),
float2(-0.473999302807f, -0.303207622949f),
float2(0.516504756772f, -0.827533722604f),
float2(0.191397162978f, 0.757781091116f),
float2(-0.942805497434f, 0.325320891913f),
float2(0.503718277072f, -0.142206433619f),
float2(0.92639800923f, -0.218479153066f),
float2(0.80364886067f, 0.508373493172f),
float2(-0.498046279304f, 0.291619367277f),
};
*/

float3	RayHitShading(float3 V, HitPayload	HitRes, float RayDepth, float InShadowSampleRadius, float2 InScreenUv)
{
	V = normalize(V);
	float3	N = HitRes.Normal;

	float3	LightPos = float3(0.0f, 600.0f, 0.0f);
	LightPos.xz += LightParams.xz*500.0f;

	float3 LightDir = normalize(HitRes.Position - LightPos);

	float shadowTerm = 0.0f;
	if (InShadowSampleRadius < 0.01f)
	{
		float3	L = (LightPos - HitRes.Position);
		float dist = length(L);
		L = normalize(L);

		HitPayload CurHit;
		if (!sphere_list_hit(HitRes.Position, L, 0.05f, dist, CurHit))
		{
			shadowTerm += 1.0f;
		}
	}
	else
	{
		float3 up = abs(LightDir.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
		float3 T = normalize(cross(up, LightDir));
		float3 B = cross(LightDir, T);

		const float2 poissonDisk[8] =
		{ 
			float2(0.373838022357f, 0.662882019975f),
			float2(-0.335774814282f, -0.940070127794f),
			float2(-0.9115721822f, 0.324130702404f),
			float2(0.837294074715f, -0.504677167232f),
			float2(-0.0500874221246f, -0.0917990757772f),
			float2(-0.358644570242f, 0.906381100284f),
			float2(0.961200130218f, 0.219135111748f),
			float2(-0.896666615007f, -0.440304757692f)
		};

		float RandRotation = frac(sin(dot(InScreenUv, float2(12.9898f, 78.233f))) * (43758.5453f));
		float theta = RandRotation * 3.1415926f*2.0f;
		float2x2 randomRotationMatrix = float2x2(float2(cos(theta), -sin(theta)),
			float2(sin(theta), cos(theta)));

		for (int i = 0; i < 8; i++)
		{
			float2 RandPos = mul(poissonDisk[i], randomRotationMatrix)*InShadowSampleRadius;
			float3 SamplePos = LightPos + B*RandPos.x + T*RandPos.y;

			float3	L = (SamplePos - HitRes.Position);
			float dist = length(L);
			L = normalize(L);

			HitPayload CurHit;
			if (!sphere_list_hit(HitRes.Position, L, 0.05f, dist, CurHit))
			{
				shadowTerm += 1.0f;
			}
		}

		shadowTerm /= 8.0f;
	}

	if (shadowTerm < 0.001f)
	{
		return float3(0.0f, 0.0f, 0.0f);
	}

	float3 L = -LightDir;
	float3 Lighting = CalculatePBR(L, N, V, HitRes.MaterialParams.x, HitRes.MaterialParams.y, HitRes.Color.rgb, 25.0f);
	return float3(1.0f, 1.0f, 1.0f)*Lighting*shadowTerm;
}

RWStructuredBuffer<float4>	OutColorBuffer : register(u0);
RWTexture2D<float4>			OutDirectColor : register(u3);

[numthreads(16, 16, 1)]
void RayGenCS(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	uint Idx = GTid.x * 16 + GTid.y;
	if (Idx < OBJECT_NUM)
	{
		g_HitableList[Idx] = InHitableList[Idx];
	}

	uint PixelIdx = DTid.y*(uint)(ViewSizeAndInv.x) + DTid.x;
	OutRayHeaderBuffer[PixelIdx] = -1;

	GroupMemoryBarrierWithGroupSync();

	float2 uv = (DTid.xy + 0.5f) / ViewSizeAndInv.xy;

	float2 ScreenPos;
	ScreenPos.x = uv.x*2.0f - 1.0f;
	ScreenPos.y = 1.0f - uv.y*2.0f;

	uint rngState = wang_hash(((DTid.x + DTid.y * (uint)(ViewSizeAndInv.x)) + (uint)FrameNum.x*(uint)(ViewSizeAndInv.x*ViewSizeAndInv.y)));

	float3	TotalColor = float3(0.0f, 0.0f, 0.0f);
	for(int RayIdx = 0; RayIdx < RayNumPerPixel; RayIdx++)
	{
		float XOffset = (RandomFloat01(rngState)*2.0f - 1.0f)*ViewSizeAndInv.z;
		float YOffset = (RandomFloat01(rngState)*2.0f - 1.0f)*ViewSizeAndInv.w;

		float3 CameraRay = normalize(CameraXAxis.xyz*(ScreenPos.x + XOffset) + CameraYAxis.xyz*(ScreenPos.y + YOffset) + CameraZAxis.xyz);

		HitPayload	HitRes;
		if (sphere_list_hit(CameraOrigin.xyz, CameraRay, 0.05f, 1000000.0f, HitRes))
		{
			float3	atten = float3(1.0f, 1.0f, 1.0f);
			float3	scattered_dir = CameraRay;
			bool sample_diffuse = true;
			float diffuse_chance = 1.0f;
			if (scatter_hemisphere(CameraOrigin.xyz, CameraRay, HitRes, rngState, scattered_dir, sample_diffuse, diffuse_chance))
			{
				int NewRayAddress = OutRayBuffer.IncrementCounter();

				int PreRayAddress = 0;
				InterlockedExchange(OutRayHeaderBuffer[PixelIdx], NewRayAddress, PreRayAddress);

				Ray newRay;
				newRay.Origin = HitRes.Position;
				newRay.Direction = scattered_dir;
				newRay.DiffuseBRDF = HitRes.Color.rgb *(1.0f - HitRes.MaterialParams.y) / diffuse_chance;
				newRay.SampleDiffuse = (uint)sample_diffuse;
				newRay.rngState = rngState;
				newRay.Next = PreRayAddress;
				newRay.Depth = 1;

				float3 V = (-CameraRay);
				float3 N = HitRes.Normal;
				float3 L = scattered_dir;

				if (dot(N, L) > 0.0f && dot(N, V) > 0.0f)
				{
					float3 S = CalculateSpecular(L, N, V, HitRes.MaterialParams.x, HitRes.MaterialParams.y, HitRes.Color);
					newRay.SpecularBRDF = S / (1.0f - diffuse_chance);
				}
				else
				{
					newRay.SpecularBRDF = float3(0.0f, 0.0f, 0.0f);
				}

				OutRayBuffer[NewRayAddress] = newRay;
			}
			TotalColor += RayHitShading(-CameraRay, HitRes, 0, 5.0f, uv) / (float)(RayNumPerPixel);
		}
		else
		{
			TotalColor += RayMissShading(float3(1.0f, 1.0f, 1.0f))/ (float)(RayNumPerPixel);
		}
	}

	OutDirectColor[DTid.xy] = float4(TotalColor, 1.0f);
	OutColorBuffer[PixelIdx] = float4(0.0f, 0.0f, 0.0f, 0.0f);
}

[numthreads(16, 16, 1)]
void RayBounceCS(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	uint Idx = GTid.x * 16 + GTid.y;
	if (Idx < OBJECT_NUM)
	{
		g_HitableList[Idx] = InHitableList[Idx];
	}

	uint PixelIdx = DTid.y*(uint)(ViewSizeAndInv.x) + DTid.x;

	OutRayHeaderBuffer[PixelIdx] = -1;

	GroupMemoryBarrierWithGroupSync();

	float3	TotalColor = float3(0.0f, 0.0f, 0.0f);
	float2 uv = (DTid.xy + 0.5f) / ViewSizeAndInv.xy;

	int	RayAddress = InRayHeaderBuffer[PixelIdx];
	while (RayAddress != -1)
	{
		Ray CurRay = InRayBuffer[RayAddress];

		HitPayload	HitRes;
		if (sphere_list_hit(CurRay.Origin, CurRay.Direction, 0.05f, 1000000.0f, HitRes))
		{
			float3	atten = float3(1.0f, 1.0f, 1.0f);
			float3	scattered_dir = CurRay.Direction;
			uint	rngState = CurRay.rngState;
			bool sample_diffuse = true;
			float diffuse_chance = 1.0f;
			if (scatter_hemisphere(CurRay.Origin, CurRay.Direction, HitRes, rngState, scattered_dir, sample_diffuse, diffuse_chance))
			{
				int NewRayAddress = OutRayBuffer.IncrementCounter();

				int PreRayAddress = 0;
				InterlockedExchange(OutRayHeaderBuffer[PixelIdx], NewRayAddress, PreRayAddress);

				Ray newRay;
				newRay.Origin = HitRes.Position;
				newRay.Direction = scattered_dir;
				newRay.DiffuseBRDF = HitRes.Color *(1.0f - HitRes.MaterialParams.y) / diffuse_chance;
				newRay.SampleDiffuse = (uint)sample_diffuse;
				newRay.rngState = rngState;
				newRay.Next = PreRayAddress;
				newRay.Depth = CurRay.Depth + 1;

				float3 V = (-CurRay.Direction);
				float3 N = HitRes.Normal;
				float3 L = scattered_dir;

				if (dot(N, L) > 0.0f && dot(N, V) > 0.0f)
				{
					float3 S = CalculateSpecular(L, N, V, HitRes.MaterialParams.x, HitRes.MaterialParams.y, HitRes.Color);
					newRay.SpecularBRDF = S / (1.0f - diffuse_chance);
				}
				else
				{
					newRay.SpecularBRDF = float3(0.0f, 0.0f, 0.0f);
				}

				OutRayBuffer[NewRayAddress] = newRay;
			}

			float3	Lighting = RayHitShading(-CurRay.Direction, HitRes, CurRay.Depth, 0, uv);

			if (CurRay.SampleDiffuse > 0)
			{
				TotalColor += CurRay.DiffuseBRDF*Lighting / (float)(RayNumPerPixel);
			}
			else
			{
				TotalColor += CurRay.SpecularBRDF*Lighting/ (float)(RayNumPerPixel);
			}
		}
		else
		{
			TotalColor += RayMissShading(float3(1.0f, 1.0f, 1.0f)) / (float)(RayNumPerPixel);
		}

		RayAddress = CurRay.Next;
	}

	float4	PreColor = OutColorBuffer[PixelIdx];

	OutColorBuffer[PixelIdx] = PreColor + float4(TotalColor, 0.0f);
}

StructuredBuffer<float4>	InColorBuffer : register(t0);
Texture2D<float4>	InColorTexture : register(t5);
Texture2D<float4>			InDirectColor : register(t4);

[numthreads(16, 16, 1)]
void RayColorCombineCS(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
#if USE_SIMPLE_ACCUM
	float4 HistoryColor = HistoryTexture[DTid.xy];
	//float	HistoryW = 0.95f;
	float4 CurColor = float4( InColorBuffer[DTid.y*ViewSizeAndInv.x + DTid.x].rgb, 1.0f);
	float4 DirectColor = float4(InDirectColor[DTid.xy].rgb, 0.0f);
	OutputTexture[DTid.xy] = (CurColor + DirectColor + HistoryColor*(FrameNum.y - 1.0f)) / FrameNum.y;
#else
	float4 CurColor = InColorTexture[DTid.xy];
	float4 DirectColor = InDirectColor[DTid.xy];
	OutputTexture[DTid.xy] = float4(CurColor.rgb, 0.0f) +DirectColor;
#endif
	//OutputTexture[DTid.xy] = float4(CurColor.w, CurColor.w, CurColor.w, 1.0f);
}

RWTexture2D<float4>		OutSVGFNormalDepth : register(u0);

[numthreads(16, 16, 1)]
void SVGFSetupCS(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	uint Idx = GTid.x * 16 + GTid.y;
	if (Idx < OBJECT_NUM)
	{
		g_HitableList[Idx] = InHitableList[Idx];
	}

	uint PixelIdx = DTid.y*(uint)(ViewSizeAndInv.x) + DTid.x;

	GroupMemoryBarrierWithGroupSync();

	float2 uv = (DTid.xy + 0.5f) / ViewSizeAndInv.xy;

	float2 ScreenPos;
	ScreenPos.x = uv.x*2.0f - 1.0f;
	ScreenPos.y = 1.0f - uv.y*2.0f;

	float3 CameraRay = normalize(CameraXAxis.xyz*(ScreenPos.x) + CameraYAxis.xyz*(ScreenPos.y) + CameraZAxis.xyz);

	HitPayload	HitRes;
	if (sphere_list_hit(CameraOrigin.xyz, CameraRay, 0.05f, 1000000.0f, HitRes))
	{
		float4 WldPos = float4(CameraOrigin.xyz + CameraRay*HitRes.Dist, 1.0f);
		float4	ProjPos = mul(WldPos, mViewProjection);

		OutSVGFNormalDepth[DTid.xy] = float4(HitRes.Normal, ProjPos.w);
	}
	else
	{
		OutSVGFNormalDepth[DTid.xy] = float4(0.0f, 0.0f, 0.0f, 0.0f);
	}
}

