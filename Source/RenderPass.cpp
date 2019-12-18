#include "DXUT.h"
#include "SDKmisc.h"

#include "RenderPass.h"

#include "SceneRenderer.h"
#include "PostProcess.h"
#include "SkyLight.h"
#include "MotionBlur.h"
#include "TiledDeferredLighting.h"
#include "VoxelConeTracer.h"
#include "AtmosphereScattering.h"
#include "Histogram.h"
#include "DOF.h"
#include "TemporalAA.h"
#include "RayTraceSphere.h"
#include "LPV.h"


GTexture2D::GTexture2D(ID3D11Device* pd3dDevice, UINT InW, UINT InH, DXGI_FORMAT InFormat, bool bNeedRTV, bool bNeedUAV)
{
	D3D11_TEXTURE2D_DESC	Texture_desc;
	ZeroMemory(&Texture_desc, sizeof(D3D11_TEXTURE2D_DESC));
	Texture_desc.ArraySize = 1;
	Texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	if (bNeedRTV)
	{
		Texture_desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
	}
	if (bNeedUAV)
	{
		Texture_desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
	}

	Texture_desc.Usage = D3D11_USAGE_DEFAULT;
	Texture_desc.Format = InFormat;
	Texture_desc.Width = InW;
	Texture_desc.Height = InH;
	Texture_desc.MipLevels = 1;
	Texture_desc.SampleDesc.Count = 1;
	pd3dDevice->CreateTexture2D(&Texture_desc, nullptr, &Texture);

	D3D11_SHADER_RESOURCE_VIEW_DESC SRV_desc;
	SRV_desc.Format = InFormat;
	SRV_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SRV_desc.Texture2D.MipLevels = 1;
	SRV_desc.Texture2D.MostDetailedMip = 0;
	pd3dDevice->CreateShaderResourceView(Texture, &SRV_desc, &SRV);

	if (bNeedRTV)
	{
		D3D11_RENDER_TARGET_VIEW_DESC RT_desc;
		RT_desc.Format = InFormat;
		RT_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		RT_desc.Texture2D.MipSlice = 0;
		pd3dDevice->CreateRenderTargetView(Texture, &RT_desc, &RTV);
	}

	if (bNeedUAV)
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC UAV_desc;
		ZeroMemory(&UAV_desc, sizeof(UAV_desc));
		UAV_desc.Format = InFormat;
		UAV_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		UAV_desc.Texture2D.MipSlice = 0;
		pd3dDevice->CreateUnorderedAccessView(Texture, &UAV_desc, &UAV);
	}
}

GTexture2D::~GTexture2D()
{
	SAFE_RELEASE(Texture);
	SAFE_RELEASE(SRV);

	SAFE_RELEASE(RTV);

	SAFE_RELEASE(UAV);
}

GTexture3D::GTexture3D(ID3D11Device* pd3dDevice, UINT InW, UINT InH, UINT InD, DXGI_FORMAT InFormat, bool bNeedUAV)
{
	D3D11_TEXTURE3D_DESC	texture_desc;

	ZeroMemory(&texture_desc, sizeof(D3D11_TEXTURE3D_DESC));
	texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	if (bNeedUAV)
	{
		texture_desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
	}
	texture_desc.Usage = D3D11_USAGE_DEFAULT;
	texture_desc.Width = InW;
	texture_desc.Height = InH;
	texture_desc.Depth = InD;
	texture_desc.MipLevels = 1;

	texture_desc.Format = InFormat;
	pd3dDevice->CreateTexture3D(&texture_desc, nullptr, &Texture);

	if (bNeedUAV)
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC tex_uav_desc;
		ZeroMemory(&tex_uav_desc, sizeof(tex_uav_desc));
		tex_uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
		tex_uav_desc.Texture3D.MipSlice = 0;
		tex_uav_desc.Texture3D.FirstWSlice = 0;
		tex_uav_desc.Texture3D.WSize = InD;
		tex_uav_desc.Format = InFormat;
		pd3dDevice->CreateUnorderedAccessView(Texture, &tex_uav_desc, &UAV);
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC		tex_srv_desc;
	ZeroMemory(&tex_srv_desc, sizeof(tex_srv_desc));
	tex_srv_desc.Format = InFormat;
	tex_srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
	tex_srv_desc.Texture3D.MipLevels = 1;
	tex_srv_desc.Texture3D.MostDetailedMip = 0;
	pd3dDevice->CreateShaderResourceView(Texture, &tex_srv_desc, &SRV);
}

