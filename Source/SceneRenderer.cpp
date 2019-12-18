#include "DXUT.h"
#include "SDKMesh.h"
#include "SDKmisc.h"
#include "DXUTcamera.h"

#include "SceneRenderer.h"
#include "RenderUtils.h"

GSceneRenderer::GSceneRenderer()
{
}

GSceneRenderer::~GSceneRenderer()
{

}

void GSceneRenderer::Init(const GRenderContext& InContext)
{
	GRenderPass::Init(InContext);

	// Compile the shaders using the lowest possible profile for broadest feature level support
	ID3DBlob* pVertexShaderBuffer = NULL;
	CompileShaderFromFile(L"Shaders\\Scene.hlsl", NULL, "VSMain", "vs_5_0", &pVertexShaderBuffer);

	ID3DBlob* pPixelShaderBuffer = NULL;
	CompileShaderFromFile(L"Shaders\\Scene.hlsl", NULL, "PSMain", "ps_5_0", &pPixelShaderBuffer);

	// Create the shaders
	pd3dDevice->CreateVertexShader(pVertexShaderBuffer->GetBufferPointer(),
		pVertexShaderBuffer->GetBufferSize(), NULL, &SceneVertexShader);
	DXUT_SetDebugName(SceneVertexShader, "VSMain");
	pd3dDevice->CreatePixelShader(pPixelShaderBuffer->GetBufferPointer(),
		pPixelShaderBuffer->GetBufferSize(), NULL, &ScenePixelShader);
	DXUT_SetDebugName(ScenePixelShader, "PSMain");

	// Create our vertex input layout
	const D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT,   0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	pd3dDevice->CreateInputLayout(layout, ARRAYSIZE(layout), pVertexShaderBuffer->GetBufferPointer(),
		pVertexShaderBuffer->GetBufferSize(), &SceneVertexLayout);
	DXUT_SetDebugName(SceneVertexLayout, "Primary");

	SAFE_RELEASE(pVertexShaderBuffer);
	SAFE_RELEASE(pPixelShaderBuffer);

	D3D_SHADER_MACRO defines[2] = {
		"SKY_BOX", "1",
		NULL, NULL
	};

	CompileShaderFromFile(L"Shaders\\Scene.hlsl", defines, "VSMain", "vs_5_0", &pVertexShaderBuffer);
	CompileShaderFromFile(L"Shaders\\Scene.hlsl", NULL, "PSSky", "ps_5_0", &pPixelShaderBuffer);

	// Create the shaders
	pd3dDevice->CreateVertexShader(pVertexShaderBuffer->GetBufferPointer(),
		pVertexShaderBuffer->GetBufferSize(), NULL, &SkyVertexShader);
	pd3dDevice->CreatePixelShader(pPixelShaderBuffer->GetBufferPointer(),
		pPixelShaderBuffer->GetBufferSize(), NULL, &SkyPixelShader);


	SAFE_RELEASE(pVertexShaderBuffer);
	SAFE_RELEASE(pPixelShaderBuffer);

	D3D_SHADER_MACRO shadow_defines[2] = {
		"SHADOW", "1",
		NULL, NULL
	};

	CompileShaderFromFile(L"Shaders\\Scene.hlsl", shadow_defines, "VSMain", "vs_5_0", &pVertexShaderBuffer);
	pd3dDevice->CreateVertexShader(pVertexShaderBuffer->GetBufferPointer(),
		pVertexShaderBuffer->GetBufferSize(), NULL, &ShadowVertexShader);

	SAFE_RELEASE(pVertexShaderBuffer);

	CompileShaderFromFile(L"Shaders\\Scene.hlsl", NULL, "ShadowGS", "gs_5_0", &pVertexShaderBuffer);
	pd3dDevice->CreateGeometryShader(pVertexShaderBuffer->GetBufferPointer(),
		pVertexShaderBuffer->GetBufferSize(), NULL, &ShadowGeometryShader);

	SAFE_RELEASE(pVertexShaderBuffer);

	CompileShaderFromFile(L"Shaders\\Scene.hlsl", NULL, "RSMVSMain", "vs_5_0", &pVertexShaderBuffer);
	pd3dDevice->CreateVertexShader(pVertexShaderBuffer->GetBufferPointer(),
		pVertexShaderBuffer->GetBufferSize(), NULL, &RSMVertexShader);
	SAFE_RELEASE(pVertexShaderBuffer);

	CompileShaderFromFile(L"Shaders\\LPV.hlsl", NULL, "RSMPs", "ps_5_0", &pVertexShaderBuffer);
	pd3dDevice->CreatePixelShader(pVertexShaderBuffer->GetBufferPointer(),
		pVertexShaderBuffer->GetBufferSize(), NULL, &RSMPixelShader);
	SAFE_RELEASE(pVertexShaderBuffer);

	// Load the mesh
	SceneMesh.Create(pd3dDevice, L"sponza\\Sponza.sdkmesh", true);

	SkyMesh.Create(pd3dDevice, L"misc\\cube.sdkmesh");

	// find the file
	WCHAR skybox_path[1000];
	DXUTFindDXSDKMediaFileCch(skybox_path, MAX_PATH, L"Light Probes\\rnl_cross.dds");

	DXUTGetGlobalResourceCache().CreateTextureFromFile(pd3dDevice, pd3dDeviceContext, skybox_path, &SkyBox_SRV);

	ID3D11Resource*	SkyBox_Res;
	SkyBox_SRV->GetResource(&SkyBox_Res);
	ID3D11Texture2D* SkyBox_2D_Res = static_cast<ID3D11Texture2D*>(SkyBox_Res);
	D3D11_TEXTURE2D_DESC skyCube_desc;
	SkyBox_2D_Res->GetDesc(&skyCube_desc);
	SAFE_RELEASE(SkyBox_Res);

	SkyCubeSize = skyCube_desc.Width;

	for (UINT i = 0; i < SceneMesh.GetNumMeshes(); ++i)
	{
		SDKMESH_MESH* msh = SceneMesh.GetMesh(i);
		D3DXVECTOR4 MinV = D3DXVECTOR4(msh->BoundingBoxCenter.x - msh->BoundingBoxExtents.x,
			msh->BoundingBoxCenter.y - msh->BoundingBoxExtents.y,
			msh->BoundingBoxCenter.z - msh->BoundingBoxExtents.z,
			1.0f);

		D3DXVECTOR4 MaxV = D3DXVECTOR4(msh->BoundingBoxCenter.x + msh->BoundingBoxExtents.x,
			msh->BoundingBoxCenter.y + msh->BoundingBoxExtents.y,
			msh->BoundingBoxCenter.z + msh->BoundingBoxExtents.z,
			1.0f);

		if (i == 0)
		{
			SceneBoundMin = MinV;
			SceneBoundMax = MaxV;
		}
		else
		{
			if (MinV.x < SceneBoundMin.x)	SceneBoundMin.x = MinV.x;
			if (MinV.y < SceneBoundMin.y)	SceneBoundMin.y = MinV.y;
			if (MinV.z < SceneBoundMin.z)	SceneBoundMin.z = MinV.z;

			if (MaxV.x > SceneBoundMax.x)	SceneBoundMax.x = MaxV.x;
			if (MaxV.y > SceneBoundMax.y)	SceneBoundMax.y = MaxV.y;
			if (MaxV.z > SceneBoundMax.z)	SceneBoundMax.z = MaxV.z;
		}
	}

	// Create a sampler state
	D3D11_SAMPLER_DESC SamDesc;
	SamDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	SamDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	SamDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	SamDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	SamDesc.MipLODBias = 0.0f;
	SamDesc.MaxAnisotropy = 1;
	SamDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	SamDesc.BorderColor[0] = SamDesc.BorderColor[1] = SamDesc.BorderColor[2] = SamDesc.BorderColor[3] = 0;
	SamDesc.MinLOD = 0;
	SamDesc.MaxLOD = D3D11_FLOAT32_MAX;
	pd3dDevice->CreateSamplerState(&SamDesc, &SamplerLinear);

	SamDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	pd3dDevice->CreateSamplerState(&SamDesc, &SamplerPoint);

	SamDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	SamDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	SamDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	pd3dDevice->CreateSamplerState(&SamDesc, &SamplerPointWrap);

	SamDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	pd3dDevice->CreateSamplerState(&SamDesc, &SamplerLinearWrap);

	// Setup constant buffers
	D3D11_BUFFER_DESC Desc;
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	Desc.MiscFlags = 0;

	Desc.ByteWidth = sizeof(CB_VIEW);
	pd3dDevice->CreateBuffer(&Desc, NULL, &ViewBuffer);
	DXUT_SetDebugName(ViewBuffer, "CB_VIEW");

	Desc.ByteWidth = sizeof(CB_MODEL);
	pd3dDevice->CreateBuffer(&Desc, NULL, &ModelBuffer);

	// Scene color

	SceneColor = new GTexture2D(pd3dDevice, ViewportWidth, ViewportHeight, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, true, false);
	GBufferA = new GTexture2D(pd3dDevice, ViewportWidth, ViewportHeight, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, true, false);
	GBufferB = new GTexture2D(pd3dDevice, ViewportWidth, ViewportHeight, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, true, false);
	AO = new GTexture2D(pd3dDevice, ViewportWidth, ViewportHeight, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, true, false);
	AOBlur0 = new GTexture2D(pd3dDevice, ViewportWidth, ViewportHeight, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, true, false);
	AOBlur1 = new GTexture2D(pd3dDevice, ViewportWidth, ViewportHeight, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, true, false);

	TemporalAA = new GTexture2D(pd3dDevice, ViewportWidth, ViewportHeight, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, true, false);

	SceneColorHDR = new GTexture2D(pd3dDevice, ViewportWidth, ViewportHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, true, false);
	DOF = new GTexture2D(pd3dDevice, ViewportWidth, ViewportHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, true, false);

	Velocity = new GTexture2D(pd3dDevice, ViewportWidth, ViewportHeight, DXGI_FORMAT_R32G32_FLOAT, true, false);

	FinalOutput = new GTexture2D(pd3dDevice, ViewportWidth, ViewportHeight, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, true, false);

	Depth = new GTextureDepthStencil(pd3dDevice, ViewportWidth, ViewportHeight);

	Shadow = new GTextureDepthStencil(pd3dDevice, ShadowTextureSize, ShadowTextureSize, MAX_SHADOW_CASCADE_NUM);

	D3D11_RASTERIZER_DESC	Raster_desc = CD3D11_RASTERIZER_DESC(CD3D11_DEFAULT());
	Raster_desc.CullMode = D3D11_CULL_FRONT;
	pd3dDevice->CreateRasterizerState(&Raster_desc, &Frontcull_RS);

	Raster_desc.CullMode = D3D11_CULL_BACK;
	pd3dDevice->CreateRasterizerState(&Raster_desc, &Backcull_RS);

	Raster_desc.CullMode = D3D11_CULL_NONE;
	pd3dDevice->CreateRasterizerState(&Raster_desc, &NoneCull_RS);

	Raster_desc.SlopeScaledDepthBias = 1.0f;
	pd3dDevice->CreateRasterizerState(&Raster_desc, &Backcull_ZBias_RS);

	D3D11_DEPTH_STENCIL_DESC	depth_desc = CD3D11_DEPTH_STENCIL_DESC(CD3D11_DEFAULT());
	depth_desc.DepthEnable = FALSE;
	pd3dDevice->CreateDepthStencilState(&depth_desc, &NoDepth_DSS);

	depth_desc.DepthEnable = TRUE;
	depth_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	pd3dDevice->CreateDepthStencilState(&depth_desc, &DisableWriteDepth_DSS);

	Shadow_VP.Width = (float)ShadowTextureSize;
	Shadow_VP.Height = (float)ShadowTextureSize;
	Shadow_VP.MaxDepth = 1.0f;
	Shadow_VP.MinDepth = 0.0f;
	Shadow_VP.TopLeftX = 0.0f;
	Shadow_VP.TopLeftY = 0.0f;

	Main_VP.Width = (float)ViewportWidth;
	Main_VP.Height = (float)ViewportHeight;
	Main_VP.MaxDepth = 1.0f;
	Main_VP.MinDepth = 0.0f;
	Main_VP.TopLeftX = 0.0f;
	Main_VP.TopLeftY = 0.0f;

}

