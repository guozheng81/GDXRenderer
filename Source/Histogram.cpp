#include "DXUT.h"
#include "SDKmisc.h"

#include "RenderUtils.h"

#include "Histogram.h"

void GHistogram::Init(const GRenderContext& InContext)
{
	GRenderPass::Init(InContext);

	Histogram = new GStructuredBuffer(pd3dDevice, sizeof(int), BinNum);
	AverageLuminance = new GStructuredBuffer(pd3dDevice, sizeof(float), 1);

	ID3DBlob* ShaderBlob = nullptr;
	CompileShaderFromFile(L"Shaders\\PostProcessing.hlsl", NULL, "HistogramCS", "cs_5_0", &ShaderBlob);
	pd3dDevice->CreateComputeShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &HistogramShader);
	SAFE_RELEASE(ShaderBlob);

	CompileShaderFromFile(L"Shaders\\PostProcessing.hlsl", NULL, "AverageLuminance", "cs_5_0", &ShaderBlob);
	pd3dDevice->CreateComputeShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &AverageLuminanceShader);
	SAFE_RELEASE(ShaderBlob);

}

void GHistogram::Render(UINT InSRVCount, ID3D11ShaderResourceView** InputSRV, ID3D11RenderTargetView* OutputRTV)
{
	UINT	ClearValue[4] = { 0, 0, 0, 0 };
	pd3dDeviceContext->ClearUnorderedAccessViewUint(Histogram->UAV, ClearValue);
	pd3dDeviceContext->CSSetShader(HistogramShader, NULL, 0);
	pd3dDeviceContext->CSSetShaderResources(0, InSRVCount, InputSRV);
	pd3dDeviceContext->CSSetUnorderedAccessViews(0, 1, &Histogram->UAV, nullptr);

	pd3dDeviceContext->Dispatch(ViewportWidth / 16, ViewportHeight / 16, 1);

	pd3dDeviceContext->CSSetShader(AverageLuminanceShader, NULL, 0);
	pd3dDeviceContext->CSSetUnorderedAccessViews(0, 1, &AverageLuminance->UAV, nullptr);
	pd3dDeviceContext->CSSetShaderResources(0, 1, &Histogram->SRV);
	pd3dDeviceContext->Dispatch(1, 1, 1);

	pd3dDeviceContext->CSSetShader(NULL, NULL, 0);

	ID3D11UnorderedAccessView* ppUAViewNULL[1] = { NULL };
	pd3dDeviceContext->CSSetUnorderedAccessViews(0, 1, ppUAViewNULL, NULL);

	ID3D11ShaderResourceView* ppSRVNULL[1] = { NULL };
	pd3dDeviceContext->CSSetShaderResources(0, 1, ppSRVNULL);

}

void GHistogram::Shutdown()
{
	SAFE_DELETE(Histogram);

	SAFE_DELETE(AverageLuminance);

	SAFE_RELEASE(HistogramShader);
	SAFE_RELEASE(AverageLuminanceShader);
}