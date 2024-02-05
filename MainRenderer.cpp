//--------------------------------------------------------------------------------------
// renderer framework started with directX BasicHLSL sample
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsDlg.h"
#include "SDKmisc.h"
#include "SDKMesh.h"
#include "resource.h"

#include "Source\SceneRenderer.h"
#include "Source\PostProcess.h"
#include "Source\SkyLight.h"
#include "Source\MotionBlur.h"
#include "Source\TiledDeferredLighting.h"
#include "Source\VoxelConeTracer.h"
#include "Source\AtmosphereScattering.h"
#include "Source\Histogram.h"
#include "Source\DOF.h"
#include "Source\TemporalAA.h"

#include "Source\RenderPass.h"

#pragma comment(lib, "legacy_stdio_definitions.lib")

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CDXUTDialogResourceManager  g_DialogResourceManager; // manager for shared resources of dialogs
CFirstPersonCamera          g_Camera;               // A model viewing camera
CDXUTDirectionWidget        g_LightControl;
CD3DSettingsDlg             g_D3DSettingsDlg;       // Device settings dialog
CDXUTDialog                 g_HUD;                  // manages the 3D   
CDXUTDialog                 g_SampleUI;             // dialog for sample specific controls
bool                        g_bShowHelp = false;    // If true, it renders the UI control text

CDXUTTextHelper*            g_pTxtHelper = NULL;

GSceneRenderer				*g_SceneRenderer = NULL;

GRenderPassManager			g_RenderPassMgr;

D3D11_VIEWPORT				Output_VP;

UINT						g_ViewportWidth = 1280;
UINT						g_ViewportHeight = 720;
float						g_FOV = D3DX_PI / 3;

float						g_param_exposure = 20.0f;
D3DXVECTOR4					g_param_LightingControl = D3DXVECTOR4(50.0f, 0.5f, 1.0f, 2.0f);

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           3
#define IDC_CHANGEDEVICE        4

#define IDC_TONEMAP_EXPOSURE_TEXT  5
#define IDC_TONEMAP_EXPOSURE  6

#define IDC_DIR_LIGHT_TEXT  7
#define IDC_DIR_LIGHT  8

#define IDC_SKY_LIGHT_TEXT  9
#define IDC_SKY_LIGHT  10

#define IDC_POINT_LIGHTS 11
#define IDC_DOF 12
#define IDC_ATMOSPHERE_SCATTERING 13
#define IDC_MOTION_BLUR 14
#define IDC_ENABLE_TONEMAPPING 15
#define IDC_ENABLE_HISTOGRAM 16

#define IDC_ENABLE_VCT 17
#define IDC_ENABLE_LPV 18
#define IDC_ENABLE_RAYTRACE_SPHERE 19

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );

bool CALLBACK IsD3D11DeviceAcceptable(const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext );
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext );
void CALLBACK OnD3D11DestroyDevice( void* pUserContext );
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                  float fElapsedTime, void* pUserContext );

void InitApp();
void RenderText();


//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // DXUT will create and use the best device (either D3D9 or D3D11) 
    // that is available on the system depending on which D3D callbacks are set below

    // Set DXUT callbacks
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( OnKeyboard );
    DXUTSetCallbackFrameMove( OnFrameMove );

    DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );

    InitApp();
    DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"MainRenderer" );
    DXUTCreateDevice (D3D_FEATURE_LEVEL_11_0, true, g_ViewportWidth, g_ViewportHeight );
    //DXUTCreateDevice(true, 640, 480);
    DXUTMainLoop(); // Enter into the DXUT render loop

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    D3DXVECTOR3 vLightDir( -0.5f, 1, -0.5f );
    D3DXVec3Normalize( &vLightDir, &vLightDir );
    g_LightControl.SetLightDirection( vLightDir );

    // Initialize dialogs
    g_D3DSettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent ); int iY = 10;
