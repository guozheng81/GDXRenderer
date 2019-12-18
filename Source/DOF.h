#pragma once

#ifndef _DOF_
#define _DOF_

#include "RenderPass.h"
#include "PostProcess.h"

class GDOF : public GRenderPass
{
protected:

	ID3D11VertexShader*			BokehVertexShader = nullptr;

	GTexture2D*					COC = nullptr;
	GTexture2D*					Far = nullptr;
	GTexture2D*					Near = nullptr;

	GPostProcess				COCPass;
	GPostProcess				BokehPass;
	GPostProcess				CombinePass;

	D3D11_VIEWPORT				Main_VP;
	D3D11_VIEWPORT				Half_VP;

	ID3D11BlendState*		BlendAddState;

public:
	GDOF()
	{
	}

	~GDOF()
	{
	}

	virtual void Init(const GRenderContext& InContext) override;
	virtual void Render(UINT InSRVCount, ID3D11ShaderResourceView** InputSRV, ID3D11RenderTargetView* OutputRTV) override;
	virtual void Shutdown() override;

};

#endif