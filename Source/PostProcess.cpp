#include "DXUT.h"
#include "SDKmisc.h"

#include "PostProcess.h"
#include "RenderUtils.h"

GPostProcess::GPostProcess()
{

}

GPostProcess::~GPostProcess()
{

}

void	GPostProcess::SetPSShader(WCHAR* InShaderFileName, LPCSTR InEntryFuncName)
{
	ShaderFileName = InShaderFileName;
	EntryFuncName = InEntryFuncName;
}

void GPostProcess::Init(const GRenderContext& InContext)
{
	GRenderPass::Init(InContext);

	ID3DBlob* ShaderBlob = nullptr;

	CompileShaderFromFile(L"Shaders\\PostProcessing.hlsl", NULL, "VSMain", "vs_5_0", &ShaderBlob);
	pd3dDevice->CreateVertexShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &ScreenVertexShader);

	const D3D11_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	pd3dDevice->CreateInputLayout(layout, 2, ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), &ScreenInputLayout);

	SAFE_RELEASE(ShaderBlob);

	CompileShaderFromFile(ShaderFileName, NULL, EntryFuncName, "ps_5_0", &ShaderBlob);
	pd3dDevice->CreatePixelShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &ScreenPixelShader);

	SAFE_RELEASE(ShaderBlob);

	GScreenVertex QuadVerts[4];

	QuadVerts[0].Pos = D3DXVECTOR4(-1.0f, 1.0f, 0.0f, 1.0f);
	QuadVerts[0].Tex = D3DXVECTOR2(0.0f, 0.0f);
	QuadVerts[1].Pos = D3DXVECTOR4(1.0f, 1.0f, 0.0f, 1.0f);
	QuadVerts[1].Tex = D3DXVECTOR2(1.0f, 0.0f);
	QuadVerts[2].Pos = D3DXVECTOR4(-1.0f, -1.0f, 0.0f, 1.0f);
	QuadVerts[2].Tex = D3DXVECTOR2(0.0f, 1.0f);
	QuadVerts[3].Pos = D3DXVECTOR4(1.0f, -1.0f, 0.0f, 1.0f);
	QuadVerts[3].Tex = D3DXVECTOR2(1.0f, 1.0f);

	D3D11_BUFFER_DESC Desc;
	Desc.Usage = D3D11_USAGE_DEFAULT;
	Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	Desc.ByteWidth = 4 * sizeof(GScreenVertex);
	Desc.CPUAccessFlags = 0;
	Desc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA Data;
	Data.pSysMem = QuadVerts;
	Data.SysMemPitch = 0;
	Data.SysMemSlicePitch = 0;

	pd3dDevice->CreateBuffer(&Desc, &Data, &ScreenVertexBuffer);
}

void GPostProcess::Render(UINT InSRVCount, ID3D11ShaderResourceView** InputSRV, ID3D11RenderTargetView* OutputRTV)
{
	UINT strides = sizeof(GScreenVertex);
	UINT offsets = 0;

	if (bClearRenderTarget)
	{		
		pd3dDeviceContext->ClearRenderTargetView(OutputRTV, ClearColor);
	}

	if (OutputRTV != nullptr)
	{
		pd3dDeviceContext->OMSetRenderTargets(1, &OutputRTV, nullptr);
	}

	pd3dDeviceContext->IASetInputLayout(ScreenInputLayout);
	pd3dDeviceContext->IASetVertexBuffers(0, 1, &ScreenVertexBuffer, &strides, &offsets);

	pd3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	if (bUseDefaultVS)
	{
		pd3dDeviceContext->VSSetShader(ScreenVertexShader, NULL, 0);
	}
	pd3dDeviceContext->PSSetShader(ScreenPixelShader, NULL, 0);

	if (InSRVCount > 0)
	{
		pd3dDeviceContext->PSSetShaderResources(0, InSRVCount, InputSRV);
	}

	if (InstanceNum > 1)
	{
		pd3dDeviceContext->DrawInstanced(4, InstanceNum, 0, 0);
	}
	else
	{
		pd3dDeviceContext->Draw(4, 0);
	}

	ID3D11ShaderResourceView* pSRVs[16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
	pd3dDeviceContext->PSSetShaderResources(0, 16, pSRVs);
}

void GPostProcess::Shutdown()
{
	SAFE_RELEASE(ScreenVertexBuffer);
	SAFE_RELEASE(ScreenVertexShader);
	SAFE_RELEASE(ScreenPixelShader);


	SAFE_RELEASE(ScreenInputLayout);
}