GTexture3D::~GTexture3D()
{
	SAFE_RELEASE(Texture);
	SAFE_RELEASE(SRV);
	SAFE_RELEASE(UAV);
}

GTextureDepthStencil::GTextureDepthStencil(ID3D11Device* pd3dDevice, UINT InW, UINT InH, UINT InArraySize)
{
	D3D11_TEXTURE2D_DESC	Texture_desc;
	ZeroMemory(&Texture_desc, sizeof(D3D11_TEXTURE2D_DESC));

	Texture_desc.ArraySize = InArraySize;
	Texture_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	Texture_desc.Usage = D3D11_USAGE_DEFAULT;
	// support stencil later
	Texture_desc.Format = DXGI_FORMAT_R32_TYPELESS;
	Texture_desc.Width = InW;
	Texture_desc.Height = InH;
	Texture_desc.MipLevels = 1;
	Texture_desc.SampleDesc.Count = 1;
	pd3dDevice->CreateTexture2D(&Texture_desc, nullptr, &Texture);

	D3D11_DEPTH_STENCIL_VIEW_DESC DSV_desc = {
		DXGI_FORMAT_D32_FLOAT,
		InArraySize <= 1? D3D11_DSV_DIMENSION_TEXTURE2D : D3D11_DSV_DIMENSION_TEXTURE2DARRAY,
		0
	};

	D3D11_SHADER_RESOURCE_VIEW_DESC Depth_srv_desc = {
		DXGI_FORMAT_R32_FLOAT,
		InArraySize <= 1 ? D3D11_SRV_DIMENSION_TEXTURE2D : D3D11_SRV_DIMENSION_TEXTURE2DARRAY,
		0
	};

	if (InArraySize <= 1)
	{
		DSV_desc.Texture2D.MipSlice = 0;
		pd3dDevice->CreateDepthStencilView(Texture, &DSV_desc, &DSV);

		Depth_srv_desc.Texture2D.MipLevels = 1;
		Depth_srv_desc.Texture2D.MostDetailedMip = 0;
		pd3dDevice->CreateShaderResourceView(Texture, &Depth_srv_desc, &SRV);
	}
	else
	{
		DSV_desc.Texture2DArray.ArraySize = InArraySize;
		DSV_desc.Texture2DArray.FirstArraySlice = 0;
		DSV_desc.Texture2DArray.MipSlice = 0;
		pd3dDevice->CreateDepthStencilView(Texture, &DSV_desc, &DSV);

		Depth_srv_desc.Texture2DArray.ArraySize = InArraySize;
		Depth_srv_desc.Texture2DArray.FirstArraySlice = 0;
		Depth_srv_desc.Texture2DArray.MipLevels = 1;
		Depth_srv_desc.Texture2DArray.MostDetailedMip = 0;
		pd3dDevice->CreateShaderResourceView(Texture, &Depth_srv_desc, &SRV);
	}
}

GTextureDepthStencil::~GTextureDepthStencil()
{
	SAFE_RELEASE(Texture);
	SAFE_RELEASE(SRV);
	SAFE_RELEASE(DSV);
}

