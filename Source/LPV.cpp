#include "DXUT.h"
#include "SDKMesh.h"
#include "SDKmisc.h"
#include "DXUTcamera.h"

#include "LPV.h"
#include "SceneRenderer.h"
#include "RenderUtils.h"

void GLPV::Init(const GRenderContext& InContext)
{
	GRenderPass::Init(InContext);

	VolumeX = 32;
	VolumeY = 16;
	VolumeZ = 32;

	RSM_Depth = new GTextureDepthStencil(pd3dDevice, ShadowTextureSize, ShadowTextureSize);
	RSM_BufferA = new GTexture2D(pd3dDevice, ShadowTextureSize, ShadowTextureSize, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, true, false);

	Vpl_Address = new GTexture3D(pd3dDevice, VolumeX, VolumeY, VolumeZ, DXGI_FORMAT_R32_UINT, true);
	Gv_Address = new GTexture3D(pd3dDevice, VolumeX, VolumeY, VolumeZ, DXGI_FORMAT_R32_UINT, true);

	Resolved_Vpl_R = new GTexture3D(pd3dDevice, VolumeX, VolumeY, VolumeZ, DXGI_FORMAT_R16G16B16A16_FLOAT, true);
	Resolved_Vpl_G = new GTexture3D(pd3dDevice, VolumeX, VolumeY, VolumeZ, DXGI_FORMAT_R16G16B16A16_FLOAT, true);
	Resolved_Vpl_B = new GTexture3D(pd3dDevice, VolumeX, VolumeY, VolumeZ, DXGI_FORMAT_R16G16B16A16_FLOAT, true);


	UINT	MaxVoxelDataCount = VolumeX*VolumeY*VolumeZ * 16;

	Vpl = new GStructuredBuffer(pd3dDevice, sizeof(float) * 4 * 3 + sizeof(int), MaxVoxelDataCount, DXGI_FORMAT_UNKNOWN, true, true);
	Gv = new GStructuredBuffer(pd3dDevice, sizeof(float) * 4 + sizeof(int), MaxVoxelDataCount, DXGI_FORMAT_UNKNOWN, true, true);

	Accum_Vpl = new GStructuredBuffer(pd3dDevice, sizeof(float) * 4 * 3, VolumeX*VolumeY*VolumeZ, DXGI_FORMAT_UNKNOWN, true, false);
	Propagate_Vpl = new GStructuredBuffer(pd3dDevice, sizeof(float) * 4 * 3, VolumeX*VolumeY*VolumeZ, DXGI_FORMAT_UNKNOWN, true, false);

	Accum_Gv = new GStructuredBuffer(pd3dDevice, sizeof(float) * 4, VolumeX*VolumeY*VolumeZ, DXGI_FORMAT_UNKNOWN, true, false);

	ID3DBlob* ShaderBlob = nullptr;

	CompileShaderFromFile(L"Shaders\\LPV.hlsl", NULL, "AccumulateVpl", "cs_5_0", &ShaderBlob);
	pd3dDevice->CreateComputeShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &AccumulateVplShader);
	SAFE_RELEASE(ShaderBlob);

	CompileShaderFromFile(L"Shaders\\LPV.hlsl", NULL, "AccumulateGv", "cs_5_0", &ShaderBlob);
	pd3dDevice->CreateComputeShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &AccumulateGvShader);
	SAFE_RELEASE(ShaderBlob);

	CompileShaderFromFile(L"Shaders\\LPV.hlsl", NULL, "ResolveVpl", "cs_5_0", &ShaderBlob);
	pd3dDevice->CreateComputeShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &ResolveVplShader);
	SAFE_RELEASE(ShaderBlob);

	CompileShaderFromFile(L"Shaders\\LPV.hlsl", NULL, "PropagateVpl", "cs_5_0", &ShaderBlob);
	pd3dDevice->CreateComputeShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &PropagateVplShader);
	SAFE_RELEASE(ShaderBlob);


}

