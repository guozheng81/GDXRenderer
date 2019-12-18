#pragma once

#ifndef _MOTION_BLUR_H_
#define _MOTION_BLUR_H_

#include "RenderPass.h"
#include "PostProcess.h"

//https ://casual-effects.com/research/McGuire2012Blur/McGuire12Blur.pdf

class GMotionBlur : public GRenderPass
{
protected:
	class GPostProcess	BlurPass;

	UINT	TileSize = 10;

	ID3D11ComputeShader*	MaxVelocityShader = nullptr;
	ID3D11ComputeShader*	NeighborMaxVelocityShader = nullptr;

	GTexture2D*					MaxVelocity = nullptr;
	GTexture2D*					NeighborMaxVelocity = nullptr;

	UINT						MaxVelocity_Width;
	UINT						MaxVelocity_Height;

public:
	GMotionBlur()	{}
	~GMotionBlur()	{}

	virtual void Init(const GRenderContext& InContext) override;
	virtual void Render(UINT InSRVCount, ID3D11ShaderResourceView** InputSRV, ID3D11RenderTargetView* OutputRTV) override;
	virtual void Shutdown() override;

};

#endif