GStructuredBuffer::GStructuredBuffer(ID3D11Device* pd3dDevice, UINT InByteStride, UINT InElementNum, DXGI_FORMAT InFormat, bool bNeedUAV, bool bNeedUAVCounter)
{
	D3D11_BUFFER_DESC BufferDesc;
	ZeroMemory(&BufferDesc, sizeof(BufferDesc));
	BufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	if (bNeedUAV)
	{
		BufferDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
	}

	BufferDesc.MiscFlags = 0;
	if (InFormat == DXGI_FORMAT_UNKNOWN)
	{
		BufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	}
	BufferDesc.StructureByteStride = InByteStride;
	BufferDesc.ByteWidth = BufferDesc.StructureByteStride*InElementNum;

	pd3dDevice->CreateBuffer(&BufferDesc, NULL, &Buffer);

	if (bNeedUAV)
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC Uav_desc;
		ZeroMemory(&Uav_desc, sizeof(Uav_desc));
		Uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		Uav_desc.Buffer.FirstElement = 0;
		Uav_desc.Buffer.NumElements = InElementNum;
		Uav_desc.Buffer.Flags = 0;
		if (bNeedUAVCounter)
		{
			Uav_desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER;
		}
		Uav_desc.Format = InFormat;
		pd3dDevice->CreateUnorderedAccessView(Buffer, &Uav_desc, &UAV);
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC Srv_desc;
	ZeroMemory(&Srv_desc, sizeof(Srv_desc));
	Srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
	Srv_desc.BufferEx.FirstElement = 0;
	Srv_desc.BufferEx.NumElements = InElementNum;
	Srv_desc.Format = InFormat;
	pd3dDevice->CreateShaderResourceView(Buffer, &Srv_desc, &SRV);

}

GStructuredBuffer::~GStructuredBuffer()
{
	SAFE_RELEASE(SRV);
	SAFE_RELEASE(UAV);
	SAFE_RELEASE(Buffer);
}

GRenderPass::GRenderPass()
{

}

GRenderPass::~GRenderPass()
{
}

void GRenderPass::Init(const GRenderContext& InContext)
{
	pd3dDevice = InContext.pd3dDevice;
	pd3dDeviceContext = InContext.pd3dDeviceContext;

	ViewportWidth = InContext.ViewportWidth;
	ViewportHeight = InContext.ViewportHeight;

	SceneBoundMin = InContext.SceneBoundMin;
	SceneBoundMax = InContext.SceneBoundMax;

	CameraFOV = InContext.CameraFOV;
}

void GRenderPass::Render(UINT InSRVCount, ID3D11ShaderResourceView** InputSRV, ID3D11RenderTargetView* OutputRTV)
{

}

void GRenderPass::Shutdown()
{
	OwnerMgr = nullptr;
}

GRenderPassManager::GRenderPassManager()
{
	AllPasses.resize((int)ERenderPassID::COUNT - 1);
}

