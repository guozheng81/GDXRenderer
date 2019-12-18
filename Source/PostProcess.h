#pragma once

#ifndef _POST_PROCESS_H_
#define _POST_PROCESS_H_

#include "RenderPass.h"

struct GScreenVertex
{
	D3DXVECTOR4 Pos;
	D3DXVECTOR2 Tex;
};


class GPostProcess : public GRenderPass
{
protected:
	ID3D11Buffer*	ScreenVertexBuffer;

	ID3D11VertexShader*		ScreenVertexShader = nullptr;
	ID3D11PixelShader*		ScreenPixelShader = nullptr;

	ID3D11InputLayout*		ScreenInputLayout = nullptr;

	WCHAR*	ShaderFileName = nullptr;
	LPCSTR EntryFuncName = nullptr;

public:
	GPostProcess();
	~GPostProcess();

	bool			bClearRenderTarget = true;
	bool			bUseDefaultVS = true;
	int				InstanceNum = 1;
	float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	void	SetPSShader(WCHAR* InShaderFileName, LPCSTR InEntryFuncName);

	virtual void Init(const GRenderContext& InContext) override;
	virtual void Render(UINT InSRVCount, ID3D11ShaderResourceView** InputSRV, ID3D11RenderTargetView* OutputRTV) override;
	virtual void Shutdown() override;
};

#endif