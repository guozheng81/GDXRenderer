// http://mrl.nyu.edu/~perlin/noise/
// https://github.com/bartwronski/CSharpRenderer

Texture2D<float>  permSamplerTex             : register(t3);
Texture2D<float4> permSampler2dTex           : register(t4);
Texture2D<float4> gradSamplerTex             : register(t5);
Texture2D<float4> permGradSamplerTex         : register(t6);

SamplerState LinearSamplerWrap : register(s2);
SamplerState pointWrapSampler :  register(s3);

float3 fade(float3 t)
{
	return t * t * t * (t * (t * 6 - 15) + 10); // new curve
												//	return t * t * (3 - 2 * t); // old curve
}

float4 fade(float4 t)
{
	return t * t * t * (t * (t * 6 - 15) + 10); // new curve
												//	return t * t * (3 - 2 * t); // old curve
}

float perm(float x)
{
	return permSamplerTex.SampleLevel(pointWrapSampler, float2(x, 0.5f), 0);
}

float4 perm2d(float2 p)
{
	return permSampler2dTex.SampleLevel(pointWrapSampler, p, 0);
}

float grad(float x, float3 p)
{
	return dot(gradSamplerTex.SampleLevel(pointWrapSampler, float2(x * 16, 0.5f), 0).xyz, p);
}

float gradperm(float x, float3 p)
{
	return dot(permGradSamplerTex.SampleLevel(pointWrapSampler, float2(x, 0.5f), 0).xyz, p);
}

// optimized version
float inoise(float3 p)
{
	float3 P = fmod(floor(p), 256.0);	// FIND UNIT CUBE THAT CONTAINS POINT
	p -= floor(p);                      // FIND RELATIVE X,Y,Z OF POINT IN CUBE.
	float3 f = fade(p);                 // COMPUTE FADE CURVES FOR EACH OF X,Y,Z.

	P = P / 256.0;
	const float one = 1.0 / 256.0;

	// HASH COORDINATES OF THE 8 CUBE CORNERS
	float4 AA = perm2d(P.xy) + P.z;

	// AND ADD BLENDED RESULTS FROM 8 CORNERS OF CUBE
	return lerp(lerp(lerp(gradperm(AA.x, p),
		gradperm(AA.z, p + float3(-1, 0, 0)), f.x),
		lerp(gradperm(AA.y, p + float3(0, -1, 0)),
			gradperm(AA.w, p + float3(-1, -1, 0)), f.x), f.y),

		lerp(lerp(gradperm(AA.x + one, p + float3(0, 0, -1)),
			gradperm(AA.z + one, p + float3(-1, 0, -1)), f.x),
			lerp(gradperm(AA.y + one, p + float3(0, -1, -1)),
				gradperm(AA.w + one, p + float3(-1, -1, -1)), f.x), f.y), f.z);
}


// fractal sum
float fBm(float3 p, int octaves, float lacunarity = 2.0, float gain = 0.5)
{
	float freq = 1.0, amp = 0.5;
	float sum = 0;
	for (int i = 0; i<octaves; i++) {
		sum += inoise(p*freq)*amp;
		freq *= lacunarity;
		amp *= gain;
	}
	return sum;
}

Texture3D<float4>  CloudLowFreqNoise       : register(t7);
Texture3D<float4> CloudHighFreqNoise       : register(t8);
Texture2D<float4> CloudWeather             : register(t9);
Texture2D<float4> CloudCurlNoise          : register(t10);

//https://www.guerrilla-games.com/read/the-real-time-volumetric-cloudscapes-of-horizon-zero-dawn

float remap(float value, float oldMin, float oldMax, float newMin, float newMax)
{
	return newMin + (value - oldMin) / (oldMax - oldMin) * (newMax - newMin);
}

float densityHeightGradient(
	float heightFrac,
	float cloudType)
{
	const float4 STRATUS_GRADIENT = float4(0.02f, 0.15f, 0.3f, 0.6f);
	const float4 STRATOCUMULUS_GRADIENT = float4(0.02f, 0.3f, 0.48f, 0.7f);
	const float4 CUMULUS_GRADIENT = float4(0.01f, 0.25f, 0.6f, 1.0f);

	float stratus_weight = 1.0f - saturate(cloudType * 2.0f);
	float stratocumulus_weight = 1.0f - abs(cloudType - 0.5f) * 2.0f;
	float cumulus_weight = saturate(cloudType - 0.5f) * 2.0f;

	float stratus = smoothstep(STRATUS_GRADIENT.x, STRATUS_GRADIENT.y, heightFrac) - smoothstep(STRATUS_GRADIENT.z, STRATUS_GRADIENT.w, heightFrac);
	float stratocumulus = smoothstep(STRATOCUMULUS_GRADIENT.x, STRATOCUMULUS_GRADIENT.y, heightFrac) - smoothstep(STRATOCUMULUS_GRADIENT.z, STRATOCUMULUS_GRADIENT.w, heightFrac);
	float cumulus = smoothstep(CUMULUS_GRADIENT.x, CUMULUS_GRADIENT.y, heightFrac) - smoothstep(CUMULUS_GRADIENT.z, CUMULUS_GRADIENT.w, heightFrac);

	//return cumulus;
	return stratus*stratus_weight + stratocumulus*stratocumulus_weight + cumulus*cumulus_weight;
}

float GetCloudDensity_HZD(float3 Pos, float2 heightMinMax)
{	
	float3 weatherData = CloudWeather.SampleLevel(LinearSamplerWrap, Pos.xz* 0.0001f, 0.0f).rgb;
	float height_frac = saturate((Pos.y - heightMinMax.x) / (heightMinMax.y - heightMinMax.x));

	Pos = Pos*0.00035f;
	float4 low_freq_noise = CloudLowFreqNoise.SampleLevel(LinearSamplerWrap, Pos, 0);
	float low_freq_fbm = (low_freq_noise.g * 0.625f) + (low_freq_noise.b * 0.25f) + (low_freq_noise.a * 0.125f);

	float base_cloud = remap(low_freq_noise.r, -(1.0f - low_freq_fbm), 1.0f, 0.0f, 1.0f);

	float densityGradient = densityHeightGradient(height_frac, weatherData.b);
	base_cloud *= densityGradient;

	float cloud_coverage = weatherData.r;
	float baseCloudWithCoverage = remap(base_cloud, cloud_coverage, 1.0f, 0.0f, 1.0f);
	baseCloudWithCoverage *= cloud_coverage;

	//// pos += curlNoise.xy * (1.0f - height_frac);

	float3 high_freq_noise = CloudHighFreqNoise.SampleLevel(LinearSamplerWrap, Pos, 0.0f).rgb;
	float high_freq_fbm = (high_freq_noise.r * 0.625f) + (high_freq_noise.g * 0.25f) + (high_freq_noise.b * 0.125f);

	float high_freq_noise_modifier = lerp(high_freq_fbm, 1.0f - high_freq_fbm, saturate(height_frac * 10.0f));

	baseCloudWithCoverage = remap(
		baseCloudWithCoverage,
		high_freq_noise_modifier * 0.2f, 1.0f,
		0.0f, 1.0f);

	return saturate(baseCloudWithCoverage);
}