//    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 0, iY, 170, 23 );

	WCHAR desc[256];
	swprintf_s(desc, L"Exposure ");

	g_HUD.AddStatic(IDC_TONEMAP_EXPOSURE_TEXT, desc, 0, iY + 26, 30, 10);
	g_HUD.AddSlider(IDC_TONEMAP_EXPOSURE, 0, iY += 46, 128, 15, 1, 100, (int)g_param_exposure);

	swprintf_s(desc, L"Directional Light ");
	g_HUD.AddStatic(IDC_DIR_LIGHT_TEXT, desc, 0, iY + 26, 30, 10);
	g_HUD.AddSlider(IDC_DIR_LIGHT, 0, iY += 46, 128, 15, 0, 100, (int)(g_param_LightingControl.x));

	swprintf_s(desc, L"Sky Light ");
	g_HUD.AddStatic(IDC_SKY_LIGHT_TEXT, desc, 0, iY + 26, 30, 10);
	g_HUD.AddSlider(IDC_SKY_LIGHT, 0, iY += 46, 128, 15, 0, 200, (int)(g_param_LightingControl.y*100.0f));

	swprintf_s(desc, L"Tiled deferred lighting");
	g_HUD.AddCheckBox(IDC_POINT_LIGHTS, desc, 0, iY += 26, 128, 15);

	swprintf_s(desc, L"DOF");
	g_HUD.AddCheckBox(IDC_DOF, desc, 0, iY += 26, 128, 15);

	swprintf_s(desc, L"Volumetric fog and cloud");
	g_HUD.AddCheckBox(IDC_ATMOSPHERE_SCATTERING, desc, 0, iY += 26, 128, 15);

	swprintf_s(desc, L"Motion blur");
	g_HUD.AddCheckBox(IDC_MOTION_BLUR, desc, 0, iY += 26, 128, 15, true);

	swprintf_s(desc, L"Tone mapping");
	g_HUD.AddCheckBox(IDC_ENABLE_TONEMAPPING, desc, 0, iY += 26, 128, 15, true);

	swprintf_s(desc, L"Tonemapping using histogram");
	g_HUD.AddCheckBox(IDC_ENABLE_HISTOGRAM, desc, 0, iY += 26, 128, 15, true);

	swprintf_s(desc, L"Voxel cone trace");
	g_HUD.AddCheckBox(IDC_ENABLE_VCT, desc, 0, iY += 26, 128, 15, true);

	swprintf_s(desc, L"Light propagation volume");
	g_HUD.AddCheckBox(IDC_ENABLE_LPV, desc, 0, iY += 26, 128, 15, false);

	swprintf_s(desc, L"Ray Trace Sphere");
	g_HUD.AddCheckBox(IDC_ENABLE_RAYTRACE_SPHERE, desc, 0, iY += 26, 128, 15, false);

    g_SampleUI.SetCallback( OnGUIEvent ); iY = 10;
}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D11 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    // Uncomment this to get debug information from D3D11
    //pDeviceSettings->d3d11.CreateFlags |= D3D11_CREATE_DEVICE_DEBUG;
	pDeviceSettings->d3d11.SyncInterval = 2;

    // For the first device created if its a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if( s_bFirstTime )
    {
        s_bFirstTime = false;
        if( ( DXUT_D3D11_DEVICE == pDeviceSettings->ver &&
              pDeviceSettings->d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE ) )
        {
            DXUTDisplaySwitchingToREFWarning( pDeviceSettings->ver );
        }
    }

    return true;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
	D3DXVECTOR3	PreEyePos = (*g_Camera.GetEyePt());
	D3DXVECTOR3	PreEyeDir = (*g_Camera.GetWorldAhead());

    // Update the camera's position based on user input 
    g_Camera.FrameMove( fElapsedTime );

	D3DXVECTOR3	EyePos = (*g_Camera.GetEyePt());
	D3DXVECTOR3	EyeDir = (*g_Camera.GetWorldAhead());

	D3DXVECTOR3 PosDiff = EyePos - PreEyePos;
	D3DXVECTOR3 DirDiff = EyeDir - PreEyeDir;

	if (D3DXVec3Length(&PosDiff) > 0.0001f || D3DXVec3Length(&DirDiff) > 0.0001f)
	{
		if (g_SceneRenderer)
		{
			g_SceneRenderer->AcumulatedFrameCount = 0;
		}
	}

}


//--------------------------------------------------------------------------------------
// Render the help and statistics text
//--------------------------------------------------------------------------------------
void RenderText()
{
//    UINT nBackBufferHeight = DXUTGetDXGIBackBufferSurfaceDesc()->Height;

    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 2, 0 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );

	//g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
    //g_pTxtHelper->DrawTextLine( L"Press F1 for help" );

    g_pTxtHelper->End();
}


//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext )
{
    // Pass messages to dialog resource manager calls so GUI state is updated correctly
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass messages to settings dialog if its active
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
        return 0;
    }

    // Give the dialogs a chance to handle the message first
    *pbNoFurtherProcessing = g_HUD.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;
    *pbNoFurtherProcessing = g_SampleUI.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    g_LightControl.HandleMessages( hWnd, uMsg, wParam, lParam );

    // Pass all remaining windows messages to camera so it can respond to user input
    g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );

    return 0;
}


