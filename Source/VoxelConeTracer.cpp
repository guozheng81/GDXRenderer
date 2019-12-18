#include "DXUT.h"
#include "SDKMesh.h"
#include "SDKmisc.h"
#include "DXUTcamera.h"

#include "SceneRenderer.h"
#include "VoxelConeTracer.h"
#include "RenderUtils.h"

void GVoxelConeTracer::Init(const GRenderContext& InContext)
{
	GRenderPass::Init(InContext);

	VolumeX = 256;
	VolumeY = 128;
	VolumeZ = 256;

	Voxel_Address = new GTexture3D(pd3dDevice, VolumeX, VolumeY, VolumeZ, DXGI_FORMAT_R32_UINT, true);

	UINT TwoPowerI = 1;
	for (UINT i = 0; i < 5; ++i)
	{
		VCTVolumes[i] = new GTexture3D(pd3dDevice, VolumeX/TwoPowerI, VolumeY / TwoPowerI, VolumeZ / TwoPowerI, DXGI_FORMAT_R16G16B16A16_FLOAT, true);
		TwoPowerI *= 2;
	}

	D3D11_TEXTURE2D_DESC	small_tex_desc;
	ZeroMemory(&small_tex_desc, sizeof(D3D11_TEXTURE2D_DESC));
	small_tex_desc.ArraySize = 1;
	small_tex_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	small_tex_desc.Usage = D3D11_USAGE_DEFAULT;
	small_tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	small_tex_desc.Width = 256;
	small_tex_desc.Height = 256;
	small_tex_desc.MipLevels = 1;
	small_tex_desc.SampleDesc.Count = 1;
	pd3dDevice->CreateTexture2D(&small_tex_desc, nullptr, &Small_Texture);

	D3D11_RENDER_TARGET_VIEW_DESC RT_desc;
	RT_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	RT_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	RT_desc.Texture2D.MipSlice = 0;
	pd3dDevice->CreateRenderTargetView(Small_Texture, &RT_desc, &Small_RTV);

	UINT	MaxVoxelDataCount = VolumeX*VolumeY*VolumeZ * 16;
	VCTVoxels = new GStructuredBuffer(pd3dDevice, sizeof(float) * 4, MaxVoxelDataCount, DXGI_FORMAT_UNKNOWN, true, true);

	ID3DBlob* pVertexShaderBuffer = NULL;
	CompileShaderFromFile(L"Shaders\\Scene.hlsl", NULL, "VSMain", "vs_5_0", &pVertexShaderBuffer);

	ID3DBlob* pPixelShaderBuffer = NULL;
	CompileShaderFromFile(L"Shaders\\Scene.hlsl", NULL, "VoxelPs", "ps_5_0", &pPixelShaderBuffer);

	// Create the shaders
	pd3dDevice->CreateVertexShader(pVertexShaderBuffer->GetBufferPointer(),
		pVertexShaderBuffer->GetBufferSize(), NULL, &SceneVertexShader);
	pd3dDevice->CreatePixelShader(pPixelShaderBuffer->GetBufferPointer(),
		pPixelShaderBuffer->GetBufferSize(), NULL, &ScenePixelShader);

	SAFE_RELEASE(pVertexShaderBuffer);
	SAFE_RELEASE(pPixelShaderBuffer);

	CompileShaderFromFile(L"Shaders\\Voxelization.hlsl", NULL, "VoxelGs", "gs_5_0", &pVertexShaderBuffer);
	pd3dDevice->CreateGeometryShader(pVertexShaderBuffer->GetBufferPointer(),
		pVertexShaderBuffer->GetBufferSize(), NULL, &VoxelGeometryShader);

	SAFE_RELEASE(pVertexShaderBuffer);

	ID3DBlob* ShaderBlob = nullptr;

	CompileShaderFromFile(L"Shaders\\Voxelization.hlsl", NULL, "DownSampleMipMap", "cs_5_0", &ShaderBlob);
	pd3dDevice->CreateComputeShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &MipmapShader);
	SAFE_RELEASE(ShaderBlob);

	CompileShaderFromFile(L"Shaders\\Voxelization.hlsl", NULL, "UnpackVoxel", "cs_5_0", &ShaderBlob);
	pd3dDevice->CreateComputeShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &UnpackShader);
	SAFE_RELEASE(ShaderBlob);

	CompileShaderFromFile(L"Shaders\\Voxelization.hlsl", NULL, "ResolveVoxel", "cs_5_0", &ShaderBlob);
	pd3dDevice->CreateComputeShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &ResolveShader);
	SAFE_RELEASE(ShaderBlob);

	D3D11_BUFFER_DESC const_desc;
	const_desc.Usage = D3D11_USAGE_DYNAMIC;
	const_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	const_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	const_desc.MiscFlags = 0;

	const_desc.ByteWidth = sizeof(CB_VOXELIZATION);
	pd3dDevice->CreateBuffer(&const_desc, NULL, &ConstBuffer);

	Voxelization_VP.Width = (float)small_tex_desc.Width;
	Voxelization_VP.Height = (float)small_tex_desc.Height;
	Voxelization_VP.MaxDepth = 1.0f;
	Voxelization_VP.MinDepth = 0.0f;
	Voxelization_VP.TopLeftX = 0.0f;
	Voxelization_VP.TopLeftY = 0.0f;

	VoxelVisulizationPass.SetPSShader(L"Shaders\\Voxelization.hlsl", "VisualizeVoxelPs");
	VoxelVisulizationPass.Init(InContext);
}

