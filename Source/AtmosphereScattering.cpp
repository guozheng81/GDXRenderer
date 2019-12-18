#include "DXUT.h"
#include "SDKmisc.h"
#include "PostProcess.h"
#include "RenderUtils.h"
#include "AtmosphereScattering.h"

void GAtmosphereScattering::Init(const GRenderContext& InContext)
{
	GRenderPass::Init(InContext);

	LightScattering = new GTexture3D(pd3dDevice, VolumeX, VolumeY, VolumeZ, DXGI_FORMAT_R16G16B16A16_FLOAT, true);
//	LightIntegration = new GTexture3D(pd3dDevice, VolumeX, VolumeY, VolumeZ, DXGI_FORMAT_R16G16B16A16_FLOAT, true);

//	LightIntegrationHistory0 = new GTexture3D(pd3dDevice, VolumeX, VolumeY, VolumeZ, DXGI_FORMAT_R16G16B16A16_FLOAT, true);
//	LightIntegrationHistory1 = new GTexture3D(pd3dDevice, VolumeX, VolumeY, VolumeZ, DXGI_FORMAT_R16G16B16A16_FLOAT, true);

	ID3DBlob* ShaderBlob = nullptr;

	CompileShaderFromFile(L"Shaders\\AtmosphereScattering.hlsl", NULL, "LightScattering", "cs_5_0", &ShaderBlob);
	pd3dDevice->CreateComputeShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &LightScatteringShader);
	SAFE_RELEASE(ShaderBlob);

	CompileShaderFromFile(L"Shaders\\AtmosphereScattering.hlsl", NULL, "LightIntegration", "cs_5_0", &ShaderBlob);
	pd3dDevice->CreateComputeShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &LightIntegrationShader);
	SAFE_RELEASE(ShaderBlob);

	CompileShaderFromFile(L"Shaders\\AtmosphereScattering.hlsl", NULL, "LightIntegrationTemporal", "cs_5_0", &ShaderBlob);
	pd3dDevice->CreateComputeShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &LightIntegrationTemporalShader);
	SAFE_RELEASE(ShaderBlob);

	ApplyLightScatteringPass.SetPSShader(L"Shaders\\AtmosphereScattering.hlsl", "ApplyScatteringPs");
	ApplyLightScatteringPass.Init(InContext);

	D3D11_BUFFER_DESC const_desc;
	const_desc.Usage = D3D11_USAGE_DYNAMIC;
	const_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	const_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	const_desc.MiscFlags = 0;

	const_desc.ByteWidth = sizeof(CB_AS);
	pd3dDevice->CreateBuffer(&const_desc, NULL, &ConstBuffer);

	D3D11_BLEND_DESC	blend_desc;
	ZeroMemory(&blend_desc, sizeof(D3D11_BLEND_DESC));
	blend_desc.RenderTarget[0].BlendEnable = TRUE;
	blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_SRC_ALPHA;
	blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
	blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
	blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	pd3dDevice->CreateBlendState(&blend_desc, &BlendAddState);

	blend_desc.RenderTarget[0].BlendEnable = FALSE;
	pd3dDevice->CreateBlendState(&blend_desc, &BlendDisabledState);

	WCHAR noise_path[1000];
	DXUTFindDXSDKMediaFileCch(noise_path, MAX_PATH, L"Noise\\perlin_noise_perm_texture.dds");
	DXUTGetGlobalResourceCache().CreateTextureFromFile(pd3dDevice, pd3dDeviceContext, noise_path, &NoisePerm_SRV);

	DXUTFindDXSDKMediaFileCch(noise_path, MAX_PATH, L"Noise\\perlin_noise_perm_texture2d.dds");
	DXUTGetGlobalResourceCache().CreateTextureFromFile(pd3dDevice, pd3dDeviceContext, noise_path, &NoisePerm2D_SRV);

	DXUTFindDXSDKMediaFileCch(noise_path, MAX_PATH, L"Noise\\perlin_noise_grad_texture.dds");
	DXUTGetGlobalResourceCache().CreateTextureFromFile(pd3dDevice, pd3dDeviceContext, noise_path, &NoiseGrad_SRV);

	DXUTFindDXSDKMediaFileCch(noise_path, MAX_PATH, L"Noise\\perlin_noise_perm_grad_texture.dds");
	DXUTGetGlobalResourceCache().CreateTextureFromFile(pd3dDevice, pd3dDeviceContext, noise_path, &NoisePermGrad_SRV);

	DXUTFindDXSDKMediaFileCch(noise_path, MAX_PATH, L"Noise\\LowFreq.dds");
	DXUTGetGlobalResourceCache().CreateTextureFromFile(pd3dDevice, pd3dDeviceContext, noise_path, &CloudLowFreqNoise_SRV);

	DXUTFindDXSDKMediaFileCch(noise_path, MAX_PATH, L"Noise\\HighFreq.dds");
	DXUTGetGlobalResourceCache().CreateTextureFromFile(pd3dDevice, pd3dDeviceContext, noise_path, &CloudHighFreqNoise_SRV);

	DXUTFindDXSDKMediaFileCch(noise_path, MAX_PATH, L"Noise\\weatherMap.png");
	DXUTGetGlobalResourceCache().CreateTextureFromFile(pd3dDevice, pd3dDeviceContext, noise_path, &CloudWeather_SRV);

	DXUTFindDXSDKMediaFileCch(noise_path, MAX_PATH, L"Noise\\curlNoise.png");
	DXUTGetGlobalResourceCache().CreateTextureFromFile(pd3dDevice, pd3dDeviceContext, noise_path, &CloudCurlNoise_SRV);

}

