#include "RenderUtils.h"
#include "DXUT.h"
#include "SDKmisc.h"

#include <fstream>
using namespace std;

HRESULT CompileShaderFromFile(WCHAR* szFileName, D3D_SHADER_MACRO* pDefines, LPCSTR szEntryPoint,
	LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
{
	HRESULT hr = S_OK;

	// find the file
	WCHAR str[MAX_PATH];
	V_RETURN(DXUTFindDXSDKMediaFileCch(str, MAX_PATH, szFileName));

	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
	// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
	// Setting this flag improves the shader debugging experience, but still allows 
	// the shaders to be optimized and to run exactly the way they will run in 
	// the release configuration of this program.
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	ID3DBlob* pErrorBlob;
	hr = D3DX11CompileFromFile(str, pDefines, NULL, szEntryPoint, szShaderModel,
		dwShaderFlags, 0, NULL, ppBlobOut, &pErrorBlob, NULL);
	if (FAILED(hr))
	{
		if (pErrorBlob != NULL)
			OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
		SAFE_RELEASE(pErrorBlob);
		return hr;
	}
	SAFE_RELEASE(pErrorBlob);

	return S_OK;
}

void LoadPreCompiledComputeShader(ID3D11Device* pd3dDevice, LPCWSTR strFilename, ID3D11ComputeShader **ppComputeShader)
{
	std::ifstream shader_stream;
	size_t shader_size;
	char* shader_data;

	WCHAR str[MAX_PATH];
	DXUTFindDXSDKMediaFileCch(str, MAX_PATH, strFilename);

	shader_stream.open(str, ifstream::in | ifstream::binary);
	if (shader_stream.good())
	{
		shader_stream.seekg(0, ios::end);
		shader_size = size_t(shader_stream.tellg());
		shader_data = new char[shader_size];
		shader_stream.seekg(0, ios::beg);
		shader_stream.read(&shader_data[0], shader_size);
		shader_stream.close();

		pd3dDevice->CreateComputeShader(shader_data, shader_size, 0, ppComputeShader);

		delete[] shader_data;
	}
}