GRenderPass*	GRenderPassManager::CreateRenderPass(ERenderPassID	inID)
{
	GRenderPass* newPass = nullptr;

	switch (inID)
	{
	case None:
		break;
	case Scene:
	{
		newPass = new GSceneRenderer();
		static_cast<GSceneRenderer*>(newPass)->bApplyTemporalJitter = !bShowRayTraceSphere;
	}
		break;
	case Tonemapping:
	{
		GPostProcess*	pp = new GPostProcess();
		pp->SetPSShader(L"Shaders\\PostProcessing.hlsl", "TonemapPS");
		newPass = pp;
	}
		break;
	case SkyLight:
		newPass = new GSkyLight();
		break;
	case LightPass:
	{
		GPostProcess*	pp = new GPostProcess();
		pp->SetPSShader(L"Shaders\\Scene.hlsl", "PSDirAndSkyLight");
		newPass = pp;
	}
		break;
	case SSAO:
	{
		GPostProcess*	pp = new GPostProcess();
		pp->SetPSShader(L"Shaders\\SSAO.hlsl", "SSAO_PS");
		newPass = pp;
	}
	break;
	case Velocity:
	{
		GPostProcess*	pp = new GPostProcess();
		pp->SetPSShader(L"Shaders\\MotionBlur.hlsl", "Velocity_PS");
		newPass = pp;
	}
	break;
	case MotionBlur:
	{
		GPostProcess*	pp = new GPostProcess();
		pp->SetPSShader(L"Shaders\\MotionBlur.hlsl", "MotionBlur_PS");
		newPass = pp;
	}
	break;
	case FilterMotionBlur:
		newPass = new GMotionBlur();
		break;
	case TiledDeferredLighting:
		newPass = new GTiledDeferredLighting();
		break;
	case VoxelConeTracer:
		newPass = new GVoxelConeTracer();
		break;
	case AtmosphereScattering:
		newPass = new GAtmosphereScattering();
		break;
	case Histogram:
		newPass = new GHistogram();
		break;
	case DOF:
		newPass = new GDOF();
		break;
	case TemporalAA:
		newPass = new GTemporalAA();
		break;
	case UpSampling:
	{
		GPostProcess*	pp = new GPostProcess();
		pp->SetPSShader(L"Shaders\\PostProcessing.hlsl", "PSMain");
		newPass = pp;
	}
	break;
	case AOBlur0:
	{
		GPostProcess*	pp = new GPostProcess();
		pp->SetPSShader(L"Shaders\\PostProcessing.hlsl", "AOBlur0_PS");
		newPass = pp;
	}
	break;
	case AOBlur1:
	{
		GPostProcess*	pp = new GPostProcess();
		pp->SetPSShader(L"Shaders\\PostProcessing.hlsl", "AOBlur1_PS");
		newPass = pp;
	}
	break;
	case RayTraceSphere:
	{
		newPass = new GRayTraceSphere();
	}
	break;
	case LPV:
	{
		newPass = new GLPV();
	}
	break;
	case COUNT:
		break;
	default:
		break;
	}

	AllPasses[(int)inID - 1] = newPass;
	if (newPass)
	{
		newPass->SetOwner(this);
	}

	return newPass;
}

GRenderPass*	GRenderPassManager::GetRenderPass(ERenderPassID	inID)
{
	return AllPasses[(int)inID - 1];
}

class GSceneRenderer*	GRenderPassManager::GetSceneRenderPass()
{
	return dynamic_cast<GSceneRenderer*>(GetRenderPass(ERenderPassID::Scene));
}

void	GRenderPassManager::Init(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, UINT InW, UINT InH, float InFOV)
{
	for (int i = 1; i < (int)ERenderPassID::COUNT; ++i)
	{
		CreateRenderPass((ERenderPassID)i);
	}

	CurrentContext.pd3dDevice = pd3dDevice;
	CurrentContext.pd3dDeviceContext = pd3dImmediateContext;
	CurrentContext.ViewportWidth = InW;
	CurrentContext.ViewportHeight = InH;
	CurrentContext.CameraFOV = InFOV;

	for (int i = 0; i < AllPasses.size(); ++i)
	{
		GRenderPass*	CurPass = AllPasses[i];
		if (CurPass != nullptr)
		{
			CurPass->Init(CurrentContext);
		}

		if (i == (int)ERenderPassID::Scene - 1)
		{
			CurrentContext.SceneBoundMin = GetSceneRenderPass()->SceneBoundMin;
			CurrentContext.SceneBoundMax = GetSceneRenderPass()->SceneBoundMax;
		}
	}
}

