#include "DXUT.h"
#include "SDKmisc.h"
#include "TiledDeferredLighting.h"
#include "RenderUtils.h"
#include <time.h>

float	RandFloat(float InMin, float InMax)
{
	float R = (float)rand() / (float)RAND_MAX;

	return InMin + (InMax - InMin)*R;
}

GTiledDeferredLighting::GTiledDeferredLighting()
{

}

GTiledDeferredLighting::~GTiledDeferredLighting()
{

}

void GTiledDeferredLighting::Init(const GRenderContext& InContext)
{
	GRenderPass::Init(InContext);

	TiledWidth = ViewportWidth / TileSize;
	TiledHeight = ViewportHeight / TileSize;

	ID3DBlob* ShaderBlob = nullptr;
	
	CompileShaderFromFile(L"Shaders\\TiledDeferredLighting.hlsl", NULL, "FillTiledLightList", "cs_5_0", &ShaderBlob);
	pd3dDevice->CreateComputeShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &LightListShader);
	SAFE_RELEASE(ShaderBlob);

	PointLightArray = new PointLightInfo[PointLightCount];

	srand(time(nullptr));

	for (UINT i = 0; i < PointLightCount; ++i)
	{
		D3DXVECTOR4		Color(RandFloat(0.5f, 1.0f), RandFloat(0.5f, 1.0f), RandFloat(0.5f, 1.0f), RandFloat(10.0f, 30.0f));
		PointLightArray[i].Color = Color;
		PointLightArray[i].Position = D3DXVECTOR4(RandFloat(SceneBoundMin.x, SceneBoundMax.x)*0.65f, RandFloat(SceneBoundMin.y, SceneBoundMax.y), RandFloat(SceneBoundMin.z, SceneBoundMax.z)*0.65f, RandFloat(100.0f, 150.0f));
	}

	D3D11_BUFFER_DESC BufferDesc;
	ZeroMemory(&BufferDesc, sizeof(BufferDesc));
	BufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	BufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	BufferDesc.StructureByteStride = sizeof(float) * 4 * 2;
	BufferDesc.ByteWidth = BufferDesc.StructureByteStride*PointLightCount;
	BufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	BufferDesc.Usage = D3D11_USAGE_DYNAMIC;

	D3D11_SUBRESOURCE_DATA ResourceData;
	ResourceData.pSysMem = PointLightArray;

	pd3dDevice->CreateBuffer(&BufferDesc, &ResourceData, &PointLightBuffer);

	D3D11_SHADER_RESOURCE_VIEW_DESC Srv_desc;
	ZeroMemory(&Srv_desc, sizeof(Srv_desc));
	Srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
	Srv_desc.BufferEx.FirstElement = 0;
	Srv_desc.BufferEx.NumElements = PointLightCount;
	Srv_desc.Format = DXGI_FORMAT_UNKNOWN;
	pd3dDevice->CreateShaderResourceView(PointLightBuffer, &Srv_desc, &PointLight_SRV);

	LightListHeader = new GStructuredBuffer(pd3dDevice, sizeof(int), TiledWidth*TiledHeight, DXGI_FORMAT_R32_SINT, true, false);
	LightList = new GStructuredBuffer(pd3dDevice, sizeof(int) * 2, TiledWidth*TiledHeight*PointLightCount, DXGI_FORMAT_UNKNOWN, true, true);

	LightPass.SetPSShader(L"Shaders\\TiledDeferredLighting.hlsl", "TiledDeferredLighting_PS");
	LightPass.Init(InContext); 

	D3D11_BLEND_DESC	blend_desc;
	ZeroMemory(&blend_desc, sizeof(D3D11_BLEND_DESC));
	blend_desc.RenderTarget[0].BlendEnable = TRUE;
	blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
	blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
	blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
	blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	pd3dDevice->CreateBlendState(&blend_desc, &BlendAddState);

	blend_desc.RenderTarget[0].BlendEnable = FALSE;
	pd3dDevice->CreateBlendState(&blend_desc, &BlendDisabledState);

}

void GTiledDeferredLighting::Render(UINT InSRVCount, ID3D11ShaderResourceView** InputSRV, ID3D11RenderTargetView* OutputRTV)
{
	// update light positions
	D3DXMATRIX RotY;
	D3DXMatrixRotationY(&RotY, 0.005f);

	for (UINT i = 0; i < PointLightCount; ++i)
	{
		D3DXVECTOR4 Pos_i = PointLightArray[i].Position;
		D3DXVECTOR3 CurPos(Pos_i.x, Pos_i.y, Pos_i.z);
		D3DXVECTOR3	NewPos;

		D3DXVec3TransformCoord(&NewPos, &CurPos, &RotY);
		PointLightArray[i].Position.x = NewPos.x;
		PointLightArray[i].Position.y = NewPos.y;
		PointLightArray[i].Position.z = NewPos.z;
	}

	D3D11_MAPPED_SUBRESOURCE ResourceData;

	pd3dDeviceContext->Map(PointLightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ResourceData);
	memcpy(ResourceData.pData, PointLightArray, PointLightCount * sizeof(PointLightInfo));
	pd3dDeviceContext->Unmap(PointLightBuffer, 0);

	ID3D11UnorderedAccessView* ppUAViews[2] = { LightListHeader->UAV, LightList->UAV};

	UINT	InitCounter[2] = { 0, 0 };
	pd3dDeviceContext->CSSetUnorderedAccessViews(0, 2, ppUAViews, InitCounter);
	pd3dDeviceContext->CSSetShaderResources(0, 1, &PointLight_SRV);
	pd3dDeviceContext->CSSetShader(LightListShader, nullptr, 0);

	pd3dDeviceContext->Dispatch(TiledWidth, TiledHeight, 1);

	pd3dDeviceContext->CSSetShader(NULL, NULL, 0);

	ID3D11UnorderedAccessView* ppUAViewNULL[3] = { NULL, NULL, NULL };
	pd3dDeviceContext->CSSetUnorderedAccessViews(0, 3, ppUAViewNULL, NULL);

	ID3D11ShaderResourceView* ppSRVNULL[1] = { NULL };
	pd3dDeviceContext->CSSetShaderResources(0, 1, ppSRVNULL);

	float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

	pd3dDeviceContext->OMSetBlendState(BlendAddState, blendFactor, 0xffffffff);

	pd3dDeviceContext->PSSetShaderResources(InSRVCount, 1, &PointLight_SRV);
	pd3dDeviceContext->PSSetShaderResources(InSRVCount+1, 1, &LightListHeader->SRV);
	pd3dDeviceContext->PSSetShaderResources(InSRVCount+2, 1, &LightList->SRV);
	LightPass.bClearRenderTarget = false;
	LightPass.Render(InSRVCount, InputSRV, OutputRTV);

	blendFactor[0] = blendFactor[1] = blendFactor[2] = blendFactor[3] = 0.0f;
	pd3dDeviceContext->OMSetBlendState(BlendDisabledState, blendFactor, 0xffffffff);
}

void GTiledDeferredLighting::Shutdown()
{
	SAFE_RELEASE(PointLight_SRV);
	SAFE_RELEASE(PointLightBuffer);

	SAFE_RELEASE(LightListShader);

	SAFE_DELETE(LightListHeader);

	SAFE_DELETE(LightList);

	SAFE_RELEASE(BlendAddState);
	SAFE_RELEASE(BlendDisabledState);

	delete[] PointLightArray;
	LightPass.Shutdown();
}