#pragma once
//https://learnopengl.com/PBR/IBL/Specular-IBL

#ifndef _SKY_LIGHT_H_
#define _SKY_LIGHT_H_

#include "RenderPass.h"

struct CB_SKYLIGHT
{
	D3DXVECTOR4	SkyCubeSize;
	D3DXVECTOR4	MaterialParams;
};

class GSkyLight : public GRenderPass
{
protected:
	ID3D11ComputeShader*	SHConvertShader = nullptr;

	GStructuredBuffer*		Diffuse1D = nullptr;

	ID3D11ComputeShader*	SHCombineShader = nullptr;

	ID3D11ComputeShader*	SpecularFilterShader = nullptr;
	ID3D11Texture2D*		SpecularCubeMap = nullptr;
	ID3D11UnorderedAccessView*		Specular_UAV[5] = { nullptr };

	ID3D11ComputeShader*	SpecularIntegrationShader = nullptr;

	bool	bHasRendered = false;

	UINT	CubeMapSize = 256;
	UINT	GroupThreadSize = 16;
	UINT	GroupCount = 16;

	ID3D11Buffer*               ConstBuffer = NULL;

	const UINT SpecularMaxRoughnessLevel = 5;

	void	SetConstBuffer(float InSize, float InRoughness);

public:
	GSkyLight();
	~GSkyLight();

	GStructuredBuffer*		Diffuse = nullptr;
	GTexture2D*				SpecularIntegration = nullptr;

	ID3D11ShaderResourceView*		Specular_SRV = nullptr;

	virtual void Init(const GRenderContext& InContext) override;
	virtual void Render(UINT InSRVCount, ID3D11ShaderResourceView** InputSRV, ID3D11RenderTargetView* OutputRTV) override;
	virtual void Shutdown() override;
};

#endif