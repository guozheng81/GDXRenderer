float4 SHBasisFunc(float3 Dir)
{
	float4 Res;
	Res.x = 0.282095f;
	Res.y = -0.488603f * Dir.y;
	Res.z = 0.488603f * Dir.z;
	Res.w = -0.488603f * Dir.x;
	return Res;
}

float4 SHCosTransfer(float3 Dir)
{
	float4 Res = SHBasisFunc(Dir);
	Res.x *= 3.1415926f;
	Res.yzw *= (3.1415926f*2.0f / 3.0f);
	return Res;
}

struct SH_RGB
{
	float4 R;
	float4 G;
	float4 B;
};

//http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
float radicalInverse_VdC(uint bits) {
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

float2 hammersley2d(uint i, uint N) {
	return float2(float(i) / float(N), radicalInverse_VdC(i));
}

float3 hemisphereSample_cos(float u, float v) {
	float phi = v * 2.0f * 3.1415926f;
	float cosTheta = sqrt(1.0f - u);
	float sinTheta = sqrt(1.0f - cosTheta * cosTheta);
	return float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

float3	hemisphereSample_GGX(float2 Xi, float roughness, out float pdf)
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

	float d = (cosTheta * a*a - cosTheta) * cosTheta + 1;
	float D = a*a / (3.1415926f*d*d);
	pdf = D*cosTheta;

	return H;
}


uint RNG(inout uint state)
{
	uint x = state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 15;
	state = x;
	return x;
}

uint initRand(uint val0, uint val1, uint backoff = 16)
{
	uint v0 = val0, v1 = val1, s0 = 0;

	[unroll]
	for (uint n = 0; n < backoff; n++)
	{
		s0 += 0x9e3779b9;
		v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
		v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
	}
	return v0;
}

// Takes our seed, updates it, and returns a pseudorandom float in [0..1]
float nextRand(inout uint s)
{
	s = (1664525u * s + 1013904223u);
	return float(s & 0x00FFFFFF) / float(0x01000000);
}

float RandomFloat01(inout uint state)
{
	return (RNG(state) & 0xFFFFFF) / 16777216.0f;
	//return RNG(state) / 4294967296.0f;
	//state = (1664525u * state + 1013904223u);
	//return float(state & 0x00FFFFFF) / float(0x01000000);
}

float3 RandInSphere(inout uint state)
{
	float Random = RandomFloat01(state);
	float Random1 = RandomFloat01(state);
	float Random2 = RandomFloat01(state);

	float a = Random * 2.0f * 3.1415926f;
	float z = Random1*2.0f - 1.0f;
	float c = sqrt(1.0f - z*z);
	float r = pow(Random2, 1.0f / 3.0f);

	return float3(c*cos(a), c*sin(a), z)*r;
}

//http://www.reedbeta.com/blog/quick-and-easy-gpu-random-numbers-in-d3d11/
uint wang_hash(uint seed)
{
	seed = (seed ^ 61) ^ (seed >> 16);
	seed *= 9;
	seed = seed ^ (seed >> 4);
	seed *= 0x27d4eb2d;
	seed = seed ^ (seed >> 15);
	return seed;
}
