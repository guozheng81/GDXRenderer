#include "DXUT.h"
#include "SDKmisc.h"
#include "PostProcess.h"
#include "RenderUtils.h"
#include "DOF.h"

void GDOF::Init(const GRenderContext& InContext)
{
	GRenderPass::Init(InContext);

	ID3DBlob* ShaderBlob = nullptr;

	CompileShaderFromFile(L"Shaders\\PostProcessing.hlsl", NULL, "Bokeh_VS", "vs_5_0", &ShaderBlob);
	pd3dDevice->CreateVertexShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &BokehVertexShader);
	SAFE_RELEASE(ShaderBlob);

	COC = new GTexture2D(pd3dDevice, ViewportWidth / 2, ViewportHeight / 2, DXGI_FORMAT_R16G16B16A16_FLOAT, true, false);
	Far = new GTexture2D(pd3dDevice, ViewportWidth / 2, ViewportHeight / 2, DXGI_FORMAT_R16G16B16A16_FLOAT, true, false);
	Near = new GTexture2D(pd3dDevice, ViewportWidth / 2, ViewportHeight / 2, DXGI_FORMAT_R16G16B16A16_FLOAT, true, false);

	COCPass.SetPSShader(L"Shaders\\PostProcessing.hlsl", "COC_PS");
	COCPass.Init(InContext);

	BokehPass.SetPSShader(L"Shaders\\PostProcessing.hlsl", "Bokeh_PS");
	BokehPass.Init(InContext);

	CombinePass.SetPSShader(L"Shaders\\PostProcessing.hlsl", "DOF_PS");
	CombinePass.Init(InContext);

	Main_VP.Width = (float)ViewportWidth;
	Main_VP.Height = (float)ViewportHeight;
	Main_VP.MaxDepth = 1.0f;
	Main_VP.MinDepth = 0.0f;
	Main_VP.TopLeftX = 0.0f;
	Main_VP.TopLeftY = 0.0f;

	Half_VP.Width = (float)(ViewportWidth/2);
	Half_VP.Height = (float)(ViewportHeight/2);
	Half_VP.MaxDepth = 1.0f;
	Half_VP.MinDepth = 0.0f;
	Half_VP.TopLeftX = 0.0f;
	Half_VP.TopLeftY = 0.0f;

	D3D11_BLEND_DESC	blend_desc;
	ZeroMemory(&blend_desc, sizeof(D3D11_BLEND_DESC));
	blend_desc.RenderTarget[0].BlendEnable = TRUE;
	blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
	blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
	blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	blend_desc.RenderTarget[1].BlendEnable = TRUE;
	blend_desc.RenderTarget[1].DestBlend = D3D11_BLEND_ONE;
	blend_desc.RenderTarget[1].SrcBlend = D3D11_BLEND_ONE;
	blend_desc.RenderTarget[1].BlendOp = D3D11_BLEND_OP_ADD;
	blend_desc.RenderTarget[1].DestBlendAlpha = D3D11_BLEND_ONE;
	blend_desc.RenderTarget[1].SrcBlendAlpha = D3D11_BLEND_ONE;
	blend_desc.RenderTarget[1].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blend_desc.RenderTarget[1].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	pd3dDevice->CreateBlendState(&blend_desc, &BlendAddState);

}

void GDOF::Render(UINT InSRVCount, ID3D11ShaderResourceView** InputSRV, ID3D11RenderTargetView* OutputRTV)
{
	pd3dDeviceContext->RSSetViewports(1, &Half_VP);

	COCPass.Render( InSRVCount, InputSRV, COC->RTV);

	float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	pd3dDeviceContext->ClearRenderTargetView(Far->RTV, ClearColor);
	pd3dDeviceContext->ClearRenderTargetView(Near->RTV, ClearColor);

	ID3D11RenderTargetView*	RTVs[2] = { Far->RTV, Near->RTV };
	pd3dDeviceContext->OMSetRenderTargets(2, RTVs, nullptr);

	float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

	pd3dDeviceContext->OMSetBlendState(BlendAddState, blendFactor, 0xffffffff);

	pd3dDeviceContext->VSSetShader(BokehVertexShader, nullptr, 0);
	pd3dDeviceContext->VSSetShaderResources(0, 1, &COC->SRV);

	BokehPass.bClearRenderTarget = false;
	BokehPass.bUseDefaultVS = false;
	BokehPass.InstanceNum = ViewportWidth*ViewportHeight / 4;

	BokehPass.Render( 0, nullptr, nullptr);

	ID3D11ShaderResourceView*	SRVs[1] = { NULL };
	pd3dDeviceContext->VSSetShaderResources(0, 1, SRVs);

	RTVs[0] = nullptr;
	RTVs[1] = nullptr;
	pd3dDeviceContext->OMSetRenderTargets(2, RTVs, nullptr);

	pd3dDeviceContext->RSSetViewports(1, &Main_VP);

	blendFactor[0] = blendFactor[1] = blendFactor[2] = blendFactor[3] = 0.0f;
	pd3dDeviceContext->OMSetBlendState(NULL, blendFactor, 0xffffffff);

	ID3D11ShaderResourceView*	CombineInput[4] = { InputSRV[0], COC->SRV, Far->SRV, Near->SRV };
	CombinePass.Render( 4, CombineInput, OutputRTV);
}

void GDOF::Shutdown()
{
	SAFE_RELEASE(BokehVertexShader);

	SAFE_DELETE(COC);
	SAFE_DELETE(Far);
	SAFE_DELETE(Near);

	SAFE_RELEASE(BlendAddState);

	COCPass.Shutdown();
	BokehPass.Shutdown();
	CombinePass.Shutdown();
}
