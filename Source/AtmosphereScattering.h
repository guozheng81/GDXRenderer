#pragma once

#ifndef _ATMOSPHERE_SCATTERING_
#define _ATMOSPHERE_SCATTERING_

#include "RenderPass.h"
#include "PostProcess.h"

struct CB_AS
{
	D3DXVECTOR4	CurrentFramePixel;
};

class GAtmosphereScattering : public GRenderPass
{
protected:
	UINT	VolumeX = 320;
	UINT	VolumeY = 180;
	UINT	VolumeZ = 256;

	GTexture3D*			LightScattering = nullptr;
//	GTexture3D*			LightIntegration = nullptr;

//	GTexture3D*			LightIntegrationHistory0 = nullptr;
//	GTexture3D*			LightIntegrationHistory1 = nullptr;

	ID3D11ComputeShader*	LightScatteringShader = nullptr;
	ID3D11ComputeShader*	LightIntegrationShader = nullptr;
	ID3D11ComputeShader*	LightIntegrationTemporalShader = nullptr;

	GPostProcess	ApplyLightScatteringPass;

	ID3D11BlendState*		BlendAddState;
	ID3D11BlendState*		BlendDisabledState;

	bool				bIsUsingHistory0 = true;

	UINT				JitterSampleIdx = 0;
	UINT				JitterSampleNum = 4;
	float				JitterSamples[4] = { 0.0f, 0.5f, 0.75f, 0.25f };

/*	float				JitterSamples[16]= { 0.0f, 0.5f, 0.125f, 0.625f ,
											 0.75f, 0.25f, 0.875f, 0.375f,
											 0.1875f, 0.6875f, 0.0625f, 0.5625 ,
											 0.9375f, 0.4375f, 0.8125f, 0.3125  };
*/
	ID3D11Buffer*               ConstBuffer = NULL;

	ID3D11ShaderResourceView*	NoisePerm_SRV;
	ID3D11ShaderResourceView*	NoisePerm2D_SRV;
	ID3D11ShaderResourceView*	NoiseGrad_SRV;
	ID3D11ShaderResourceView*	NoisePermGrad_SRV;

	ID3D11ShaderResourceView*	CloudLowFreqNoise_SRV;
	ID3D11ShaderResourceView*	CloudHighFreqNoise_SRV;
	ID3D11ShaderResourceView*	CloudWeather_SRV;
	ID3D11ShaderResourceView*	CloudCurlNoise_SRV;

public:
	GAtmosphereScattering()
	{
	}

	~GAtmosphereScattering()
	{
	}

	virtual void Init(const GRenderContext& InContext) override;
	virtual void Render(UINT InSRVCount, ID3D11ShaderResourceView** InputSRV, ID3D11RenderTargetView* OutputRTV) override;
	virtual void Shutdown() override;

};

#endif

