#pragma once

#ifndef _HISTOGRAM_
#define _HISTOGRAM_

#include "RenderPass.h"

class GHistogram : public GRenderPass
{

	UINT						BinNum = 64;

	GStructuredBuffer*			Histogram = nullptr;

	ID3D11ComputeShader*		HistogramShader = nullptr;
	ID3D11ComputeShader*		AverageLuminanceShader = nullptr;

public:
	GHistogram()
	{
	}

	~GHistogram()
	{
	}

	GStructuredBuffer*			AverageLuminance = nullptr;

	virtual void Init(const GRenderContext& InContext) override;
	virtual void Render(UINT InSRVCount, ID3D11ShaderResourceView** InputSRV, ID3D11RenderTargetView* OutputRTV) override;
	virtual void Shutdown() override;
};

#endif
