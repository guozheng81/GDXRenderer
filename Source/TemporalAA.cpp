#include "DXUT.h"
#include "SDKmisc.h"
#include "PostProcess.h"
#include "RenderUtils.h"
#include "TemporalAA.h"

void GTemporalAA::Init(const GRenderContext& InContext)
{
	GRenderPass::Init(InContext);

	History0 = new GTexture2D(pd3dDevice, ViewportWidth, ViewportHeight, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, true, false);
	History1 = new GTexture2D(pd3dDevice, ViewportWidth, ViewportHeight, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, true, false);

	TemporalAAPass.SetPSShader(L"Shaders\\TemporalAA.hlsl", "TemporalAAPS");
	TemporalAAPass.Init(InContext);
	CopyPass.SetPSShader(L"Shaders\\PostProcessing.hlsl", "PSMain");
	CopyPass.Init(InContext);

	JitterSampleIdx = 0;
	GenerateJitterSamples();
}

D3DXVECTOR4	GTemporalAA::GetCurrentJitterSample()
{
	D3DXVECTOR4	CombinedSamples;
	CombinedSamples.x = JitterSamples[JitterSampleIdx].x;
	CombinedSamples.y = JitterSamples[JitterSampleIdx].y;
	UINT	PreSampleIdx = (JitterSampleIdx - 1) % JitterSampleNum;
	CombinedSamples.z = JitterSamples[PreSampleIdx].x;
	CombinedSamples.w = JitterSamples[PreSampleIdx].y;

	return CombinedSamples;
}

float	GTemporalAA::HaltonSeq(int prime, int index)
{
	float r = 0.0f;
	float f = 1.0f;
	int i = index;
	while (i > 0)
	{
		f /= prime;
		r += f * (i % prime);
		i = (int)floor(i / (float)prime);
	}
	return r;
}

void	GTemporalAA::GenerateJitterSamples()
{
	for (int i = 0; i < JitterSampleNum; ++i)
	{
		float u = HaltonSeq(2, i + 1) - 0.5f;
		float v = HaltonSeq(3, i + 1) - 0.5f;
		JitterSamples[i] = D3DXVECTOR2(u, v);
	}
}

void GTemporalAA::Render(UINT InSRVCount, ID3D11ShaderResourceView** InputSRV, ID3D11RenderTargetView* OutputRTV)
{
	pd3dDeviceContext->PSSetShaderResources(2, 1, bIsUsingHistory0 ? &History1->SRV : &History0->SRV);
	TemporalAAPass.Render(InSRVCount, InputSRV, bIsUsingHistory0 ? History0->RTV : History1->RTV);

	CopyPass.Render(1, (bIsUsingHistory0 ? &History0->SRV : &History1->SRV), OutputRTV);

	bIsUsingHistory0 = (!bIsUsingHistory0);

	JitterSampleIdx = (JitterSampleIdx + 1) % JitterSampleNum;
}

void GTemporalAA::Shutdown()
{
	SAFE_DELETE(History0);
	SAFE_DELETE(History1);

	TemporalAAPass.Shutdown();
	CopyPass.Shutdown();
}