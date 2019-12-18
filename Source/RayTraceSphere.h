#pragma once

#ifndef _RAY_TRACE_SPHERE_
#define _RAY_TRACE_SPHERE_

#include "RenderPass.h"
#include "PostProcess.h"

struct HitableObject
{
	D3DXVECTOR4		Position;
	D3DXVECTOR4		Color;
	D3DXVECTOR4		MaterialParams;
};

struct Ray
{
	D3DXVECTOR3		Origin;
	D3DXVECTOR3		Direction;
	D3DXVECTOR3		DiffuseBRDF;
	D3DXVECTOR3		SpecularBRDF;
	UINT			SampleDiffuse;
	UINT			Depth;
	UINT			rngState;
	INT				Next;
};

class GRayTraceSphere : public GRenderPass
{
protected:
	ID3D11ComputeShader*	RayTraceShader = nullptr;
	ID3D11ComputeShader*	RayGenShader = nullptr;
	ID3D11ComputeShader*	RayBounceShader = nullptr;
	ID3D11ComputeShader*	RayColorCombineSimpleAccumShader = nullptr;
	ID3D11ComputeShader*	RayColorCombineShader = nullptr;

	ID3D11ComputeShader*	SVGFSetupShader = nullptr;
	ID3D11ComputeShader*	SVGFReprojectShader = nullptr;
	ID3D11ComputeShader*	SVGFMomentFilterShader = nullptr;
	ID3D11ComputeShader*	SVGFAtrous1Shader = nullptr;
	ID3D11ComputeShader*	SVGFAtrous2Shader = nullptr;
	ID3D11ComputeShader*	SVGFAtrous3Shader = nullptr;
	ID3D11ComputeShader*	SVGFAtrous4Shader = nullptr;
	ID3D11ComputeShader*	SVGFAtrous5Shader = nullptr;

	HitableObject*			HitableObjectList = nullptr;

	ID3D11Buffer*			HitableObjectsBuffer = nullptr;
	ID3D11ShaderResourceView*	HitableObjects_SRV = nullptr;

	GTexture2D*				History0 = nullptr;
	GTexture2D*				History1 = nullptr;

	bool				bIsUsingHistory0 = true;

	GStructuredBuffer*		RayHeader0 = nullptr;
	GStructuredBuffer*		RayBuffer0 = nullptr;

	GStructuredBuffer*		RayHeader1 = nullptr;
	GStructuredBuffer*		RayBuffer1 = nullptr;

	GStructuredBuffer*		ColorBuffer = nullptr;

	// used for SVGF filter
	GTexture2D*			NormalDepth0 = nullptr;
	GTexture2D*			NormalDepth1 = nullptr;

	GTexture2D*			DirectColor = nullptr;
	GTexture2D*			IndirectColor = nullptr;

	GTexture2D*			IndirectColor1 = nullptr;
	GTexture2D*			IndirectColor2 = nullptr;

	GTexture2D*			AtrousColor0 = nullptr;
	GTexture2D*			AtrousColor1 = nullptr;

	GStructuredBuffer*	Moments = nullptr;
	GStructuredBuffer*	HistoryCount = nullptr;

	bool bUseSimpleAccum = false;

public:
	GRayTraceSphere()
	{
	}

	~GRayTraceSphere()
	{
	}

	ID3D11ShaderResourceView*	GetCurrentSRV() {
		return (bIsUsingHistory0 ? History1->SRV : History0->SRV);
	}

	virtual void Init(const GRenderContext& InContext) override;
	virtual void Render(UINT InSRVCount, ID3D11ShaderResourceView** InputSRV, ID3D11RenderTargetView* OutputRTV) override;
	virtual void Shutdown() override;
};

#endif