//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
    if( bKeyDown )
    {
        switch( nChar )
        {
            case VK_F1:
                g_bShowHelp = !g_bShowHelp; break;
        }
    }
}


//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
	if (g_SceneRenderer == nullptr)
	{
		return;
	}

    switch( nControlID )
    {
//        case IDC_TOGGLEFULLSCREEN:
//            DXUTToggleFullScreen(); break;
	case IDC_TONEMAP_EXPOSURE:
		g_param_exposure = (float)g_HUD.GetSlider(IDC_TONEMAP_EXPOSURE)->GetValue();
		break;
	case IDC_DIR_LIGHT:
		g_SceneRenderer->LightingControl.x = (float)g_HUD.GetSlider(IDC_DIR_LIGHT)->GetValue();
		break;
	case IDC_SKY_LIGHT:
		g_SceneRenderer->LightingControl.y = ((float)g_HUD.GetSlider(IDC_SKY_LIGHT)->GetValue() / 100.0f);
		break;
	case IDC_POINT_LIGHTS:
		g_RenderPassMgr.bShowPointLights = g_HUD.GetCheckBox(IDC_POINT_LIGHTS)->GetChecked();
		break;
	case IDC_ATMOSPHERE_SCATTERING:
		g_RenderPassMgr.bShowAtmosphereScatterring = g_HUD.GetCheckBox(IDC_ATMOSPHERE_SCATTERING)->GetChecked();
		break;
	case IDC_DOF:
		g_RenderPassMgr.bShowDOF = g_HUD.GetCheckBox(IDC_DOF)->GetChecked();
		break;
	case IDC_MOTION_BLUR:
		g_RenderPassMgr.bShowMotionBlur = g_HUD.GetCheckBox(IDC_MOTION_BLUR)->GetChecked();
		break;
	case IDC_ENABLE_TONEMAPPING:
	case IDC_ENABLE_HISTOGRAM:
		{
			bool bEnableTonemapping = g_HUD.GetCheckBox(IDC_ENABLE_TONEMAPPING)->GetChecked();
			bool bEnableHistogram = g_HUD.GetCheckBox(IDC_ENABLE_HISTOGRAM)->GetChecked();

			if (bEnableTonemapping)
			{
				if (bEnableHistogram)
				{
					g_SceneRenderer->LightingControl.w = 2.0f;
				}
				else
				{
					g_SceneRenderer->LightingControl.w = 1.0f;
				}
			}
			else
			{
				g_SceneRenderer->LightingControl.w = 0.0f;
			}
		}
		break;
	case IDC_ENABLE_VCT:
	case IDC_ENABLE_LPV:
		{
			bool bEnableVCT = g_HUD.GetCheckBox(IDC_ENABLE_VCT)->GetChecked();
			bool bEnableLPV = g_HUD.GetCheckBox(IDC_ENABLE_LPV)->GetChecked();

			if (bEnableLPV && bEnableVCT)
			{
				bEnableLPV = false;
			}

			g_RenderPassMgr.bEnableVoxleConeTrace = bEnableVCT;
			g_RenderPassMgr.bEnableLPV = bEnableLPV;

			g_RenderPassMgr.ApplyGIControlParam(g_param_LightingControl);
			g_SceneRenderer->LightingControl = g_param_LightingControl;
		}
		break;
	case IDC_ENABLE_RAYTRACE_SPHERE:
		{
			bool bShowRayTraceSphere = g_HUD.GetCheckBox(IDC_ENABLE_RAYTRACE_SPHERE)->GetChecked();
			g_RenderPassMgr.bShowRayTraceSphere = bShowRayTraceSphere;
			g_SceneRenderer->bApplyTemporalJitter = !bShowRayTraceSphere;
		}
		break;
	default:
		break;
    }
}


//--------------------------------------------------------------------------------------
// Reject any D3D11 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}

