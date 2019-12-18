#include "DXUT.h"
#include "SDKmisc.h"

#include "RenderUtils.h"

#include "RayTraceSphere.h"
#include "SceneRenderer.h"

float	Rand01()
{
	float R = (float)rand() / (float)RAND_MAX;
	return R;
}

void GRayTraceSphere::Init(const GRenderContext& InContext)
{
	GRenderPass::Init(InContext);

	const UINT  MaxRayNumPerPixel = 16;
	const UINT	ObjNum = 9;
	//const UINT	ObjNum = 104;

	HitableObjectList = new HitableObject[ObjNum];
	
	HitableObjectList[0].Position = D3DXVECTOR4(0.0f, -50000.0f, 0.0f, 50000.0f);
	HitableObjectList[0].Color = D3DXVECTOR4(0.75f, 0.75f, 0.75f, 1.0f);
	HitableObjectList[0].MaterialParams = D3DXVECTOR4(0.9f, 0.0f, 0.0f, 0.0f);

	HitableObjectList[1].Position = D3DXVECTOR4(0.0f, 50800.0f, 0.0f, 50000.0f);
	HitableObjectList[1].Color = D3DXVECTOR4(0.75f, 0.75f, 0.75f, 1.0f);
	HitableObjectList[1].MaterialParams = D3DXVECTOR4(0.9f, 0.0f, 0.0f, 0.0f);

	HitableObjectList[2].Position = D3DXVECTOR4(0.0f, 300.0f, 51000.0f, 50000.0f);
	HitableObjectList[2].Color = D3DXVECTOR4(1.0f, 0.1f, 0.1f, 1.0f);
	HitableObjectList[2].MaterialParams = D3DXVECTOR4(0.9f, 0.0f, 0.0f, 0.0f);

	HitableObjectList[3].Position = D3DXVECTOR4(0.0f, 300.0f, -51000.0f, 50000.0f);
	HitableObjectList[3].Color = D3DXVECTOR4(0.0f, 1.0f, 0.1f, 1.0f);
	HitableObjectList[3].MaterialParams = D3DXVECTOR4(0.9f, 0.0f, 0.0f, 0.0f);

	HitableObjectList[4].Position = D3DXVECTOR4(-51000.0f, 300.0f, 0.0f, 50000.0f);
	HitableObjectList[4].Color = D3DXVECTOR4(0.1f, 0.1f, 1.0f, 1.0f);
	HitableObjectList[4].MaterialParams = D3DXVECTOR4(0.9f, 0.0f, 0.0f, 0.0f);

	HitableObjectList[5].Position = D3DXVECTOR4(51000.0f, 300.0f, 0.0f, 50000.0f);
	HitableObjectList[5].Color = D3DXVECTOR4(1.0f, 1.0f, 1.0f, 1.0f);
	HitableObjectList[5].MaterialParams = D3DXVECTOR4(0.9f, 0.0f, 0.0f, 0.0f);

	HitableObjectList[6].Position = D3DXVECTOR4(0.0f, 200.0f, 0.0f, 100.0f);
	HitableObjectList[6].Color = D3DXVECTOR4(0.9f, 0.9f, 0.9f, 1.0f);
	HitableObjectList[6].MaterialParams = D3DXVECTOR4(0.99f, 0.1f, 0.0f, 0.0f);

	HitableObjectList[7].Position = D3DXVECTOR4(0.0f, 50.0f, -350.0f, 50.0f);
	HitableObjectList[7].Color = D3DXVECTOR4(0.8f, 0.6f, 0.2f, 1.0f);
	HitableObjectList[7].MaterialParams = D3DXVECTOR4(0.1f, 1.0f, 0.0f, 1.0f);

	HitableObjectList[8].Position = D3DXVECTOR4(0.0f, 50.0f, 350.0f, 50.0f);
	HitableObjectList[8].Color = D3DXVECTOR4(0.1f, 0.8f, 0.1f, 1.0f);
	HitableObjectList[8].MaterialParams = D3DXVECTOR4(0.8f, 0.9f, 1.5f, 2.0f);

	/*
	HitableObjectList[0].Position = D3DXVECTOR4(0, -10000, 0, 10000);
	HitableObjectList[0].Color = D3DXVECTOR4(0.5, 0.5, 0.5, 1.0f);
	HitableObjectList[0].MaterialParams = D3DXVECTOR4(0.0f, 0.0f, 0.0f, 0.0f);

	int i = 1;
	for (int a = -5; a < 5; a++) {
		for (int b = -5; b < 5; b++) {
			float choose_mat = Rand01();
			D3DXVECTOR4 center(a*20 + 9*Rand01(), 2, b*20 + 9*Rand01(), 2);
			if (choose_mat < 0.8) {  // diffuse
				HitableObjectList[i].Position = center;
				HitableObjectList[i].Color = D3DXVECTOR4(Rand01()*Rand01(), Rand01()*Rand01(), Rand01()*Rand01(), 1.0f);
				HitableObjectList[i].MaterialParams = D3DXVECTOR4(0.0f, 0.0f, 0.0f, 0.0f);
			}
			else if (choose_mat < 0.95) { // metal
				HitableObjectList[i].Position = center;
				HitableObjectList[i].Color = D3DXVECTOR4(0.5*(1 + Rand01()), 0.5*(1 + Rand01()), 0.5*(1 + Rand01()), 1.0f);
				HitableObjectList[i].MaterialParams = D3DXVECTOR4(0.0f, 0.5*Rand01(), 0.0f, 1.0f);
			}
			else {  // glass
				HitableObjectList[i].Position = center;
				HitableObjectList[i].Color = D3DXVECTOR4(0.0f, 0.0f, 0.0f, 1.0f);
				HitableObjectList[i].MaterialParams = D3DXVECTOR4(0.0f, 0.0f, 1.5f, 2.0f);
			}
			i++;
		}
	}

	HitableObjectList[i].Position = D3DXVECTOR4(0, 10, 0, 10);
	HitableObjectList[i].Color = D3DXVECTOR4(0.5, 0.5, 0.5, 1.0f);
	HitableObjectList[i].MaterialParams = D3DXVECTOR4(0.0f, 0.0f, 1.5f, 2.0f);
	i++;

	HitableObjectList[i].Position = D3DXVECTOR4(-40, 10, 0, 10);
	HitableObjectList[i].Color = D3DXVECTOR4(0.4, 0.2, 0.1, 1.0f);
	HitableObjectList[i].MaterialParams = D3DXVECTOR4(0.0f, 0.0f, 0.0f, 0.0f);
	i++;

	HitableObjectList[i].Position = D3DXVECTOR4(40, 10, 0, 10);
	HitableObjectList[i].Color = D3DXVECTOR4(0.7, 0.6, 0.5, 1.0f);
	HitableObjectList[i].MaterialParams = D3DXVECTOR4(0.0f, 0.0f, 0.0f, 1.0f);
	i++;
	*/

	D3D11_BUFFER_DESC	BufferDesc;
	ZeroMemory(&BufferDesc, sizeof(BufferDesc));
	BufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	BufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	BufferDesc.StructureByteStride = sizeof(float) * 4 * 3;
	BufferDesc.ByteWidth = BufferDesc.StructureByteStride*ObjNum;

	D3D11_SUBRESOURCE_DATA ResourceData;
	ResourceData.pSysMem = HitableObjectList;

	pd3dDevice->CreateBuffer(&BufferDesc, &ResourceData, &HitableObjectsBuffer);

	D3D11_SHADER_RESOURCE_VIEW_DESC Srv_desc;
	ZeroMemory(&Srv_desc, sizeof(Srv_desc));
	Srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
	Srv_desc.BufferEx.FirstElement = 0;
	Srv_desc.BufferEx.NumElements = ObjNum;
	Srv_desc.Format = DXGI_FORMAT_UNKNOWN;
	pd3dDevice->CreateShaderResourceView(HitableObjectsBuffer, &Srv_desc, &HitableObjects_SRV);


	ColorBuffer = new GStructuredBuffer(pd3dDevice, sizeof(float) * 4, ViewportWidth*ViewportHeight);

	Moments = new GStructuredBuffer(pd3dDevice, sizeof(float) * 4, ViewportWidth*ViewportHeight);

	HistoryCount = new GStructuredBuffer(pd3dDevice, sizeof(int), ViewportWidth*ViewportHeight, DXGI_FORMAT_R32_UINT);

	RayHeader0 = new GStructuredBuffer(pd3dDevice, sizeof(int), ViewportWidth*ViewportHeight, DXGI_FORMAT_R32_SINT, true);
	RayHeader1 = new GStructuredBuffer(pd3dDevice, sizeof(int), ViewportWidth*ViewportHeight, DXGI_FORMAT_R32_SINT, true);

	RayBuffer0 = new GStructuredBuffer(pd3dDevice, sizeof(Ray), ViewportWidth*ViewportHeight*MaxRayNumPerPixel, DXGI_FORMAT_UNKNOWN, true, true);
	RayBuffer1 = new GStructuredBuffer(pd3dDevice, sizeof(Ray), ViewportWidth*ViewportHeight*MaxRayNumPerPixel, DXGI_FORMAT_UNKNOWN, true, true);


	float	ClearValue[4] = { 0, 0, 0, 0 };

	UINT	ClearValueUint[4] = { 0, 0, 0, 0 };
	pd3dDeviceContext->ClearUnorderedAccessViewUint(HistoryCount->UAV, ClearValueUint);

	History0 = new GTexture2D(pd3dDevice, ViewportWidth, ViewportHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, false, true);
	History1 = new GTexture2D(pd3dDevice, ViewportWidth, ViewportHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, false, true);

	NormalDepth0 = new GTexture2D(pd3dDevice, ViewportWidth, ViewportHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, false, true);
	NormalDepth1 = new GTexture2D(pd3dDevice, ViewportWidth, ViewportHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, false, true);

	DirectColor = new GTexture2D(pd3dDevice, ViewportWidth, ViewportHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, false, true);
	IndirectColor = new GTexture2D(pd3dDevice, ViewportWidth, ViewportHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, false, true);
	IndirectColor1 = new GTexture2D(pd3dDevice, ViewportWidth, ViewportHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, false, true);
	IndirectColor2 = new GTexture2D(pd3dDevice, ViewportWidth, ViewportHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, false, true);

	AtrousColor0 = new GTexture2D(pd3dDevice, ViewportWidth, ViewportHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, false, true);
	AtrousColor1 = new GTexture2D(pd3dDevice, ViewportWidth, ViewportHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, false, true);

	pd3dDeviceContext->ClearUnorderedAccessViewFloat(History0->UAV, ClearValue);
	pd3dDeviceContext->ClearUnorderedAccessViewFloat(History1->UAV, ClearValue);

	pd3dDeviceContext->ClearUnorderedAccessViewFloat(NormalDepth0->UAV, ClearValue);
	pd3dDeviceContext->ClearUnorderedAccessViewFloat(NormalDepth1->UAV, ClearValue);

	D3D_SHADER_MACRO defines[2] = {
		"ATROUS_STEP_SIZE", "1",
		NULL, NULL
	};

	D3D_SHADER_MACRO combine_defines[2] = {
		"USE_SIMPLE_ACCUM", "0",
		NULL, NULL
	};

	ID3DBlob* ShaderBlob = nullptr;
	CompileShaderFromFile(L"Shaders\\RayTraceSphere.hlsl", NULL, "RayTraceCS", "cs_5_0", &ShaderBlob);
	pd3dDevice->CreateComputeShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &RayTraceShader);
	SAFE_RELEASE(ShaderBlob);

	CompileShaderFromFile(L"Shaders\\RayTraceSphere.hlsl", NULL, "RayGenCS", "cs_5_0", &ShaderBlob);
	pd3dDevice->CreateComputeShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &RayGenShader);
	SAFE_RELEASE(ShaderBlob);

	CompileShaderFromFile(L"Shaders\\RayTraceSphere.hlsl", NULL, "RayBounceCS", "cs_5_0", &ShaderBlob);
	pd3dDevice->CreateComputeShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &RayBounceShader);
	SAFE_RELEASE(ShaderBlob);

	CompileShaderFromFile(L"Shaders\\RayTraceSphere.hlsl", combine_defines, "RayColorCombineCS", "cs_5_0", &ShaderBlob);
	pd3dDevice->CreateComputeShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &RayColorCombineShader);
	SAFE_RELEASE(ShaderBlob);

	combine_defines[0] = { "USE_SIMPLE_ACCUM", "1" };

	CompileShaderFromFile(L"Shaders\\RayTraceSphere.hlsl", combine_defines, "RayColorCombineCS", "cs_5_0", &ShaderBlob);
	pd3dDevice->CreateComputeShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &RayColorCombineSimpleAccumShader);
	SAFE_RELEASE(ShaderBlob);

	CompileShaderFromFile(L"Shaders\\RayTraceSphere.hlsl", NULL, "SVGFSetupCS", "cs_5_0", &ShaderBlob);
	pd3dDevice->CreateComputeShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &SVGFSetupShader);
	SAFE_RELEASE(ShaderBlob);

	CompileShaderFromFile(L"Shaders\\SVGF.hlsl", defines, "SVGFReprojectCS", "cs_5_0", &ShaderBlob);
	pd3dDevice->CreateComputeShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &SVGFReprojectShader);
	SAFE_RELEASE(ShaderBlob);

	CompileShaderFromFile(L"Shaders\\SVGF.hlsl", defines, "SVGFMomentFilterCS", "cs_5_0", &ShaderBlob);
	pd3dDevice->CreateComputeShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &SVGFMomentFilterShader);
	SAFE_RELEASE(ShaderBlob);


	CompileShaderFromFile(L"Shaders\\SVGF.hlsl", defines, "SVGFAtrousCS", "cs_5_0", &ShaderBlob);
	pd3dDevice->CreateComputeShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &SVGFAtrous1Shader);
	SAFE_RELEASE(ShaderBlob);

	defines[0] = {"ATROUS_STEP_SIZE", "2" };
	CompileShaderFromFile(L"Shaders\\SVGF.hlsl", defines, "SVGFAtrousCS", "cs_5_0", &ShaderBlob);
	pd3dDevice->CreateComputeShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &SVGFAtrous2Shader);
	SAFE_RELEASE(ShaderBlob);

	defines[0] = { "ATROUS_STEP_SIZE", "4" };
	CompileShaderFromFile(L"Shaders\\SVGF.hlsl", defines, "SVGFAtrousCS", "cs_5_0", &ShaderBlob);
	pd3dDevice->CreateComputeShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &SVGFAtrous3Shader);
	SAFE_RELEASE(ShaderBlob);

	defines[0] = { "ATROUS_STEP_SIZE", "8" };
	CompileShaderFromFile(L"Shaders\\SVGF.hlsl", defines, "SVGFAtrousCS", "cs_5_0", &ShaderBlob);
	pd3dDevice->CreateComputeShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &SVGFAtrous4Shader);
	SAFE_RELEASE(ShaderBlob);

	defines[0] = { "ATROUS_STEP_SIZE", "16" };
	CompileShaderFromFile(L"Shaders\\SVGF.hlsl", defines, "SVGFAtrousCS", "cs_5_0", &ShaderBlob);
	pd3dDevice->CreateComputeShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &SVGFAtrous5Shader);
	SAFE_RELEASE(ShaderBlob);

}