void GVoxelConeTracer::Voxelization(class GSceneRenderer* InSceneRenderer)
{
/*
	UINT clear_value_uint[4] = { 0, 0, 0, 0 };
	pd3dDeviceContext->ClearUnorderedAccessViewUint(Voxel_UAV, clear_value_uint);

	ID3D11UnorderedAccessView*	OutViews[1] = { Voxel_UAV };
	pd3dDeviceContext->OMSetRenderTargetsAndUnorderedAccessViews(1, &Small_RTV, nullptr, 1, 1, OutViews, nullptr);
*/

	UINT clear_value_uint[4] = { 0, 0, 0, 0 };
	pd3dDeviceContext->ClearUnorderedAccessViewUint(Voxel_Address->UAV, clear_value_uint);

	UINT	InitCounter[2] = { 0, 0 };
	ID3D11UnorderedAccessView*	OutViews[2] = { Voxel_Address->UAV, VCTVoxels->UAV };
	pd3dDeviceContext->OMSetRenderTargetsAndUnorderedAccessViews(1, &Small_RTV, nullptr, 1, 2, OutViews, InitCounter);

	pd3dDeviceContext->RSSetViewports(1, &Voxelization_VP);

	// Set the shaders
	pd3dDeviceContext->VSSetShader(SceneVertexShader, NULL, 0);
	pd3dDeviceContext->PSSetShader(ScenePixelShader, NULL, 0);
	pd3dDeviceContext->GSSetShader(VoxelGeometryShader, NULL, 0);

	pd3dDeviceContext->PSSetShaderResources(6, 1, &(InSceneRenderer->Shadow->SRV));

	InSceneRenderer->Voxelization();

/*	pd3dDeviceContext->CSSetShader(UnpackShader, NULL, 0);

	pd3dDeviceContext->CSSetShaderResources(0, 1, &Voxel_SRV);
	pd3dDeviceContext->CSSetUnorderedAccessViews(0, 1, &(VCTVolumes[0]->UAV), nullptr);
*/

	ID3D11UnorderedAccessView* ppUAViewNULL[3] = { NULL, NULL, NULL };
	pd3dDeviceContext->CSSetUnorderedAccessViews(0, 3, ppUAViewNULL, NULL);

	pd3dDeviceContext->CSSetShader(ResolveShader, NULL, 0);

	ID3D11ShaderResourceView*	ResolveInput[2] = { Voxel_Address->SRV,  VCTVoxels->SRV };
	pd3dDeviceContext->CSSetShaderResources(0, 2, ResolveInput);
	pd3dDeviceContext->CSSetUnorderedAccessViews(0, 1, &(VCTVolumes[0]->UAV), nullptr);

	pd3dDeviceContext->Dispatch(VolumeX/8, VolumeY/8, VolumeZ/8);

	pd3dDeviceContext->CSSetShader(MipmapShader, NULL, 0);

	UINT	MipMapX = VolumeX;
	UINT	MipMapY = VolumeY;
	UINT	MipMapZ = VolumeZ;
	for (UINT MipMapLevel = 1; MipMapLevel < 5; MipMapLevel++)
	{
		SetMipmapDimension(MipMapX, MipMapY, MipMapZ);
		pd3dDeviceContext->CSSetConstantBuffers(2, 1, &ConstBuffer);
		pd3dDeviceContext->CSSetUnorderedAccessViews(0, 1, &(VCTVolumes[MipMapLevel]->UAV), nullptr);
		pd3dDeviceContext->CSSetShaderResources(0, 1, &(VCTVolumes[MipMapLevel-1]->SRV));
		pd3dDeviceContext->Dispatch((UINT)(ceil((float)MipMapX / 6.0f)), (UINT)(ceil((float)MipMapY / 6.0f)), (UINT)(ceil((float)MipMapZ / 6.0f)));

		MipMapX /= 2;
		MipMapY /= 2;
		MipMapZ /= 2;
	}

	pd3dDeviceContext->CSSetShader(NULL, NULL, 0);

	pd3dDeviceContext->CSSetUnorderedAccessViews(0, 3, ppUAViewNULL, NULL);

	ID3D11ShaderResourceView* ppSRVNULL[2] = { NULL };
	pd3dDeviceContext->CSSetShaderResources(0, 2, ppSRVNULL);

}

void	GVoxelConeTracer::SetMipmapDimension(UINT w, UINT h, UINT d)
{
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	pd3dDeviceContext->Map(ConstBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	CB_VOXELIZATION* pPerFrame = (CB_VOXELIZATION*)MappedResource.pData;
	pPerFrame->MipMapSize = D3DXVECTOR4((float)w, (float)h, (float)d, 1.0f);
	pd3dDeviceContext->Unmap(ConstBuffer, 0);
}

void GVoxelConeTracer::Render(UINT InSRVCount, ID3D11ShaderResourceView** InputSRV, ID3D11RenderTargetView* OutputRTV)
{
	VoxelVisulizationPass.Render(1, &(VCTVolumes[0]->SRV), OutputRTV);
}

void GVoxelConeTracer::Shutdown()
{
	SAFE_DELETE(Voxel_Address);

	SAFE_DELETE(VCTVoxels);

	SAFE_DELETE(VCTVolumes[0]);
	SAFE_DELETE(VCTVolumes[1]);
	SAFE_DELETE(VCTVolumes[2]);
	SAFE_DELETE(VCTVolumes[3]);
	SAFE_DELETE(VCTVolumes[4]);

	SAFE_RELEASE(Small_RTV);
	SAFE_RELEASE(Small_Texture);

	SAFE_RELEASE(SceneVertexShader);
	SAFE_RELEASE(ScenePixelShader);
	SAFE_RELEASE(VoxelGeometryShader);

	SAFE_RELEASE(MipmapShader);
	SAFE_RELEASE(UnpackShader);
	SAFE_RELEASE(ResolveShader);

	SAFE_RELEASE(ConstBuffer);

	VoxelVisulizationPass.Shutdown();
}