void GSceneRenderer::Shutdown()
{
	SceneMesh.Destroy();
	SkyMesh.Destroy();

	SAFE_RELEASE(SceneVertexLayout);
	SAFE_RELEASE(SceneVertexShader);
	SAFE_RELEASE(ScenePixelShader);
	SAFE_RELEASE(SamplerLinear);
	SAFE_RELEASE(SamplerPoint);
	SAFE_RELEASE(SamplerLinearWrap);
	SAFE_RELEASE(SamplerPointWrap);

	SAFE_RELEASE(SkyVertexShader);
	SAFE_RELEASE(SkyPixelShader);
	SAFE_RELEASE(SkyBox_SRV);

	SAFE_RELEASE(ViewBuffer);
	SAFE_RELEASE(ModelBuffer);

	SAFE_DELETE(SceneColor);

	SAFE_DELETE(SceneColorHDR);

	SAFE_DELETE(FinalOutput);

	SAFE_DELETE(GBufferA);
	SAFE_DELETE(GBufferB);

	SAFE_DELETE(AO);
	SAFE_DELETE(AOBlur0);
	SAFE_DELETE(AOBlur1);

	SAFE_DELETE(TemporalAA);

	SAFE_DELETE(DOF);

	SAFE_DELETE(Velocity);

	SAFE_DELETE(Depth);

	SAFE_DELETE(Shadow);
	SAFE_RELEASE(ShadowVertexShader);
	SAFE_RELEASE(ShadowGeometryShader);

	SAFE_RELEASE(RSMVertexShader);
	SAFE_RELEASE(RSMPixelShader);

	SAFE_RELEASE(Backcull_RS);
	SAFE_RELEASE(Frontcull_RS);
	SAFE_RELEASE(NoneCull_RS);
	SAFE_RELEASE(Backcull_ZBias_RS);

	SAFE_RELEASE(NoDepth_DSS);
	SAFE_RELEASE(DisableWriteDepth_DSS);
}

