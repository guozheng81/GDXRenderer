#pragma once

#ifndef _LPV_H_
#define _LPV_H_

#include "RenderPass.h"
#include "PostProcess.h"

class GLPV : public GRenderPass
{
protected:
	UINT	VolumeX;
	UINT	VolumeY;
	UINT	VolumeZ;

	UINT				ShadowTextureSize = 1024;

public:
	GTextureDepthStencil*		RSM_Depth = nullptr;
	GTexture2D*					RSM_BufferA = nullptr;

	GTexture3D*			Vpl_Address = nullptr;
	GStructuredBuffer*	Vpl = nullptr;

	GTexture3D*			Gv_Address = nullptr;
	GStructuredBuffer*	Gv = nullptr;

	ID3D11ComputeShader*		AccumulateVplShader = NULL;
	ID3D11ComputeShader*		PropagateVplShader = NULL;
	ID3D11ComputeShader*		ResolveVplShader = NULL;
	ID3D11ComputeShader*		AccumulateGvShader = NULL;

	GStructuredBuffer*	Accum_Vpl = nullptr;
	GStructuredBuffer*	Propagate_Vpl = nullptr;
	GStructuredBuffer*	Accum_Gv = nullptr;

	GTexture3D*			Resolved_Vpl_R = nullptr;
	GTexture3D*			Resolved_Vpl_G = nullptr;
	GTexture3D*			Resolved_Vpl_B = nullptr;

	GLPV() {}
	~GLPV() {}

	virtual void Init(const GRenderContext& InContext) override;
	virtual void Render(UINT InSRVCount, ID3D11ShaderResourceView** InputSRV, ID3D11RenderTargetView* OutputRTV) override;
	virtual void Shutdown() override;

};

#endif