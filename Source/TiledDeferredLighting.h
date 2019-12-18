#pragma once

#ifndef _TILED_DEFERRED_LIHGTING_H_
#define _TILED_DEFERRED_LIHGTING_H_

#include "RenderPass.h"
#include "PostProcess.h"

struct PointLightInfo
{
	D3DXVECTOR4		Position;
	D3DXVECTOR4		Color;
};

class GTiledDeferredLighting : public GRenderPass
{
protected:
	UINT	TileSize = 20;

	UINT						TiledWidth = 64;
	UINT						TiledHeight = 36;

	UINT	PointLightCount = 600;

	PointLightInfo*		PointLightArray = nullptr;

	ID3D11Buffer*			PointLightBuffer = nullptr;
	ID3D11ShaderResourceView*		PointLight_SRV = nullptr;

	GStructuredBuffer*		LightListHeader = nullptr;
	GStructuredBuffer*		LightList = nullptr;

	ID3D11ComputeShader*	LightListShader = nullptr;

	class GPostProcess		LightPass;

	ID3D11BlendState*		BlendAddState;
	ID3D11BlendState*		BlendDisabledState;

public:
	GTiledDeferredLighting();
	~GTiledDeferredLighting();

	virtual void Init(const GRenderContext& InContext) override;
	virtual void Render(UINT InSRVCount, ID3D11ShaderResourceView** InputSRV, ID3D11RenderTargetView* OutputRTV) override;
	virtual void Shutdown() override;

};

#endif