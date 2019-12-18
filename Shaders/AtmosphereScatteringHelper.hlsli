#define	SCATTERING_Z_RANGE 10000.0f
#define Z_SLICE_POW 2.0f

// the value needs to match C++ code
static const float3 FrustumSize = float3(320.0f, 180.0f, 256.0f);
static const float3 FrustumTexScale = (FrustumSize - float3(1.0f, 1.0f, 1.0f)) / FrustumSize;
static const float3 FrustumTexBias = float3(0.5f, 0.5f, 0.5f) / FrustumSize;

float PhaseFunction(float g, float CosTheta)
{
	return 3.0f * (1.0f + CosTheta * CosTheta) / (16.0f * 3.1415926f);
}

float HenyeyGreensteinPhase(float g, float CosTheta)
{
	g = -g;
	return (1 - g * g) / (4 * 3.1415926f * pow(1 + g * g - 2 * g * CosTheta, 1.5f));
}

float TwoLobePhaseFunction(float g0, float g1, float CosTheta)
{
	return lerp(HenyeyGreensteinPhase(g0, CosTheta), HenyeyGreensteinPhase(g1, CosTheta), 0.5f);
}

float4 GetWorldPosByFrustumPos(uint3 InFrustumPos, float zJitter)
{
	float3	NormalizedPos;
	NormalizedPos.xy = saturate(float2(InFrustumPos.x / FrustumSize.x, InFrustumPos.y / FrustumSize.y));
	NormalizedPos.z = saturate(((float)InFrustumPos.z + zJitter) / FrustumSize.z);
	NormalizedPos *= FrustumTexScale;
	NormalizedPos += FrustumTexBias;

	float z = pow(NormalizedPos.z, Z_SLICE_POW)*SCATTERING_Z_RANGE;
	//float z = (exp2((1.0f-NormalizedPos.z)*( -10.0f)) - exp2(-10.0f))/(1.0f - exp2(-10.0f))*SCATTERING_Z_RANGE;

	float2 ScreenPos;
	ScreenPos.x = NormalizedPos.x*2.0f - 1.0f;
	ScreenPos.y = 1.0f - NormalizedPos.y*2.0f;

	float3 CameraRay = (CameraXAxis.xyz*ScreenPos.x + CameraYAxis.xyz*ScreenPos.y + CameraZAxis.xyz);

	float4 WldPos;
	WldPos.xyz = CameraOrigin.xyz + CameraRay*z;
	WldPos.w = 1.0f;

	return WldPos;
}