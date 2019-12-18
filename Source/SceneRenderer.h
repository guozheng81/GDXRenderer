#pragma once

#ifndef _SCENE_RENDERER_H_
#define _SCENE_RENDERER_H_

#define MAX_SHADOW_CASCADE_NUM 3

#include "SDKMesh.h"
#include "DXUTcamera.h"

#include "RenderPass.h"

struct CB_VIEW
{
	D3DXMATRIX m_ViewProj;
	D3DXMATRIX m_View;
	D3DXMATRIX m_Projection;
	D3DXMATRIX m_PreViewProj;
	D3DXVECTOR4 m_SceneBoundMin;
	D3DXVECTOR4 m_SceneBoundMax;
	D3DXVECTOR4 ViewSizeAndInv;

	D3DXVECTOR4 LightParams;		// xyz main dir light vector
	D3DXVECTOR4 LightingControl;	// x: Dir light radiance, y: Sky light radiance, w: 0 = disable tonemapping | 1 = manual | 2 = auto  

	D3DXVECTOR4 CameraOrigin;
	D3DXVECTOR4 CameraXAxis;
	D3DXVECTOR4 CameraYAxis;
	D3DXVECTOR4 CameraZAxis;

	D3DXVECTOR4 SkyCubeSize;

	D3DXMATRIX	ShadowViewProj[MAX_SHADOW_CASCADE_NUM];

	D3DXVECTOR4	CascadeShadowDistances;
	D3DXVECTOR4	DOFParams;
	D3DXVECTOR4	TemporalJitterParams;
	D3DXVECTOR4		FrameNum;

	D3DXMATRIX	m_RSMViewProj;
};

struct CB_MODEL
{
	D3DXMATRIX m_World;
	D3DXVECTOR4	MaterialParams;
};

class GSceneRenderer : public GRenderPass
{
protected:
	CDXUTSDKMesh                SceneMesh;
	CDXUTSDKMesh				SkyMesh;

	ID3D11InputLayout*          SceneVertexLayout = NULL;
	ID3D11VertexShader*         SceneVertexShader = NULL;
	ID3D11PixelShader*          ScenePixelShader = NULL;
	ID3D11SamplerState*         SamplerLinear = NULL;
	ID3D11SamplerState*			SamplerPoint = NULL;
	ID3D11SamplerState*         SamplerLinearWrap = NULL;
	ID3D11SamplerState*			SamplerPointWrap = NULL;

	ID3D11VertexShader*         SkyVertexShader = NULL;
	ID3D11PixelShader*          SkyPixelShader = NULL;

	ID3D11VertexShader*         ShadowVertexShader = NULL;
	ID3D11GeometryShader*		ShadowGeometryShader = NULL;

	ID3D11VertexShader*         RSMVertexShader = NULL;
	ID3D11PixelShader*         RSMPixelShader = NULL;

	ID3D11RasterizerState*		Backcull_RS = NULL;
	ID3D11RasterizerState*		Frontcull_RS = NULL;
	ID3D11RasterizerState*		Backcull_ZBias_RS = NULL;
	ID3D11RasterizerState*		NoneCull_RS = NULL;

	ID3D11DepthStencilState*	NoDepth_DSS = NULL;
	ID3D11DepthStencilState*	DisableWriteDepth_DSS = NULL;

	ID3D11Buffer*               ViewBuffer = NULL;
	ID3D11Buffer*				ModelBuffer = NULL;

	class CFirstPersonCamera*			Camera = NULL;

	UINT						ShadowTextureSize = 1024;
	D3DXVECTOR4					CascadeShadowDistances = D3DXVECTOR4(650.0f, 1500.0f, 3500.0f, 50.0f); //D3DXVECTOR4(150.0f, 500.0f, 1500.0f, 50.0f);	// w is the transition range

	D3DXMATRIX					GetCascadeShadowViewProj(UINT CascadeIdx, D3DXVECTOR3 InEyePos, D3DXVECTOR3 InEyeX, D3DXVECTOR3 InEyeY, D3DXVECTOR3 InEyeZ, D3DXVECTOR3 InLight, float FOV, float ratio);
	D3DXMATRIX					GetRSMViewProj(D3DXVECTOR3 InLight);

	bool						bIsFirstFrame = true;
	D3DXMATRIX					PreWorldViewProj;

	void SetModelParams(D3DXMATRIX InWldMtx, D3DXVECTOR4 InMaterialParam);
	void RenderScene();

public:
	GSceneRenderer();
	~GSceneRenderer();

	ID3D11ShaderResourceView*	SkyBox_SRV = NULL;
	UINT						SkyCubeSize;

	GTexture2D*					SceneColor = nullptr;

	GTexture2D*					SceneColorHDR = nullptr;

	GTexture2D*					FinalOutput = nullptr;

	GTexture2D*					GBufferA = nullptr;
	GTexture2D*					GBufferB = nullptr;

	GTexture2D*					DOF = nullptr;

	GTexture2D*					TemporalAA = nullptr;

	GTexture2D*					AO = nullptr;
	GTexture2D*					AOBlur0 = nullptr;
	GTexture2D*					AOBlur1 = nullptr;

	GTexture2D*					Velocity = nullptr;

	GTextureDepthStencil*		Depth = nullptr;
	GTextureDepthStencil*		Shadow = nullptr;

	D3D11_VIEWPORT				Main_VP;
	D3D11_VIEWPORT				Shadow_VP;

	D3DXVECTOR4					MaterialParams = D3DXVECTOR4(0.9f, 0.0f, 0.0f, 0.0f);
	D3DXVECTOR4					LightingControl = D3DXVECTOR4(50.0f, 0.7f, 1.0f, 2.0f);

	UINT						FrameNum = 0;
	UINT						AcumulatedFrameCount = 0;

	bool						bApplyTemporalJitter = true;

	virtual void Init(const GRenderContext& InContext) override;
	void SetMainCamera(CFirstPersonCamera* InCamera) {
		Camera = InCamera;
	}
	virtual void Render(UINT InSRVCount, ID3D11ShaderResourceView** InputSRV, ID3D11RenderTargetView* OutputRTV);
	void RenderSkybox(ID3D11RenderTargetView* OutputRTV);
	void RenderShadow();
	void RenderRSM(ID3D11RenderTargetView* OutputRTV, ID3D11UnorderedAccessView** OutputUAVs, ID3D11DepthStencilView* OutputDSV);
	void Voxelization();
	void SetSamplerStates();
	void UpdateViewBuffer(D3DXVECTOR3 vLightDir, float InExposure, D3DXVECTOR4 TemporalJitterSample);
	virtual void Shutdown() override;
};

#endif