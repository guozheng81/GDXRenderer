#include "DXUT.h"
#include "SDKmisc.h"
#include "PostProcess.h"
#include "MotionBlur.h"
#include "RenderUtils.h"

void GMotionBlur::Init(const GRenderContext& InContext)
{
	GRenderPass::Init(InContext);

	ID3DBlob* ShaderBlob = nullptr;

	CompileShaderFromFile(L"Shaders\\MotionBlur.hlsl", NULL, "MaxVelocity", "cs_5_0", &ShaderBlob);
	pd3dDevice->CreateComputeShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &MaxVelocityShader);
	SAFE_RELEASE(ShaderBlob);

	CompileShaderFromFile(L"Shaders\\MotionBlur.hlsl", NULL, "NeighborMaxVelocity", "cs_5_0", &ShaderBlob);
	pd3dDevice->CreateComputeShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &NeighborMaxVelocityShader);
	SAFE_RELEASE(ShaderBlob);

	MaxVelocity_Width = ceil((float)ViewportWidth / TileSize);
	MaxVelocity_Height = ceil((float)ViewportHeight / TileSize);

	MaxVelocity = new GTexture2D(pd3dDevice, MaxVelocity_Width, MaxVelocity_Height, DXGI_FORMAT_R32G32_FLOAT, false, true);
	NeighborMaxVelocity = new GTexture2D(pd3dDevice, MaxVelocity_Width, MaxVelocity_Height, DXGI_FORMAT_R32G32_FLOAT, false, true);

	BlurPass.SetPSShader(L"Shaders\\MotionBlur.hlsl", "MotionBlurFilter_PS");
	BlurPass.Init(InContext);

}

void GMotionBlur::Render(UINT InSRVCount, ID3D11ShaderResourceView** InputSRV, ID3D11RenderTargetView* OutputRTV)
{
	ID3D11RenderTargetView* pRTVs[16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
	pd3dDeviceContext->OMSetRenderTargets(8, pRTVs, NULL);

	pd3dDeviceContext->CSSetShader(MaxVelocityShader, NULL, 0);

	pd3dDeviceContext->CSSetUnorderedAccessViews(0, 1, &MaxVelocity->UAV, nullptr);
	pd3dDeviceContext->CSSetShaderResources(0, 1, &(InputSRV[1]));

	pd3dDeviceContext->Dispatch(MaxVelocity_Width, MaxVelocity_Height, 1);

	pd3dDeviceContext->CSSetShader(NeighborMaxVelocityShader, NULL, 0);

	pd3dDeviceContext->CSSetUnorderedAccessViews(0, 1, &NeighborMaxVelocity->UAV, nullptr);
	pd3dDeviceContext->CSSetShaderResources(0, 1, &MaxVelocity->SRV);

	pd3dDeviceContext->Dispatch( (UINT)(ceil((float)MaxVelocity_Width/14.0f)), (UINT)(ceil((float)MaxVelocity_Height/14.0f)), 1);

	pd3dDeviceContext->CSSetShader(NULL, NULL, 0);

	ID3D11UnorderedAccessView* ppUAViewNULL[1] = { NULL };
	pd3dDeviceContext->CSSetUnorderedAccessViews(0, 1, ppUAViewNULL, NULL);

	ID3D11ShaderResourceView* ppSRVNULL[1] = { NULL };
	pd3dDeviceContext->CSSetShaderResources(0, 1, ppSRVNULL);

	ID3D11ShaderResourceView*	MotionBlurInput[4] = { InputSRV[0], InputSRV[1], InputSRV[2], NeighborMaxVelocity->SRV };
	BlurPass.Render(4, MotionBlurInput, OutputRTV);
}

void GMotionBlur::Shutdown()
{
	SAFE_RELEASE(MaxVelocityShader);
	SAFE_RELEASE(NeighborMaxVelocityShader);

	SAFE_DELETE(MaxVelocity);

	SAFE_DELETE(NeighborMaxVelocity);

	BlurPass.Shutdown();
}