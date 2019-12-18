#pragma once

#ifndef _VOXEL_CONE_TRACER_H_
#define _VOXEL_CONE_TRACER_H_

#include "RenderPass.h"
#include "PostProcess.h"

struct CB_VOXELIZATION
{
	D3DXVECTOR4	MipMapSize;
};

class GVoxelConeTracer : public GRenderPass
{
protected:
	UINT	VolumeX;
	UINT	VolumeY;
	UINT	VolumeZ;

	GTexture3D*			Voxel_Address = nullptr;

	GStructuredBuffer*	VCTVoxels = nullptr;

	ID3D11Texture2D*	Small_Texture = nullptr;
	ID3D11RenderTargetView*		Small_RTV = nullptr;

	ID3D11VertexShader*         SceneVertexShader = NULL;
	ID3D11PixelShader*          ScenePixelShader = NULL;
	ID3D11GeometryShader*		VoxelGeometryShader = NULL;

	ID3D11ComputeShader*		MipmapShader = NULL;
	ID3D11ComputeShader*		UnpackShader = NULL;
	ID3D11ComputeShader*		ResolveShader = NULL;

	D3D11_VIEWPORT				Voxelization_VP;

	ID3D11Buffer*               ConstBuffer = NULL;

	GPostProcess				VoxelVisulizationPass;

	void	SetMipmapDimension(UINT w, UINT h, UINT d);

public:
	GVoxelConeTracer()	{}
	~GVoxelConeTracer()	{}

	GTexture3D*			VCTVolumes[5] = { nullptr };

	virtual void Init(const GRenderContext& InContext) override;
	virtual void Render(UINT InSRVCount, ID3D11ShaderResourceView** InputSRV, ID3D11RenderTargetView* OutputRTV) override;
	void Voxelization(class GSceneRenderer* InSceneRenderer);
	virtual void Shutdown() override;

};

#endif