void GAtmosphereScattering::Render(UINT InSRVCount, ID3D11ShaderResourceView** InputSRV, ID3D11RenderTargetView* OutputRTV)
{
	UINT	PixelX = 0;
	UINT	PixelY = 0;
	for (UINT i = 0; i < JitterSampleNum; i++)
	{
		if ((UINT)(JitterSamples[i] * JitterSampleNum) == JitterSampleIdx)
		{
			PixelX = i % 2;
			PixelY = i / 2;
		}
	}

	D3D11_MAPPED_SUBRESOURCE MappedResource;
	pd3dDeviceContext->Map(ConstBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	CB_AS* pPerFrame = (CB_AS*)MappedResource.pData;
	pPerFrame->CurrentFramePixel = D3DXVECTOR4(JitterSampleIdx, PixelX, PixelY, 0);
	pd3dDeviceContext->Unmap(ConstBuffer, 0);
	pd3dDeviceContext->CSSetConstantBuffers(2, 1, &ConstBuffer);

	int	reprojection_sample_width = 1;//2

	pd3dDeviceContext->CSSetUnorderedAccessViews(0, 1, &LightScattering->UAV, nullptr);
	pd3dDeviceContext->CSSetShaderResources(0, 1, InputSRV);
	ID3D11ShaderResourceView* InputNoise[8] = { NoisePerm_SRV, NoisePerm2D_SRV, NoiseGrad_SRV, NoisePermGrad_SRV,
		CloudLowFreqNoise_SRV, CloudHighFreqNoise_SRV, CloudWeather_SRV, CloudCurlNoise_SRV};

	pd3dDeviceContext->CSSetShaderResources(3, 8, InputNoise);

	pd3dDeviceContext->CSSetShader(LightScatteringShader, nullptr, 0);

	pd3dDeviceContext->Dispatch((UINT)(ceil((float)VolumeX/8.0f/ reprojection_sample_width)), (UINT)(ceil(float(VolumeY)/8.0f/ reprojection_sample_width)), VolumeZ);

	ID3D11ShaderResourceView*	Input[2] = { LightScattering->SRV, InputSRV[1] };

	/*
	// use integration texture
	pd3dDeviceContext->CSSetUnorderedAccessViews(0, 1, &LightIntegration->UAV, nullptr);
	pd3dDeviceContext->CSSetShaderResources(0, 2, Input);
	pd3dDeviceContext->CSSetShader(LightIntegrationShader, nullptr, 0);
	pd3dDeviceContext->Dispatch((UINT)(ceil((float)VolumeX/8.0f/ reprojection_sample_width)), (UINT)(ceil(float(VolumeY)/8.0f/ reprojection_sample_width)), 1);
	Input[0] = LightIntegration->SRV;

	if (reprojection_sample_width > 1)
	{
		// temporal accumulate
		pd3dDeviceContext->CSSetUnorderedAccessViews(0, 1, bIsUsingHistory0 ? &LightIntegrationHistory0->UAV : &LightIntegrationHistory1->UAV, nullptr);
		pd3dDeviceContext->CSSetShaderResources(0, 1, &LightIntegration->SRV);
		pd3dDeviceContext->CSSetShaderResources(1, 1, bIsUsingHistory0 ? &LightIntegrationHistory1->SRV : &LightIntegrationHistory0->SRV);
		pd3dDeviceContext->CSSetShader(LightIntegrationTemporalShader, nullptr, 0);
		pd3dDeviceContext->Dispatch((UINT)(ceil((float)VolumeX / 8.0f)), (UINT)(ceil(float(VolumeY) / 8.0f)), VolumeZ);
		Input[0] = (bIsUsingHistory0 ? LightIntegrationHistory0->SRV : LightIntegrationHistory1->SRV);
	}
	*/

	pd3dDeviceContext->CSSetShader(NULL, NULL, 0);

	ID3D11UnorderedAccessView* ppUAViewNULL[3] = { NULL, NULL, NULL };
	pd3dDeviceContext->CSSetUnorderedAccessViews(0, 3, ppUAViewNULL, NULL);

	ID3D11ShaderResourceView* ppSRVNULL[8] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
	pd3dDeviceContext->CSSetShaderResources(0, 8, ppSRVNULL);

	ID3D11Buffer* ppCBNULL[1] = { NULL };
	pd3dDeviceContext->CSSetConstantBuffers(2, 1, ppCBNULL);

	float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

	pd3dDeviceContext->OMSetBlendState(BlendAddState, blendFactor, 0xffffffff);

	ApplyLightScatteringPass.bClearRenderTarget = false;
	ApplyLightScatteringPass.Render(2, Input, OutputRTV);

	blendFactor[0] = blendFactor[1] = blendFactor[2] = blendFactor[3] = 0.0f;
	pd3dDeviceContext->OMSetBlendState(BlendDisabledState, blendFactor, 0xffffffff);

	ID3D11RenderTargetView* pRTVs[16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
	pd3dDeviceContext->OMSetRenderTargets(8, pRTVs, NULL);

	JitterSampleIdx = (JitterSampleIdx + 1) % JitterSampleNum;
	bIsUsingHistory0 = (!bIsUsingHistory0);
}

void GAtmosphereScattering::Shutdown()
{
	SAFE_DELETE(LightScattering);
//	SAFE_DELETE(LightIntegration);

//	SAFE_DELETE(LightIntegrationHistory0);
//	SAFE_DELETE(LightIntegrationHistory1);

	SAFE_RELEASE(LightScatteringShader);
	SAFE_RELEASE(LightIntegrationShader);

	ApplyLightScatteringPass.Shutdown();

	SAFE_RELEASE(ConstBuffer);
	SAFE_RELEASE(LightIntegrationTemporalShader);

	SAFE_RELEASE(BlendAddState);
	SAFE_RELEASE(BlendDisabledState);

	SAFE_RELEASE(NoisePerm_SRV);
	SAFE_RELEASE(NoisePerm2D_SRV);
	SAFE_RELEASE(NoiseGrad_SRV);
	SAFE_RELEASE(NoisePermGrad_SRV);

	SAFE_RELEASE(CloudLowFreqNoise_SRV);
	SAFE_RELEASE(CloudHighFreqNoise_SRV);
	SAFE_RELEASE(CloudWeather_SRV);
	SAFE_RELEASE(CloudCurlNoise_SRV);

}