void	GRenderPassManager::Render(ID3D11RenderTargetView* pRTV)
{
	GSceneRenderer*	pSceneRenderer = GetSceneRenderPass();

	if (bShowRayTraceSphere)
	{
		RenderRayTraceSphere(pSceneRenderer);
		return;
	}

	GVoxelConeTracer*	pVoxelConeTracer = dynamic_cast<GVoxelConeTracer*>(GetRenderPass(ERenderPassID::VoxelConeTracer));
	GSkyLight* pSkyLight = dynamic_cast<GSkyLight*>(GetRenderPass(ERenderPassID::SkyLight));

	pSkyLight->Render(1, &pSceneRenderer->SkyBox_SRV, nullptr);

	// shadow
	pSceneRenderer->RenderShadow();

	if (bEnableLPV)
	{
		GLPV*	pLPV = static_cast<GLPV*>(GetRenderPass(ERenderPassID::LPV));
		pLPV->Render(0, nullptr, pSceneRenderer->FinalOutput->RTV);
	}

	if (bEnableVoxleConeTrace)
	{
		// voxelization
		pVoxelConeTracer->Voxelization(pSceneRenderer);

		// voxel scene visualization
		//	pVoxelConeTracer->Render(0, nullptr, pSceneRenderer->pRTV);
	}

	// base pass 
	pSceneRenderer->Render(0, nullptr, nullptr);

	// lighting
	RenderLighting(pSceneRenderer,	pVoxelConeTracer, pSkyLight);

	// combine skybox
	pSceneRenderer->RenderSkybox(pSceneRenderer->SceneColorHDR->RTV);

	// post process
	RenderPostProcess(pSceneRenderer, pRTV);

}

void	GRenderPassManager::RenderLighting(GSceneRenderer*	pSceneRenderer, GVoxelConeTracer*	pVoxelConeTracer, GSkyLight* pSkyLight)
{
	ID3D11ShaderResourceView*	SSAOInput[3] = { pSceneRenderer->GBufferB->SRV, pSceneRenderer->Depth->SRV, pSceneRenderer->Shadow->SRV };
	GetRenderPass(ERenderPassID::SSAO)->Render(3, SSAOInput, pSceneRenderer->AO->RTV);

	ID3D11ShaderResourceView*	AOBlurInput[2] = { pSceneRenderer->AO->SRV, pSceneRenderer->Depth->SRV };
	GetRenderPass(ERenderPassID::AOBlur0)->Render(2, AOBlurInput, pSceneRenderer->AOBlur0->RTV);

	AOBlurInput[0] = pSceneRenderer->AOBlur0->SRV;
	GetRenderPass(ERenderPassID::AOBlur1)->Render(2, AOBlurInput, pSceneRenderer->AOBlur1->RTV);

	GLPV*	pLPV = static_cast<GLPV*>(GetRenderPass(ERenderPassID::LPV));

	ID3D11ShaderResourceView*	LightPassInput[15] = { pSceneRenderer->GBufferA->SRV, pSceneRenderer->GBufferB->SRV, pSceneRenderer->Depth->SRV, pSkyLight->Diffuse->SRV, pSkyLight->Specular_SRV, pSkyLight->SpecularIntegration->SRV, pSceneRenderer->Shadow->SRV, pSceneRenderer->AOBlur1->SRV,
		pVoxelConeTracer->VCTVolumes[1]->SRV, pVoxelConeTracer->VCTVolumes[2]->SRV, pVoxelConeTracer->VCTVolumes[3]->SRV, pVoxelConeTracer->VCTVolumes[4]->SRV,
		pLPV->Resolved_Vpl_R->SRV, pLPV->Resolved_Vpl_G->SRV, pLPV->Resolved_Vpl_B->SRV };
	GetRenderPass(ERenderPassID::LightPass)->Render(15, LightPassInput, pSceneRenderer->SceneColorHDR->RTV);

	if (bShowPointLights)
	{
		ID3D11ShaderResourceView*	TiledDeferredLightPassInput[3] = { pSceneRenderer->GBufferA->SRV, pSceneRenderer->GBufferB->SRV, pSceneRenderer->Depth->SRV };
		GetRenderPass(ERenderPassID::TiledDeferredLighting)->Render(3, TiledDeferredLightPassInput, pSceneRenderer->SceneColorHDR->RTV);
	}

}

