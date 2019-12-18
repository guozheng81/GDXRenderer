#pragma once

#ifndef _RENDER_UTILS_H_
#define _RENDER_UTILS_H_


#include <D3DCompiler.h>

HRESULT CompileShaderFromFile(WCHAR* szFileName, D3D_SHADER_MACRO *defines, LPCSTR szEntryPoint,
	LPCSTR szShaderModel, ID3DBlob** ppBlobOut = nullptr);

void LoadPreCompiledComputeShader(ID3D11Device* pd3dDevice, LPCWSTR strFilename, ID3D11ComputeShader **ppComputeShader);

#endif