void GRayTraceSphere::Render(UINT InSRVCount, ID3D11ShaderResourceView** InputSRV, ID3D11RenderTargetView* OutputRTV)
{
	GSceneRenderer*	pSceneRenderer = OwnerMgr->GetSceneRenderPass();

	ID3D11UnorderedAccessView*	Cur_UAV = (bIsUsingHistory0 ? History0->UAV : History1->UAV);
	float	ClearValue[4] = { 0, 0, 0, 1 };
	pd3dDeviceContext->ClearUnorderedAccessViewFloat(Cur_UAV, ClearValue);


	if (bUseSimpleAccum)
	{
		if (pSceneRenderer->AcumulatedFrameCount == 1)
		{
			pd3dDeviceContext->ClearUnorderedAccessViewFloat(bIsUsingHistory0 ? History1->UAV : History0->UAV, ClearValue);
		}
	}

	ID3D11ShaderResourceView* ppSRVNULL[6] = { NULL, NULL, NULL, NULL, NULL, NULL };
	ID3D11UnorderedAccessView* ppUAViewNULL[4] = { NULL, NULL, NULL, NULL };

/*	pd3dDeviceContext->CSSetShader(RayTraceShader, NULL, 0);
	pd3dDeviceContext->CSSetShaderResources(0, 1, &HitableObjects_SRV);
	pd3dDeviceContext->CSSetShaderResources(1, 1, bIsUsingHistory0 ? &History1->SRV : &History0->SRV);

	pd3dDeviceContext->CSSetUnorderedAccessViews(0, 1, &Cur_UAV, nullptr);

	pd3dDeviceContext->Dispatch(ViewportWidth / 16, ViewportHeight / 16, 1);
*/

	UINT	InitCounter[4] = { 0, 0, 0, 0};
	ID3D11UnorderedAccessView* ppUAViews[4] = { ColorBuffer->UAV, RayHeader0->UAV, RayBuffer0->UAV, DirectColor->UAV };

	pd3dDeviceContext->CSSetUnorderedAccessViews(0, 4, ppUAViews, InitCounter);

	pd3dDeviceContext->CSSetShader(RayGenShader, NULL, 0);
	pd3dDeviceContext->CSSetShaderResources(0, 1, &HitableObjects_SRV);

	pd3dDeviceContext->Dispatch(ViewportWidth / 16, ViewportHeight / 16, 1);
		
	///// ray bounce multiple times

	BOOL bUseRayBuffer1 = true;
	for (UINT BounceIdx = 0; BounceIdx < 2; ++BounceIdx)
	{
		bUseRayBuffer1 = (BounceIdx % 2 == 0);

		ppUAViews[1] = (bUseRayBuffer1 ? RayHeader1->UAV : RayHeader0->UAV);
		ppUAViews[2] = (bUseRayBuffer1 ? RayBuffer1->UAV : RayBuffer0->UAV);
		ppUAViews[3] = nullptr;

		pd3dDeviceContext->CSSetShaderResources(0, 6, ppSRVNULL);

		pd3dDeviceContext->CSSetUnorderedAccessViews(0, 4, ppUAViews, InitCounter);

		pd3dDeviceContext->CSSetShaderResources(0, 1, &HitableObjects_SRV);
		pd3dDeviceContext->CSSetShaderResources(2, 1, bUseRayBuffer1 ? &RayHeader0->SRV : &RayHeader1->SRV);
		pd3dDeviceContext->CSSetShaderResources(3, 1, bUseRayBuffer1 ? &RayBuffer0->SRV : &RayBuffer1->SRV);

		pd3dDeviceContext->CSSetShader(RayBounceShader, NULL, 0);

		pd3dDeviceContext->Dispatch(ViewportWidth / 16, ViewportHeight / 16, 1);
	}

	//// simple accumulate over time

	if (bUseSimpleAccum)
	{
		pd3dDeviceContext->CSSetUnorderedAccessViews(0, 1, &Cur_UAV, nullptr);
		pd3dDeviceContext->CSSetShaderResources(0, 1, &ColorBuffer->SRV);
		pd3dDeviceContext->CSSetShaderResources(1, 1, bIsUsingHistory0 ? &History1->SRV : &History0->SRV);
		pd3dDeviceContext->CSSetShaderResources(4, 1, &DirectColor->SRV);
		pd3dDeviceContext->CSSetShader(RayColorCombineSimpleAccumShader, NULL, 0);

		pd3dDeviceContext->Dispatch(ViewportWidth / 16, ViewportHeight / 16, 1);
	}
	else
	{
		//// SVFG

		pd3dDeviceContext->CSSetUnorderedAccessViews(0, 1, &(bIsUsingHistory0 ? NormalDepth0->UAV : NormalDepth1->UAV), nullptr);
		pd3dDeviceContext->CSSetShader(SVGFSetupShader, NULL, 0);
		pd3dDeviceContext->CSSetShaderResources(0, 1, &HitableObjects_SRV);
		pd3dDeviceContext->Dispatch(ViewportWidth / 16, ViewportHeight / 16, 1);

		ID3D11UnorderedAccessView* reproject_UAViews[3] = { IndirectColor1->UAV, Moments->UAV, HistoryCount->UAV };
		pd3dDeviceContext->CSSetUnorderedAccessViews(0, 3, reproject_UAViews, nullptr);
		pd3dDeviceContext->CSSetShaderResources(0, 1, &ColorBuffer->SRV);
		pd3dDeviceContext->CSSetShaderResources(1, 1, bIsUsingHistory0 ? &NormalDepth0->SRV : &NormalDepth1->SRV);
		pd3dDeviceContext->CSSetShaderResources(2, 1, bIsUsingHistory0 ? &NormalDepth1->SRV : &NormalDepth0->SRV);
		pd3dDeviceContext->CSSetShaderResources(4, 1, &IndirectColor->SRV);
		pd3dDeviceContext->CSSetShader(SVGFReprojectShader, NULL, 0);

		pd3dDeviceContext->Dispatch(ViewportWidth / 16, ViewportHeight / 16, 1);

		reproject_UAViews[0] = IndirectColor2->UAV;
		reproject_UAViews[1] = nullptr;
		reproject_UAViews[2] = nullptr;
		pd3dDeviceContext->CSSetUnorderedAccessViews(0, 3, reproject_UAViews, nullptr);
		pd3dDeviceContext->CSSetShaderResources(0, 1, &HistoryCount->SRV);
		pd3dDeviceContext->CSSetShaderResources(1, 1, bIsUsingHistory0 ? &NormalDepth0->SRV : &NormalDepth1->SRV);
		pd3dDeviceContext->CSSetShaderResources(3, 1, &Moments->SRV);
		pd3dDeviceContext->CSSetShaderResources(4, 1, &IndirectColor1->SRV);
		pd3dDeviceContext->CSSetShader(SVGFMomentFilterShader, NULL, 0);

		pd3dDeviceContext->Dispatch(ViewportWidth / 16, ViewportHeight / 16, 1);

		reproject_UAViews[0] = IndirectColor->UAV;
		reproject_UAViews[1] = AtrousColor0->UAV;
		pd3dDeviceContext->CSSetUnorderedAccessViews(0, 3, reproject_UAViews, nullptr);
		pd3dDeviceContext->CSSetShaderResources(4, 1, &IndirectColor2->SRV);
		pd3dDeviceContext->CSSetShader(SVGFAtrous1Shader, NULL, 0);
		pd3dDeviceContext->Dispatch(ViewportWidth / 16, ViewportHeight / 16, 1);

		reproject_UAViews[0] = AtrousColor1->UAV;
		reproject_UAViews[1] = nullptr;
		pd3dDeviceContext->CSSetUnorderedAccessViews(0, 3, reproject_UAViews, nullptr);
		pd3dDeviceContext->CSSetShaderResources(4, 1, &AtrousColor0->SRV);
		pd3dDeviceContext->CSSetShader(SVGFAtrous2Shader, NULL, 0);
		pd3dDeviceContext->Dispatch(ViewportWidth / 16, ViewportHeight / 16, 1);

		reproject_UAViews[0] = AtrousColor0->UAV;
		pd3dDeviceContext->CSSetShaderResources(4, 1, ppSRVNULL);
		pd3dDeviceContext->CSSetUnorderedAccessViews(0, 3, reproject_UAViews, nullptr);
		pd3dDeviceContext->CSSetShaderResources(4, 1, &AtrousColor1->SRV);
		pd3dDeviceContext->CSSetShader(SVGFAtrous3Shader, NULL, 0);
		pd3dDeviceContext->Dispatch(ViewportWidth / 16, ViewportHeight / 16, 1);

		reproject_UAViews[0] = AtrousColor1->UAV;
		pd3dDeviceContext->CSSetShaderResources(4, 1, ppSRVNULL);
		pd3dDeviceContext->CSSetUnorderedAccessViews(0, 3, reproject_UAViews, nullptr);
		pd3dDeviceContext->CSSetShaderResources(4, 1, &AtrousColor0->SRV);
		pd3dDeviceContext->CSSetShader(SVGFAtrous4Shader, NULL, 0);
		pd3dDeviceContext->Dispatch(ViewportWidth / 16, ViewportHeight / 16, 1);

		reproject_UAViews[0] = AtrousColor0->UAV;
		pd3dDeviceContext->CSSetShaderResources(4, 1, ppSRVNULL);
		pd3dDeviceContext->CSSetUnorderedAccessViews(0, 3, reproject_UAViews, nullptr);
		pd3dDeviceContext->CSSetShaderResources(4, 1, &AtrousColor1->SRV);
		pd3dDeviceContext->CSSetShader(SVGFAtrous5Shader, NULL, 0);
		pd3dDeviceContext->Dispatch(ViewportWidth / 16, ViewportHeight / 16, 1);

		pd3dDeviceContext->CSSetUnorderedAccessViews(0, 4, ppUAViewNULL, NULL);

		pd3dDeviceContext->CSSetShaderResources(0, 6, ppSRVNULL);
		pd3dDeviceContext->CSSetUnorderedAccessViews(0, 1, &Cur_UAV, nullptr);
		pd3dDeviceContext->CSSetShaderResources(5, 1, &AtrousColor0->SRV);
		pd3dDeviceContext->CSSetShaderResources(4, 1, &DirectColor->SRV);
		pd3dDeviceContext->CSSetShader(RayColorCombineShader, NULL, 0);

		pd3dDeviceContext->Dispatch(ViewportWidth / 16, ViewportHeight / 16, 1);
	}
	///////////////////////

	pd3dDeviceContext->CSSetShader(NULL, NULL, 0);

//	ID3D11UnorderedAccessView* ppUAViewNULL[4] = { NULL, NULL, NULL, NULL };
	pd3dDeviceContext->CSSetUnorderedAccessViews(0, 4, ppUAViewNULL, NULL);

	pd3dDeviceContext->CSSetShaderResources(0, 6, ppSRVNULL);

	bIsUsingHistory0 = (!bIsUsingHistory0);
}