void	GRenderPassManager::RenderPostProcess(GSceneRenderer*	pSceneRenderer, ID3D11RenderTargetView* pRTV)
{
	if (bShowAtmosphereScatterring)
	{
		ID3D11ShaderResourceView*	AtmosphereScattering[2] = { pSceneRenderer->Shadow->SRV, pSceneRenderer->Depth->SRV };
		GetRenderPass(ERenderPassID::AtmosphereScattering)->Render(2, AtmosphereScattering, pSceneRenderer->SceneColorHDR->RTV);
	}

	GHistogram*		pHistogram = dynamic_cast<GHistogram*>(GetRenderPass(ERenderPassID::Histogram));
	pHistogram->Render(1, &(pSceneRenderer->SceneColorHDR->SRV), nullptr);

	ID3D11ShaderResourceView*	TonemappingInput[2] = { pSceneRenderer->SceneColorHDR->SRV, pHistogram->AverageLuminance->SRV };

	if (bShowDOF)
	{
		ID3D11ShaderResourceView*	DOFInput[2] = { pSceneRenderer->SceneColorHDR->SRV, pSceneRenderer->Depth->SRV };
		GetRenderPass(ERenderPassID::DOF)->Render(2, DOFInput, pSceneRenderer->DOF->RTV);
		TonemappingInput[0] = pSceneRenderer->DOF->SRV;
	}

	GetRenderPass(ERenderPassID::Tonemapping)->Render(2, TonemappingInput, pSceneRenderer->SceneColor->RTV);

	ID3D11ShaderResourceView*	VelocityInput[1] = { pSceneRenderer->Depth->SRV };
	GetRenderPass(ERenderPassID::Velocity)->Render(1, VelocityInput, pSceneRenderer->Velocity->RTV);

	ID3D11ShaderResourceView*	AAInput[2] = { pSceneRenderer->SceneColor->SRV, pSceneRenderer->Velocity->SRV };
	GetRenderPass(ERenderPassID::TemporalAA)->Render(2, AAInput, bShowMotionBlur? pSceneRenderer->TemporalAA->RTV : pSceneRenderer->FinalOutput->RTV);

	// simple motion blur
	//	ID3D11ShaderResourceView*	MotionBlurInput[2] = { pSceneRenderer->SceneColor->SRV, pSceneRenderer->Velocity->SRV };
	//	GetRenderPass(ERenderPassID::MotionBlur)->Render(2, MotionBlurInput, pRTV);

	if (bShowMotionBlur)
	{
		ID3D11ShaderResourceView*	MotionBlurInput[3] = { pSceneRenderer->TemporalAA->SRV, pSceneRenderer->Velocity->SRV, pSceneRenderer->Depth->SRV };
		GetRenderPass(ERenderPassID::FilterMotionBlur)->Render(3, MotionBlurInput, pSceneRenderer->FinalOutput->RTV);
	}

}

void	GRenderPassManager::RenderRayTraceSphere(class GSceneRenderer*	pSceneRenderer)
{
	GRayTraceSphere*	TracePass = static_cast<GRayTraceSphere*>(GetRenderPass(ERenderPassID::RayTraceSphere));
	TracePass->Render(0, nullptr, nullptr);

	GHistogram*		pHistogram = dynamic_cast<GHistogram*>(GetRenderPass(ERenderPassID::Histogram));
	ID3D11ShaderResourceView*	CurSRV = TracePass->GetCurrentSRV();
	pHistogram->Render(1, &CurSRV, nullptr);

	ID3D11ShaderResourceView*	TonemappingInput[2] = { CurSRV, pHistogram->AverageLuminance->SRV };

	GetRenderPass(ERenderPassID::Tonemapping)->Render(2, TonemappingInput, pSceneRenderer->FinalOutput->RTV);
}

void	GRenderPassManager::Shutdown()
{
	for (int i = 0; i < AllPasses.size(); ++i)
	{
		GRenderPass*	CurPass = AllPasses[i];
		AllPasses[i] = nullptr;
		if (CurPass)
		{
			CurPass->Shutdown();
		}
		delete	CurPass;
	}
	AllPasses.clear();
}