D3DXMATRIX		GSceneRenderer::GetCascadeShadowViewProj(UINT CascadeIdx, D3DXVECTOR3 InEyePos, D3DXVECTOR3 InEyeX, D3DXVECTOR3 InEyeY, D3DXVECTOR3 InEyeZ, D3DXVECTOR3 InLight, float FOV, float ratio)
{
	float CurDepth = 0.0f;
	if (CascadeIdx > 0)
	{
		CurDepth = CascadeShadowDistances[CascadeIdx-1];
	}

	float HalfH = tan(FOV / 2.0f)*CurDepth;
	float HalfW = HalfH*ratio;
	float CornerDist = sqrt(CurDepth*CurDepth + HalfW*HalfW + HalfH*HalfH);

	D3DXVECTOR3	CornerPos[8];
	CornerPos[0] = InEyePos + (InEyeX + InEyeY + InEyeZ)*CornerDist;
	CornerPos[1] = InEyePos + (InEyeX - InEyeY + InEyeZ)*CornerDist;
	CornerPos[2] = InEyePos + (-InEyeX + InEyeY + InEyeZ)*CornerDist;
	CornerPos[3] = InEyePos + (-InEyeX - InEyeY + InEyeZ)*CornerDist;

	float NextDepth = CascadeShadowDistances[CascadeIdx];
	HalfH = (float)tan(FOV / 2.0f)*NextDepth;
	HalfW = HalfH*ratio;
	CornerDist = (float)sqrt(NextDepth*NextDepth + HalfW*HalfW + HalfH*HalfH);

	CornerPos[4] = InEyePos + (InEyeX + InEyeY + InEyeZ)*CornerDist;
	CornerPos[5] = InEyePos + (InEyeX - InEyeY + InEyeZ)*CornerDist;
	CornerPos[6] = InEyePos + (-InEyeX + InEyeY + InEyeZ)*CornerDist;
	CornerPos[7] = InEyePos + (-InEyeX - InEyeY + InEyeZ)*CornerDist;


	D3DXVECTOR3	ShadowViewDir;
	D3DXVec3Normalize(&ShadowViewDir, &InLight);

	D3DXVECTOR3	SceneCenter = (SceneBoundMin + SceneBoundMax)*0.5f;
	D3DXVECTOR3	SceneExtent = (SceneBoundMax - SceneBoundMin)*0.5f;
	float		SceneRadius = D3DXVec3Length(&SceneExtent);

	D3DXVECTOR3	CascadeCenter = InEyePos + InEyeZ*((CurDepth + NextDepth) / 2.0f);
	float		HalfZ = (NextDepth - CurDepth)*0.5f;
	float		CascadeRadius = (float)sqrt(HalfZ*HalfZ + HalfW*HalfW + HalfH*HalfH);

	D3DXVECTOR3 CascadeToSceneCenter = CascadeCenter - SceneCenter;
	float ToCenterDist = D3DXVec3Dot(&CascadeToSceneCenter, &ShadowViewDir);
	D3DXVECTOR3	ShadowViewPos = CascadeCenter + ShadowViewDir*(SceneRadius - ToCenterDist + 1.0f);

	D3DXMATRIX ShadowProj;
	D3DXMATRIX ShadowView;

	D3DXVECTOR3 vUp(0, 1, 0);
	D3DXVECTOR3 LookAtPos = ShadowViewPos - ShadowViewDir*SceneRadius;
	D3DXMatrixLookAtLH(&ShadowView, &ShadowViewPos, &LookAtPos, &vUp);

	D3DXVECTOR3	CenterLightSpacePos;
	D3DXVec3TransformCoord(&CenterLightSpacePos, &CascadeCenter, &ShadowView);
	float	ZRange = max((CenterLightSpacePos.z + CascadeRadius), 1.0f);

	D3DXMatrixOrthoLH(&ShadowProj, CascadeRadius*2.0f, CascadeRadius*2.0f, 0.1f, min(ZRange,SceneRadius*2.0f) + 1.0f);

	D3DXMATRIX	ShadowViewProj = ShadowView*ShadowProj;
	return ShadowViewProj;
}