void GRayTraceSphere::Shutdown()
{
	SAFE_RELEASE(RayTraceShader);
	SAFE_RELEASE(RayGenShader);
	SAFE_RELEASE(RayBounceShader);
	SAFE_RELEASE(RayColorCombineShader);
	SAFE_RELEASE(RayColorCombineSimpleAccumShader);

	SAFE_RELEASE(SVGFSetupShader);
	SAFE_RELEASE(SVGFReprojectShader);
	SAFE_RELEASE(SVGFMomentFilterShader);
	SAFE_RELEASE(SVGFAtrous1Shader);
	SAFE_RELEASE(SVGFAtrous2Shader);
	SAFE_RELEASE(SVGFAtrous3Shader);
	SAFE_RELEASE(SVGFAtrous4Shader);
	SAFE_RELEASE(SVGFAtrous5Shader);

	SAFE_RELEASE(HitableObjects_SRV);
	SAFE_RELEASE(HitableObjectsBuffer);

	SAFE_DELETE(History0);
	SAFE_DELETE(History1);

	SAFE_DELETE(NormalDepth0);
	SAFE_DELETE(NormalDepth1);

	SAFE_DELETE(DirectColor);

	SAFE_DELETE(RayHeader0);
	SAFE_DELETE(RayHeader1);

	SAFE_DELETE(RayBuffer0);
	SAFE_DELETE(RayBuffer1);

	SAFE_DELETE(ColorBuffer);

	SAFE_DELETE(IndirectColor);
	SAFE_DELETE(IndirectColor1);
	SAFE_DELETE(IndirectColor2);

	SAFE_DELETE(AtrousColor0);
	SAFE_DELETE(AtrousColor1);

	SAFE_DELETE(Moments);

	SAFE_DELETE(HistoryCount);

	delete[] HitableObjectList;
}