void GLPV::Render(UINT InSRVCount, ID3D11ShaderResourceView** InputSRV, ID3D11RenderTargetView* OutputRTV)
{
	GSceneRenderer*	SceneRenderer = OwnerMgr->GetSceneRenderPass();

	UINT clear_value_uint[4] = { 0, 0, 0, 0 };
	pd3dDeviceContext->ClearUnorderedAccessViewUint(Vpl_Address->UAV, clear_value_uint);
	pd3dDeviceContext->ClearUnorderedAccessViewUint(Gv_Address->UAV, clear_value_uint);

	ID3D11UnorderedAccessView*	OutUAVs[4] = { Vpl_Address->UAV, Vpl->UAV, Gv_Address->UAV, Gv->UAV };
	SceneRenderer->RenderRSM(RSM_BufferA->RTV, OutUAVs, RSM_Depth->DSV);

	ID3D11ShaderResourceView*	ResolveInput[2] = { Vpl_Address->SRV, Vpl->SRV };
	pd3dDeviceContext->CSSetShaderResources(0, 2, ResolveInput);

	pd3dDeviceContext->CSSetUnorderedAccessViews(0, 1, &Accum_Vpl->UAV, nullptr);

	pd3dDeviceContext->CSSetShader(AccumulateVplShader, NULL, 0);
	pd3dDeviceContext->Dispatch(VolumeX / 4, VolumeY / 4, VolumeZ / 4);

	ResolveInput[0] = Gv_Address->SRV;
	ResolveInput[1] = Gv->SRV;
	pd3dDeviceContext->CSSetShaderResources(0, 2, ResolveInput);

	pd3dDeviceContext->CSSetUnorderedAccessViews(0, 1, &Accum_Gv->UAV, nullptr);

	pd3dDeviceContext->CSSetShader(AccumulateGvShader, NULL, 0);
	pd3dDeviceContext->Dispatch(VolumeX / 4, VolumeY / 4, VolumeZ / 4);

	ID3D11ShaderResourceView* ppSRVNULL[6] = { NULL, NULL, NULL, NULL, NULL, NULL };

	bool bUsingPropagateBuffer = true;
	for (UINT i = 0; i < 8; ++i)
	{
		pd3dDeviceContext->CSSetShaderResources(0, 6, ppSRVNULL);
		pd3dDeviceContext->CSSetUnorderedAccessViews(0, 1, bUsingPropagateBuffer? &Propagate_Vpl->UAV : &Accum_Vpl->UAV, nullptr);
		pd3dDeviceContext->CSSetShader(PropagateVplShader, NULL, 0);
		pd3dDeviceContext->CSSetShaderResources(0, 1, bUsingPropagateBuffer ? &Accum_Vpl->SRV : &Propagate_Vpl->SRV);
		pd3dDeviceContext->Dispatch(VolumeX / 4, VolumeY / 4, VolumeZ / 4);
		pd3dDeviceContext->CSSetShaderResources(1, 1, &Accum_Gv->SRV);

		bUsingPropagateBuffer = !bUsingPropagateBuffer;
	}

	ID3D11UnorderedAccessView*	OutVplUAV[3] = { Resolved_Vpl_R->UAV, Resolved_Vpl_G->UAV, Resolved_Vpl_B->UAV };
	pd3dDeviceContext->CSSetUnorderedAccessViews(0, 3, OutVplUAV, nullptr);
	pd3dDeviceContext->CSSetShaderResources(0, 1, bUsingPropagateBuffer ? &Propagate_Vpl->SRV : &Accum_Vpl->SRV);

	pd3dDeviceContext->CSSetShader(ResolveVplShader, NULL, 0);

	pd3dDeviceContext->Dispatch(VolumeX / 4, VolumeY / 4, VolumeZ / 4);

	pd3dDeviceContext->CSSetShader(NULL, NULL, 0);

	ID3D11UnorderedAccessView* ppUAViewNULL[4] = { NULL, NULL, NULL, NULL };
	pd3dDeviceContext->CSSetUnorderedAccessViews(0, 4, ppUAViewNULL, NULL);

	pd3dDeviceContext->CSSetShaderResources(0, 6, ppSRVNULL);

//	CopyPass.Render(1, &RSM_BufferA->SRV, OutputRTV);
}

void GLPV::Shutdown()
{
	SAFE_DELETE(RSM_Depth);
	SAFE_DELETE(RSM_BufferA);

	SAFE_DELETE(Vpl_Address);
	SAFE_DELETE(Vpl);

	SAFE_DELETE(Gv_Address);
	SAFE_DELETE(Gv);

	SAFE_RELEASE(AccumulateVplShader);
	SAFE_RELEASE(ResolveVplShader);
	SAFE_RELEASE(PropagateVplShader);
	SAFE_RELEASE(AccumulateGvShader);

	SAFE_DELETE(Accum_Vpl);
	SAFE_DELETE(Propagate_Vpl);
	SAFE_DELETE(Accum_Gv);


	SAFE_DELETE(Resolved_Vpl_R);
	SAFE_DELETE(Resolved_Vpl_G);
	SAFE_DELETE(Resolved_Vpl_B);

}