D3DXMATRIX		GSceneRenderer::GetRSMViewProj(D3DXVECTOR3 InLight)
{
	D3DXVECTOR3	ShadowViewDir;
	D3DXVec3Normalize(&ShadowViewDir, &InLight);

	D3DXVECTOR3	SceneCenter = (SceneBoundMin + SceneBoundMax)*0.5f;
	D3DXVECTOR3	SceneExtent = (SceneBoundMax - SceneBoundMin)*0.5f;
	float		SceneRadius = D3DXVec3Length(&SceneExtent);

	D3DXVECTOR3	CascadeCenter = SceneCenter;
	float		CascadeRadius = SceneRadius;

	D3DXVECTOR3	ShadowViewPos = SceneCenter + ShadowViewDir*(SceneRadius + 1.0f);

	D3DXMATRIX ShadowProj;
	D3DXMATRIX ShadowView;

	D3DXVECTOR3 vUp(0, 1, 0);
	D3DXVECTOR3 LookAtPos = ShadowViewPos - ShadowViewDir*SceneRadius;
	D3DXMatrixLookAtLH(&ShadowView, &ShadowViewPos, &LookAtPos, &vUp);

	D3DXMatrixOrthoLH(&ShadowProj, CascadeRadius*2.0f, CascadeRadius*2.0f, 0.1f, SceneRadius*2.0f + 1.0f);

	D3DXMATRIX	ShadowViewProj = ShadowView*ShadowProj;
	return ShadowViewProj;

}

