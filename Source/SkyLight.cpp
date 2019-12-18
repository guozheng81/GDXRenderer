#include "DXUT.h"
#include "SDKmisc.h"

#include "SkyLight.h"
#include "SceneRenderer.h"
#include "RenderUtils.h"

GSkyLight::GSkyLight()
{

}

GSkyLight::~GSkyLight()
{

}

void GSkyLight::Init(const GRenderContext& InContext)
{
	GRenderPass::Init(InContext);
	ID3DBlob* ShaderBlob = nullptr;

/*
	CompileShaderFromFile(L"Shaders\\SkyLight.hlsl", NULL, "SHConvert", "cs_5_0", &ShaderBlob);
	pd3dDevice->CreateComputeShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &SHConvertShader);
	SAFE_RELEASE(ShaderBlob);

	CompileShaderFromFile(L"Shaders\\SkyLight.hlsl", NULL, "SHCombine", "cs_5_0", &ShaderBlob);
	pd3dDevice->CreateComputeShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &SHCombineShader);
	SAFE_RELEASE(ShaderBlob);

	CompileShaderFromFile(L"Shaders\\SkyLight.hlsl", NULL, "SpecularFilter", "cs_5_0", &ShaderBlob);
	pd3dDevice->CreateComputeShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &SpecularFilterShader);
	SAFE_RELEASE(ShaderBlob);

	CompileShaderFromFile(L"Shaders\\SkyLight.hlsl", NULL, "SpecularIntegration", "cs_5_0", &ShaderBlob);
	pd3dDevice->CreateComputeShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &SpecularIntegrationShader);
	SAFE_RELEASE(ShaderBlob);
*/
	LoadPreCompiledComputeShader(pd3dDevice, L"Shaders\\Bin\\SHConvert.cso", &SHConvertShader);
	LoadPreCompiledComputeShader(pd3dDevice, L"Shaders\\Bin\\SHCombine.cso", &SHCombineShader);
	LoadPreCompiledComputeShader(pd3dDevice, L"Shaders\\Bin\\SpecularFilter.cso", &SpecularFilterShader);
	LoadPreCompiledComputeShader(pd3dDevice, L"Shaders\\Bin\\SpecularIntegration.cso", &SpecularIntegrationShader);
	
	CubeMapSize = OwnerMgr->GetSceneRenderPass()->SkyCubeSize;
	GroupCount = CubeMapSize / GroupThreadSize;

	Diffuse1D = new GStructuredBuffer(pd3dDevice, sizeof(float) * 4 * 3, GroupCount*GroupCount);
	Diffuse = new GStructuredBuffer(pd3dDevice, sizeof(float) * 4 * 3, 1);

	D3D11_TEXTURE2D_DESC	Specular_tex_desc;
	ZeroMemory(&Specular_tex_desc, sizeof(Specular_tex_desc));
	Specular_tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	Specular_tex_desc.Width = CubeMapSize;
	Specular_tex_desc.Height = CubeMapSize;
	Specular_tex_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	Specular_tex_desc.ArraySize = 6;
	Specular_tex_desc.MipLevels = SpecularMaxRoughnessLevel;
	Specular_tex_desc.SampleDesc.Count = 1;
	Specular_tex_desc.Usage = D3D11_USAGE_DEFAULT;
	Specular_tex_desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

	pd3dDevice->CreateTexture2D(&Specular_tex_desc, nullptr, &SpecularCubeMap);

	D3D11_UNORDERED_ACCESS_VIEW_DESC	Specular_tex_uav_desc;
	Specular_tex_uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
	Specular_tex_uav_desc.Texture2DArray.ArraySize = 6;
	Specular_tex_uav_desc.Texture2DArray.FirstArraySlice = 0;
	Specular_tex_uav_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

	for (UINT MipIdx = 0; MipIdx < SpecularMaxRoughnessLevel; ++MipIdx)
	{
		Specular_tex_uav_desc.Texture2DArray.MipSlice = MipIdx;
		pd3dDevice->CreateUnorderedAccessView(SpecularCubeMap, &Specular_tex_uav_desc, &(Specular_UAV[MipIdx]));
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC		Specular_tex_srv_desc;
	Specular_tex_srv_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	Specular_tex_srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
	Specular_tex_srv_desc.TextureCube.MipLevels = SpecularMaxRoughnessLevel;
	Specular_tex_srv_desc.TextureCube.MostDetailedMip = 0;
	pd3dDevice->CreateShaderResourceView(SpecularCubeMap, &Specular_tex_srv_desc, &Specular_SRV);

	SpecularIntegration = new GTexture2D(pd3dDevice, 1024, 1024, DXGI_FORMAT_R16G16B16A16_FLOAT, false, true);

	D3D11_BUFFER_DESC const_desc;
	const_desc.Usage = D3D11_USAGE_DYNAMIC;
	const_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	const_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	const_desc.MiscFlags = 0;

	const_desc.ByteWidth = sizeof(CB_SKYLIGHT);
	pd3dDevice->CreateBuffer(&const_desc, NULL, &ConstBuffer);


}

void	GSkyLight::SetConstBuffer(float InSize, float InRoughness)
{
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	pd3dDeviceContext->Map(ConstBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	CB_SKYLIGHT* pPerFrame = (CB_SKYLIGHT*)MappedResource.pData;
	pPerFrame->SkyCubeSize = D3DXVECTOR4(InSize, InSize, 1.0f / InSize, 1.0f / InSize);
	pPerFrame->MaterialParams = D3DXVECTOR4(InRoughness, 0.0f, 0.0f, 0.0f);
	pd3dDeviceContext->Unmap(ConstBuffer, 0);
	pd3dDeviceContext->CSSetConstantBuffers(2, 1, &ConstBuffer);

}

void GSkyLight::Render(UINT InSRVCount, ID3D11ShaderResourceView** InputSRV, ID3D11RenderTargetView* OutputRTV)
{
	if (bHasRendered)
	{
		return;
	}

	float	skycube_size_float = (float)CubeMapSize;
	SetConstBuffer(skycube_size_float, 1.0f);

	// convert cube map to SH based
	pd3dDeviceContext->CSSetShader(SHConvertShader, NULL, 0);

	pd3dDeviceContext->CSSetShaderResources(0, 1, InputSRV);
	pd3dDeviceContext->CSSetUnorderedAccessViews(0, 1, &Diffuse1D->UAV, nullptr);

	pd3dDeviceContext->Dispatch(GroupCount, GroupCount, 1);

	// combine final SH coefficients
	pd3dDeviceContext->CSSetShader(SHCombineShader, NULL, 0);
	pd3dDeviceContext->CSSetUnorderedAccessViews(0, 1, &Diffuse->UAV, nullptr);
	pd3dDeviceContext->CSSetShaderResources(0, 1, &Diffuse1D->SRV);

	pd3dDeviceContext->Dispatch(1, 1, 1);

	// specular filtering
	pd3dDeviceContext->CSSetShader(SpecularFilterShader, NULL, 0);

	UINT	MipmapGroundCount = GroupCount;
	for (UINT i = 0; i < SpecularMaxRoughnessLevel; ++i)
	{
		SetConstBuffer(skycube_size_float, 0.05f + (1.0f-0.05f)/ (float)(SpecularMaxRoughnessLevel - 1)*i);

		pd3dDeviceContext->CSSetUnorderedAccessViews(0, 1, &(Specular_UAV[i]), nullptr);
		pd3dDeviceContext->CSSetShaderResources(0, 1, InputSRV);
		pd3dDeviceContext->Dispatch(MipmapGroundCount, MipmapGroundCount, 6);
		MipmapGroundCount /= 2;
		skycube_size_float = skycube_size_float*0.5f;
	}

	//
	pd3dDeviceContext->CSSetShader(SpecularIntegrationShader, NULL, 0);
	pd3dDeviceContext->CSSetUnorderedAccessViews(0, 1, &SpecularIntegration->UAV, nullptr);
	pd3dDeviceContext->Dispatch(1024 / 16, 1024 / 16, 1);

	pd3dDeviceContext->CSSetShader(NULL, NULL, 0);

	ID3D11UnorderedAccessView* ppUAViewNULL[1] = { NULL };
	pd3dDeviceContext->CSSetUnorderedAccessViews(0, 1, ppUAViewNULL, NULL);

	ID3D11ShaderResourceView* ppSRVNULL[1] = { NULL };
	pd3dDeviceContext->CSSetShaderResources(0, 1, ppSRVNULL);

//	ID3D11Buffer* ppCBNULL[1] = { NULL };
//	pd3dDeviceContext->CSSetConstantBuffers(0, 1, ppCBNULL);

	bHasRendered = true;
}

void GSkyLight::Shutdown()
{
	SAFE_RELEASE(SHConvertShader);
	SAFE_DELETE(Diffuse);

	SAFE_RELEASE(SHCombineShader);
	SAFE_DELETE(Diffuse1D);

	SAFE_RELEASE(SpecularFilterShader);
	SAFE_RELEASE(SpecularCubeMap);
	for (UINT i = 0; i < 5; ++i)
	{
		SAFE_RELEASE(Specular_UAV[i]);
	}
	SAFE_RELEASE(Specular_SRV);

	SAFE_RELEASE(SpecularIntegrationShader);
	SAFE_DELETE(SpecularIntegration);

	SAFE_RELEASE(ConstBuffer);
}