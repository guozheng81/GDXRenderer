#pragma once

#ifndef _TEMPORAL_AA_
#define _TEMPORAL_AA_

#include "RenderPass.h"
#include "PostProcess.h"

class GTemporalAA : public GRenderPass
{
protected:
	GTexture2D*			History0 = nullptr;
	GTexture2D*			History1 = nullptr;

	GPostProcess		TemporalAAPass;
	GPostProcess		CopyPass;

	bool				bIsUsingHistory0 = true;

	UINT				JitterSampleIdx = 0;
	UINT				JitterSampleNum = 16;
	D3DXVECTOR2			JitterSamples[16];

	// https://github.com/playdeadgames/temporal
	// http://en.wikipedia.org/wiki/Halton_sequence
	float		HaltonSeq(int prime, int index = 1);
	void		GenerateJitterSamples();

public:
	GTemporalAA()
	{

	}

	~GTemporalAA()
	{

	}

	D3DXVECTOR4	GetCurrentJitterSample();

	virtual void Init(const GRenderContext& InContext) override;
	virtual void Render(UINT InSRVCount, ID3D11ShaderResourceView** InputSRV, ID3D11RenderTargetView* OutputRTV) override;
	virtual void Shutdown() override;

};

#endif