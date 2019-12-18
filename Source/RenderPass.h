#pragma once

#ifndef _RENDER_PASS_H_
#define _RENDER_PASS_H_

#include <vector>

class GRenderContext
{
public:
	ID3D11Device* pd3dDevice = nullptr;
	ID3D11DeviceContext* pd3dDeviceContext = nullptr;

	UINT						ViewportWidth = 1280;
	UINT						ViewportHeight = 720;

	D3DXVECTOR4					SceneBoundMin;
	D3DXVECTOR4					SceneBoundMax;

	float						CameraFOV = 3.1415926f / 4.0f;

public:
	GRenderContext()
	{
	};
	~GRenderContext()
	{
	};
};

enum ERenderPassID
{
	None,
	Scene,
	Tonemapping,
	SkyLight,
	LightPass,
	SSAO,
	Velocity,
	MotionBlur,
	FilterMotionBlur,
	TiledDeferredLighting,
	VoxelConeTracer,
	AtmosphereScattering,
	Histogram,
	DOF,
	TemporalAA,
	AOBlur0,
	AOBlur1,
	UpSampling,
	RayTraceSphere,
	LPV,
	COUNT
};

class GTexture2D
{
public:
	ID3D11Texture2D*			Texture = nullptr;
	ID3D11ShaderResourceView*	SRV = nullptr;
	ID3D11RenderTargetView*		RTV = nullptr;
	ID3D11UnorderedAccessView*	UAV = nullptr;

	GTexture2D(ID3D11Device* pd3dDevice, UINT InW, UINT InH, DXGI_FORMAT InFormat, bool bNeedRTV, bool bNeedUAV);
	virtual ~GTexture2D();
};

class GTexture3D
{
public:
	ID3D11Texture3D*			Texture = nullptr;
	ID3D11ShaderResourceView*	SRV = nullptr;
	ID3D11UnorderedAccessView*	UAV = nullptr;

	GTexture3D(ID3D11Device* pd3dDevice, UINT InW, UINT InH, UINT InD, DXGI_FORMAT InFormat, bool bNeedUAV);
	virtual ~GTexture3D();
};

class GTextureDepthStencil
{
public:
	ID3D11Texture2D*			Texture = nullptr;
	ID3D11ShaderResourceView*	SRV = nullptr;
	ID3D11DepthStencilView*		DSV = nullptr;

	GTextureDepthStencil(ID3D11Device* pd3dDevice, UINT InW, UINT InH, UINT InArraySize = 1);
	virtual ~GTextureDepthStencil();
};

class GStructuredBuffer
{
public:
	ID3D11Buffer*				Buffer = nullptr;
	ID3D11UnorderedAccessView*	UAV = nullptr;
	ID3D11ShaderResourceView*	SRV = nullptr;

	GStructuredBuffer(ID3D11Device* pd3dDevice, UINT InByteStride, UINT InElementNum, DXGI_FORMAT InFormat = DXGI_FORMAT_UNKNOWN, bool bNeedUAV = true, bool bNeedUAVCounter = false);
	virtual ~GStructuredBuffer();
};

class GRenderPass : public GRenderContext
{
protected:
	ERenderPassID	ID = ERenderPassID::None;
	class GRenderPassManager*	OwnerMgr = nullptr;

public:
	GRenderPass();
	virtual	~GRenderPass();

	void	SetOwner(class GRenderPassManager* InOwner)
	{
		OwnerMgr = InOwner;
	}

	virtual void Init(const GRenderContext& InContext);
	virtual void Render(UINT InSRVCount, ID3D11ShaderResourceView** InputSRV, ID3D11RenderTargetView* OutputRTV);
	virtual void Shutdown();

};

class GRenderPassManager
{
protected:
	std::vector<GRenderPass*>	AllPasses;
	GRenderContext	CurrentContext;

	void	RenderLighting(class GSceneRenderer*	pSceneRenderer, class GVoxelConeTracer*	pVoxelConeTracer, class GSkyLight* pSkyLight);
	void	RenderPostProcess(class GSceneRenderer*	pSceneRenderer, ID3D11RenderTargetView* pRTV);

	void	RenderRayTraceSphere(class GSceneRenderer*	pSceneRenderer);

public:
	GRenderPassManager();
	~GRenderPassManager()
	{

	}

	bool bShowPointLights = false;
	bool bShowDOF = false;
	bool bShowAtmosphereScatterring = false;
	bool bShowMotionBlur = true;

	bool bShowRayTraceSphere = false;

	bool bEnableLPV = false;
	bool bEnableVoxleConeTrace = true;

	GRenderPass*	CreateRenderPass(ERenderPassID	inID);

	GRenderPass*	GetRenderPass(ERenderPassID	inID);
	class GSceneRenderer*	GetSceneRenderPass();

	void	Init(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, UINT InW, UINT InH, float InFOV);
	void	Render(ID3D11RenderTargetView* pRTV);
	void	Shutdown();

	void	ApplyGIControlParam(D3DXVECTOR4& LightControlParam)
	{
		if (bEnableVoxleConeTrace)
		{
			LightControlParam.z = 1.0f;
		}
		else if (bEnableLPV)
		{
			LightControlParam.z = 2.0f;
		}
		else
		{
			LightControlParam.z = 0.0f;
		}
	}
};

#endif