void GSceneRenderer::SetModelParams(D3DXMATRIX InWldMtx, D3DXVECTOR4 InMaterialParam)
{
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	pd3dDeviceContext->Map(ModelBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	CB_MODEL* pPerModel = (CB_MODEL*)MappedResource.pData;

	D3DXMatrixTranspose(&pPerModel->m_World, &InWldMtx);
	pPerModel->MaterialParams = InMaterialParam;

	pd3dDeviceContext->Unmap(ModelBuffer, 0);

	pd3dDeviceContext->VSSetConstantBuffers(1, 1, &ModelBuffer);
	pd3dDeviceContext->PSSetConstantBuffers(1, 1, &ModelBuffer);
	pd3dDeviceContext->CSSetConstantBuffers(1, 1, &ModelBuffer);
	pd3dDeviceContext->GSSetConstantBuffers(1, 1, &ModelBuffer);
}

void GSceneRenderer::UpdateViewBuffer(D3DXVECTOR3 vLightDir, float InExposure, D3DXVECTOR4 TemporalJitterSample)
{
	D3DXMATRIX mWorldViewProjection;
	D3DXMATRIX mWorld;
	D3DXMATRIX mView;
	D3DXMATRIX mProj;

	// Get the projection & view matrix from the camera class
	mProj = *Camera->GetProjMatrix();
	mView = *Camera->GetViewMatrix();	

	// no offset and rotation for test scene, just use identity matrix, put in view buffer just for convenience. TODO: move per object related data to another buffer   
	D3DXMatrixIdentity(&mWorld);

	D3DXMATRIX mInvView;
	D3DXMatrixInverse(&mInvView, NULL, &mView);

	D3DXVECTOR3 X(1.0f/mProj._11, 0.0f, 0.0f);
	D3DXVECTOR3 ViewX;
	D3DXVec3TransformNormal(&ViewX, &X, &mInvView);

	D3DXVECTOR3 Y(0.0f, 1.0f / mProj._22, 0.0f);
	D3DXVECTOR3 ViewY;
	D3DXVec3TransformNormal(&ViewY, &Y, &mInvView);

	D3DXVECTOR3 Z(0.0f, 0.0f, 1.0f);
	D3DXVECTOR3 ViewZ;
	D3DXVec3TransformNormal(&ViewZ, &Z, &mInvView);

	float JitterX =	(TemporalJitterSample.x*2.0f)/ViewportWidth;
	float JitterY = (TemporalJitterSample.y*2.0f)/ViewportHeight;

	float PreJitterX = (TemporalJitterSample.z*2.0f) / ViewportWidth;
	float PreJitterY = (TemporalJitterSample.w*2.0f) / ViewportHeight;

//	JitterX *= 10.0f;
//	JitterY *= 10.0f;

	if (bApplyTemporalJitter)
	{
		mProj._31 += JitterX;
		mProj._32 += JitterY;
	}

	mWorldViewProjection = mWorld * mView * mProj;

	if (bIsFirstFrame)
	{
		PreWorldViewProj = mWorldViewProjection;
		PreJitterX = JitterX;
		PreJitterY = JitterY;
	}

	// Per frame cb update
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	pd3dDeviceContext->Map(ViewBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	CB_VIEW* pPerFrame = (CB_VIEW*)MappedResource.pData;
	pPerFrame->LightParams = D3DXVECTOR4(vLightDir.x, vLightDir.y, vLightDir.z, InExposure);
	pPerFrame->LightingControl = LightingControl;

	D3DXMatrixTranspose(&pPerFrame->m_ViewProj, &mWorldViewProjection);
	D3DXMatrixTranspose(&pPerFrame->m_View, &mView);
	D3DXMatrixTranspose(&pPerFrame->m_Projection, &mProj);
	D3DXMatrixTranspose(&pPerFrame->m_PreViewProj, &PreWorldViewProj);

	pPerFrame->m_SceneBoundMin = SceneBoundMin;
	pPerFrame->m_SceneBoundMax = SceneBoundMax;

	pPerFrame->ViewSizeAndInv = D3DXVECTOR4(float(ViewportWidth), float(ViewportHeight), 1.0f / float(ViewportWidth), 1.0f / float(ViewportHeight));

	D3DXVECTOR3	EyePos = (*Camera->GetEyePt());
	pPerFrame->CameraOrigin = D3DXVECTOR4(EyePos, 1.0f);
	pPerFrame->CameraXAxis = D3DXVECTOR4(ViewX, 1.0f);
	pPerFrame->CameraYAxis = D3DXVECTOR4(ViewY, 1.0f);
	pPerFrame->CameraZAxis = D3DXVECTOR4(ViewZ, 1.0f);
	float	skycube_size_float = (float)SkyCubeSize;
	pPerFrame->SkyCubeSize = D3DXVECTOR4(skycube_size_float, skycube_size_float, 1.0f / skycube_size_float, 1.0f / skycube_size_float);

	for (UINT i = 0; i < MAX_SHADOW_CASCADE_NUM; i++)
	{
		D3DXMATRIX	ShadowViewProj = GetCascadeShadowViewProj(i, EyePos, ViewX, ViewY, ViewZ, vLightDir, CameraFOV , float(ViewportWidth) / float(ViewportHeight));
		D3DXMatrixTranspose(&(pPerFrame->ShadowViewProj[i]), &ShadowViewProj);
	}

	pPerFrame->CascadeShadowDistances = CascadeShadowDistances;

	pPerFrame->DOFParams = D3DXVECTOR4(100.0f, 500.0f, 2000.0f, 10.0f);
	pPerFrame->TemporalJitterParams = (bApplyTemporalJitter? D3DXVECTOR4(JitterX, JitterY, PreJitterX, PreJitterY) : D3DXVECTOR4(0.0f, 0.0f, 0.0f, 0.0f));

	FrameNum += 1;
	AcumulatedFrameCount += 1;
	pPerFrame->FrameNum = D3DXVECTOR4((float)FrameNum, (float)AcumulatedFrameCount, 0.0f, 0.0f);

	D3DXMATRIX	RSMViewProj = GetRSMViewProj(vLightDir);
	D3DXMatrixTranspose(&pPerFrame->m_RSMViewProj, &RSMViewProj);

	pd3dDeviceContext->Unmap(ViewBuffer, 0);

	if (!bIsFirstFrame)
	{
		PreWorldViewProj = mWorldViewProjection;
	}

	pd3dDeviceContext->VSSetConstantBuffers(0, 1, &ViewBuffer);
	pd3dDeviceContext->PSSetConstantBuffers(0, 1, &ViewBuffer);
	pd3dDeviceContext->CSSetConstantBuffers(0, 1, &ViewBuffer);
	pd3dDeviceContext->GSSetConstantBuffers(0, 1, &ViewBuffer);

	bIsFirstFrame = false;
}

void GSceneRenderer::Render(UINT InSRVCount, ID3D11ShaderResourceView** InputSRV, ID3D11RenderTargetView* OutputRTV)
{
	float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	pd3dDeviceContext->ClearRenderTargetView(SceneColor->RTV, ClearColor);
	pd3dDeviceContext->ClearDepthStencilView(Depth->DSV, D3D11_CLEAR_DEPTH, 1.0, 0);

	pd3dDeviceContext->ClearRenderTargetView(GBufferA->RTV, ClearColor);
	float NormalClearColor[4] = { 0.5f, 0.5f, 0.5f, 0.0f };
	pd3dDeviceContext->ClearRenderTargetView(GBufferB->RTV, ClearColor);

	ID3D11RenderTargetView*	GBuffers[2] = { GBufferA->RTV, GBufferB->RTV };

	pd3dDeviceContext->OMSetRenderTargets(2, GBuffers, Depth->DSV);

	//Get the mesh
	//IA setup
	pd3dDeviceContext->IASetInputLayout(SceneVertexLayout);

	pd3dDeviceContext->RSSetState(Backcull_RS);

	// Set the shaders
	pd3dDeviceContext->VSSetShader(SceneVertexShader, NULL, 0);
	pd3dDeviceContext->PSSetShader(ScenePixelShader, NULL, 0);

	SceneMesh.Render(pd3dDeviceContext, 0, 1);

	ID3D11RenderTargetView* pRTVs[16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
	pd3dDeviceContext->OMSetRenderTargets(8, pRTVs, NULL);
}

void GSceneRenderer::RenderScene()
{
	D3DXMATRIX mWorld;
	D3DXMatrixIdentity(&mWorld);

	SetModelParams(mWorld, MaterialParams);
	SceneMesh.Render(pd3dDeviceContext, 0, 1);
}

void GSceneRenderer::RenderSkybox(ID3D11RenderTargetView* OutputRTV)
{
	pd3dDeviceContext->OMSetRenderTargets(1, &OutputRTV, Depth->DSV);
	pd3dDeviceContext->IASetInputLayout(SceneVertexLayout);

	pd3dDeviceContext->RSSetState(Frontcull_RS);
	pd3dDeviceContext->OMSetDepthStencilState(DisableWriteDepth_DSS, 0);

	pd3dDeviceContext->VSSetShader(SkyVertexShader, NULL, 0);
	pd3dDeviceContext->PSSetShader(SkyPixelShader, NULL, 0);
	pd3dDeviceContext->PSSetShaderResources(0, 1, &SkyBox_SRV);

	D3DXMATRIX mWorld;
	D3DXMatrixIdentity(&mWorld);

	SetModelParams(mWorld, MaterialParams);

	SkyMesh.Render(pd3dDeviceContext);

	pd3dDeviceContext->OMSetDepthStencilState(nullptr, 0);
	pd3dDeviceContext->RSSetState(Backcull_RS);

	ID3D11RenderTargetView* pRTVs[16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
	pd3dDeviceContext->OMSetRenderTargets(8, pRTVs, NULL);

	ID3D11Buffer* ppCBNULL[1] = { NULL };
	pd3dDeviceContext->VSSetConstantBuffers(1, 1, ppCBNULL);
	pd3dDeviceContext->PSSetConstantBuffers(1, 1, ppCBNULL);
	pd3dDeviceContext->CSSetConstantBuffers(1, 1, ppCBNULL);
	pd3dDeviceContext->GSSetConstantBuffers(1, 1, ppCBNULL);
}

void GSceneRenderer::RenderShadow()
{
	pd3dDeviceContext->RSSetViewports(1, &Shadow_VP);
	pd3dDeviceContext->ClearDepthStencilView(Shadow->DSV, D3D11_CLEAR_DEPTH, 1.0, 0);

	ID3D11RenderTargetView*	NullView[1] = { NULL };
	pd3dDeviceContext->OMSetRenderTargets(1, NullView, Shadow->DSV);

	pd3dDeviceContext->IASetInputLayout(SceneVertexLayout);
	pd3dDeviceContext->RSSetState(Backcull_ZBias_RS);

	// Set the shaders
	pd3dDeviceContext->VSSetShader(ShadowVertexShader, NULL, 0);
	pd3dDeviceContext->PSSetShader(nullptr, NULL, 0);
	pd3dDeviceContext->GSSetShader(ShadowGeometryShader, NULL, 0);

	RenderScene();

	ID3D11RenderTargetView* pRTVs[16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
	pd3dDeviceContext->OMSetRenderTargets(8, pRTVs, NULL);

	pd3dDeviceContext->RSSetViewports(1, &Main_VP);
	pd3dDeviceContext->GSSetShader(NULL, NULL, 0);

}

void GSceneRenderer::RenderRSM(ID3D11RenderTargetView* OutputRTV, ID3D11UnorderedAccessView** OutputUAVs, ID3D11DepthStencilView* OutputDSV)
{
	pd3dDeviceContext->RSSetViewports(1, &Shadow_VP);
	pd3dDeviceContext->ClearDepthStencilView(OutputDSV, D3D11_CLEAR_DEPTH, 1.0, 0);

	UINT	InitCounter[4] = { 0, 0, 0, 0 };
	pd3dDeviceContext->OMSetRenderTargetsAndUnorderedAccessViews(1, &OutputRTV, OutputDSV, 1, 4, OutputUAVs, InitCounter);

	pd3dDeviceContext->IASetInputLayout(SceneVertexLayout);
	pd3dDeviceContext->RSSetState(Backcull_RS);

	// Set the shaders
	pd3dDeviceContext->VSSetShader(RSMVertexShader, NULL, 0);
	pd3dDeviceContext->PSSetShader(RSMPixelShader, NULL, 0);
	pd3dDeviceContext->PSSetShaderResources(6, 1, &Shadow->SRV);

	RenderScene();

	ID3D11RenderTargetView* pRTVs[16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
	pd3dDeviceContext->OMSetRenderTargets(8, pRTVs, NULL);

	pd3dDeviceContext->RSSetViewports(1, &Main_VP);
	pd3dDeviceContext->GSSetShader(NULL, NULL, 0);
}

void GSceneRenderer::Voxelization()
{
	pd3dDeviceContext->IASetInputLayout(SceneVertexLayout);
	pd3dDeviceContext->RSSetState(NoneCull_RS);
	pd3dDeviceContext->OMSetDepthStencilState(NoDepth_DSS, 0);

	RenderScene();

	ID3D11RenderTargetView* pRTVs[16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
	pd3dDeviceContext->OMSetRenderTargets(8, pRTVs, NULL);

	pd3dDeviceContext->RSSetViewports(1, &Main_VP);
	pd3dDeviceContext->GSSetShader(NULL, NULL, 0);

	pd3dDeviceContext->OMSetDepthStencilState(nullptr, 0);
	pd3dDeviceContext->RSSetState(Backcull_RS);
}

void GSceneRenderer::SetSamplerStates()
{
	pd3dDeviceContext->VSSetSamplers(0, 1, &SamplerLinear);
	pd3dDeviceContext->VSSetSamplers(1, 1, &SamplerPoint);
	pd3dDeviceContext->VSSetSamplers(2, 1, &SamplerLinearWrap);
	pd3dDeviceContext->VSSetSamplers(3, 1, &SamplerPointWrap);

	pd3dDeviceContext->PSSetSamplers(0, 1, &SamplerLinear);
	pd3dDeviceContext->PSSetSamplers(1, 1, &SamplerPoint);
	pd3dDeviceContext->PSSetSamplers(2, 1, &SamplerLinearWrap);
	pd3dDeviceContext->PSSetSamplers(3, 1, &SamplerPointWrap);

	pd3dDeviceContext->CSSetSamplers(0, 1, &SamplerLinear);
	pd3dDeviceContext->CSSetSamplers(1, 1, &SamplerPoint);
	pd3dDeviceContext->CSSetSamplers(2, 1, &SamplerLinearWrap);
	pd3dDeviceContext->CSSetSamplers(3, 1, &SamplerPointWrap);
}