//--------------------------------------------------------------------------------------
// Create any D3D11 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext )
{
    HRESULT hr;

    ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
    V_RETURN( g_DialogResourceManager.OnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11CreateDevice( pd3dDevice ) );
    g_pTxtHelper = new CDXUTTextHelper( pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 15 );


    // Setup the camera's view parameters
	D3DXVECTOR3 vecEye(100.0f, 5.0f, 5.0f);
	D3DXVECTOR3 vecAt(0.0f, 0.0f, 0.0f);

	D3DXVECTOR3 vMin = D3DXVECTOR3(-2000.0f, -1000.0f, -2000.0f);
	D3DXVECTOR3 vMax = D3DXVECTOR3(2000.0f, 1000.0f, 2000.0f);

	g_Camera.SetViewParams(&vecEye, &vecAt);
	g_Camera.SetRotateButtons(TRUE, FALSE, FALSE);
	//g_Camera.SetScalers(0.002f, 500.0f);
	g_Camera.SetScalers(0.005f, 800.0f);
	g_Camera.SetDrag(true);
	g_Camera.SetEnableYAxisMovement(true);
	g_Camera.SetClipToBoundary(FALSE, &vMin, &vMax);
	g_Camera.FrameMove(0);
	g_Camera.SetProjParams(g_FOV, (float)g_ViewportWidth /(float)g_ViewportHeight, 1.0f, 8000.0f);

	g_RenderPassMgr.Init(pd3dDevice, pd3dImmediateContext, g_ViewportWidth, g_ViewportHeight, g_FOV);

	Output_VP.Width = (float)g_ViewportWidth;
	Output_VP.Height = (float)g_ViewportHeight;
	Output_VP.MaxDepth = 1.0f;
	Output_VP.MinDepth = 0.0f;
	Output_VP.TopLeftX = 0.0f;
	Output_VP.TopLeftY = 0.0f;

	g_SceneRenderer = g_RenderPassMgr.GetSceneRenderPass();
	g_SceneRenderer->SetMainCamera(&g_Camera);

	g_RenderPassMgr.ApplyGIControlParam(g_param_LightingControl);
	g_SceneRenderer->LightingControl = g_param_LightingControl;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

	float	OutputW = (float)pBackBufferSurfaceDesc->Width;
	float	OutputH = (float)pBackBufferSurfaceDesc->Height;

	if (OutputW / OutputH >= (float)g_ViewportWidth / (float)g_ViewportHeight)
	{
		OutputH = OutputW / g_ViewportWidth*g_ViewportHeight;
		Output_VP.TopLeftX = 0.0f;
		Output_VP.TopLeftY = (-OutputH + (float)pBackBufferSurfaceDesc->Height) / 2.0f;
	}
	else
	{
		OutputW = OutputH / g_ViewportHeight*g_ViewportWidth;
		Output_VP.TopLeftX = (-OutputW + (float)pBackBufferSurfaceDesc->Width) / 2.0f;
		Output_VP.TopLeftY = 0.0f;
	}

	Output_VP.Width = OutputW;
	Output_VP.Height = OutputH;

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 300 );
    g_SampleUI.SetSize( 170, 300 );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D11 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                  float fElapsedTime, void* pUserContext )
{
    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }

    // Clear the render target and depth stencil
    float ClearColor[4] = { 0.0f, 0.25f, 0.25f, 0.55f };
    ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
    pd3dImmediateContext->ClearRenderTargetView( pRTV, ClearColor );
//    ID3D11DepthStencilView* pDSV = DXUTGetD3D11DepthStencilView();
//   pd3dImmediateContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );

	// Get the light direction
	D3DXVECTOR3 vLightDir = g_LightControl.GetLightDirection();
	D3DXMATRIX	ViewMat = *g_Camera.GetViewMatrix();
	g_LightControl.SetView(ViewMat);

	D3DXVECTOR4	jitterParams = D3DXVECTOR4(0.0f, 0.0f, 0.0f, 0.0f);
	GTemporalAA*	g_AA_pass = dynamic_cast<GTemporalAA*>(g_RenderPassMgr.GetRenderPass(ERenderPassID::TemporalAA));
	if (g_AA_pass)
	{
		jitterParams = g_AA_pass->GetCurrentJitterSample();
	}

	g_SceneRenderer->UpdateViewBuffer(vLightDir, g_param_exposure/20.0f, jitterParams);
	g_SceneRenderer->SetSamplerStates();

	g_RenderPassMgr.Render(pRTV);

	pd3dImmediateContext->RSSetViewports(1, &(Output_VP));

	ID3D11ShaderResourceView*	UpSamplingInput[1] = { g_SceneRenderer->FinalOutput->SRV };
	g_RenderPassMgr.GetRenderPass(ERenderPassID::UpSampling)->Render(1, UpSamplingInput, pRTV);

	pd3dImmediateContext->RSSetViewports(1, &(g_SceneRenderer->Main_VP));
	
	DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );
    g_HUD.OnRender( fElapsedTime );
    g_SampleUI.OnRender( fElapsedTime );
    RenderText();
    DXUT_EndPerfEvent();
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11ReleasingSwapChain();
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11DestroyDevice();
    g_D3DSettingsDlg.OnD3D11DestroyDevice();
    //CDXUTDirectionWidget::StaticOnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE( g_pTxtHelper );

	g_RenderPassMgr.Shutdown();
	g_SceneRenderer = nullptr;
}



