//--------------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------------
cbuffer cbView : register(b0)
{
	matrix		mViewProjection;
	matrix		mView;
	matrix		mProjection;
	matrix		mPreViewProjection;
	float4		SceneBoundMin;
	float4		SceneBoundMax;
	float4		ViewSizeAndInv;

	float4		LightParams;
	float4		LightingControl;

	float4		CameraOrigin;
	float4		CameraXAxis;
	float4		CameraYAxis;
	float4		CameraZAxis;
	float4		SkyCubeSize;

	matrix		ShadowViewProj[3];
	float4		CascadeShadowDistances;
	float4		DOFParams;			// x: near plane, y: focus plane, z : far plane
	float4		TemporalJitterParams;
	float4		FrameNum;

	matrix		mRSMViewProjection;
};

cbuffer cbModel : register(b1)
{
	matrix		mWorld;
	float4		MaterialParams;		// x: roughness, y: metal
};

struct QuadVS_Input
{
	float4 Pos : POSITION;
	float2 Uv : TEXCOORD0;
};

struct QuadVS_Output
{
	float4 Pos : SV_POSITION;
	float2 Uv : TEXCOORD0;
};

