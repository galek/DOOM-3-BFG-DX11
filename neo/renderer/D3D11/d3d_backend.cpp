// D3D headers
#include "d3d_common.h"
#include "d3d_backend.h"
#include "d3d_state.h"

void SetupVideoConfig();

//----------------------------------------------------------------------------
// Local data
//----------------------------------------------------------------------------
HRESULT g_hrLastError = S_OK;

//----------------------------------------------------------------------------
//
// DRIVER INTERFACE
//
//----------------------------------------------------------------------------
void D3DDrv_Shutdown( void )
{
    DestroyDrawState();
}

void D3DDrv_DriverInit( void )
{
    InitDrawState();
    SetupVideoConfig();
}

size_t D3DDrv_LastError( void )
{
    return (size_t) g_hrLastError;
}

//----------------------------------------------------------------------------
// Clear the back buffers
//----------------------------------------------------------------------------
#if 0 
void D3DDrv_Clear( unsigned long bits, const float* clearCol, unsigned long stencil, float depth )
{
    if ( bits & CLEAR_COLOR )
    {
        static float defaultCol[] = { 0, 0, 0, 0 };
        if ( !clearCol ) { clearCol = defaultCol; }

        g_pImmediateContext->ClearRenderTargetView( g_BufferState.backBufferView, clearCol );
    }

    if ( bits & ( CLEAR_DEPTH | CLEAR_STENCIL ) )
    {
        DWORD clearBits = 0;
        if ( bits & CLEAR_DEPTH ) { clearBits |= D3D11_CLEAR_DEPTH; }
        if ( bits & CLEAR_STENCIL ) { clearBits |= D3D11_CLEAR_STENCIL; }
        g_pImmediateContext->ClearDepthStencilView( g_BufferState.depthBufferView, clearBits, depth, (UINT8) stencil );
    }
}
#endif

//----------------------------------------------------------------------------
// Flush the command buffer
//----------------------------------------------------------------------------
void D3DDrv_Flush( void )
{
    g_pImmediateContext->End( g_DrawState.frameQuery );

    BOOL finished = FALSE;
    HRESULT hr;
    do
    {
        YieldProcessor();
        hr = g_pImmediateContext->GetData( g_DrawState.frameQuery, &finished, sizeof(finished), 0 );
    }
    while ( ( hr == S_OK || hr == S_FALSE ) && finished == FALSE );

    assert( SUCCEEDED( hr ) );
}

//----------------------------------------------------------------------------
// Present the frame
//----------------------------------------------------------------------------
void D3DDrv_EndFrame( int frequency )
{
    //int frequency = 0;
	//if ( r_swapInterval->integer > 0 ) 
    //{
	//	frequency = min( glConfig.displayFrequency, 60 / r_swapInterval->integer );
    //}

    DXGI_PRESENT_PARAMETERS parameters = {0};
	parameters.DirtyRectsCount = 0;
	parameters.pDirtyRects = nullptr;
	parameters.pScrollRect = nullptr;
	parameters.pScrollOffset = nullptr;
    
    HRESULT hr = S_OK;
    switch (frequency)
    {
    case 60: hr = g_pSwapChain->Present1( 1, 0, &parameters ); break; 
    case 30: hr = g_pSwapChain->Present1( 2, 0, &parameters ); break;
    default: hr = g_pSwapChain->Present1( 0, 0, &parameters ); break; 
    }

#ifdef WIN8
	// Discard the contents of the render target.
	// This is a valid operation only when the existing contents will be entirely
	// overwritten. If dirty or scroll rects are used, this call should be removed.
	g_pImmediateContext->DiscardView( g_BufferState.backBufferView );

	// Discard the contents of the depth stencil.
	g_pImmediateContext->DiscardView( g_BufferState.depthBufferView );

    // Present unbinds the rendertarget, so let's put it back (FFS)
    g_pImmediateContext->OMSetRenderTargets( 1, &g_BufferState.backBufferView, g_BufferState.depthBufferView );
#endif

	if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
	{
		// Someone kicked the cord out or something. REBOOT TEH VIDYOS!
        cmdSystem->ExecuteCommandText("vid_restart");
	}
}

void SetupVideoConfig()
{
    // Set up a bunch of default state
    switch ( g_BufferState.featureLevel )
    {
    case D3D_FEATURE_LEVEL_9_1 : glConfig.version_string = "v9.1 (Compatibility)"; break;
    case D3D_FEATURE_LEVEL_9_2 : glConfig.version_string = "v9.2 (Compatibility)"; break;
    case D3D_FEATURE_LEVEL_9_3 : glConfig.version_string = "v9.3 (Compatibility)"; break;
    case D3D_FEATURE_LEVEL_10_0: glConfig.version_string = "v10.0 (Compatibility)"; break;
    case D3D_FEATURE_LEVEL_10_1: glConfig.version_string = "v10.1 (Compatibility)"; break;
    case D3D_FEATURE_LEVEL_11_0: glConfig.version_string = "v11.0"; break;
    case D3D_FEATURE_LEVEL_11_1: glConfig.version_string = "v11.1"; break;
    default: glConfig.version_string = "Direct3D 11"; break;
    }
    glConfig.renderer_string = "Direct3D 11";
    glConfig.vendor_string = "Microsoft Corporation";
    glConfig.extensions_string = "";
    glConfig.wgl_extensions_string = "";
    glConfig.shading_language_string = "HLSL";
    glConfig.glVersion = 0;

    // TODO: 
    // glConfig.vendor?
    glConfig.uniformBufferOffsetAlignment = 0;
    

    D3D11_DEPTH_STENCIL_VIEW_DESC depthBufferViewDesc;
    g_BufferState.depthBufferView->GetDesc( &depthBufferViewDesc );

    DWORD colorDepth = 0, depthDepth = 0, stencilDepth = 0;
    if ( FAILED( QD3D::GetBitDepthForFormat( g_BufferState.swapChainDesc.Format, &colorDepth ) ) )
        common->Error( "Bad bit depth supplied for color channel (%x)\n", g_BufferState.swapChainDesc.Format );

    if ( FAILED( QD3D::GetBitDepthForDepthStencilFormat( depthBufferViewDesc.Format, &depthDepth, &stencilDepth ) ) )
        common->Error( "Bad bit depth supplied for depth-stencil (%x)\n", depthBufferViewDesc.Format );

    glConfig.maxTextureSize = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    glConfig.maxTextureCoords = D3D11_REQ_SAMPLER_OBJECT_COUNT_PER_DEVICE;
    glConfig.maxTextureImageUnits = D3D11_REQ_SAMPLER_OBJECT_COUNT_PER_DEVICE;
    glConfig.maxTextureAnisotropy = D3D11_REQ_MAXANISOTROPY;
    glConfig.colorBits = (int) colorDepth;
    glConfig.depthBits = (int) depthDepth;
    glConfig.stencilBits = (int) stencilDepth;

    glConfig.multitextureAvailable = true;
    glConfig.directStateAccess = false;
    glConfig.textureCompressionAvailable = true;
    glConfig.anisotropicFilterAvailable = true;
    glConfig.textureLODBiasAvailable = true;
    glConfig.seamlessCubeMapAvailable = true;
    glConfig.sRGBFramebufferAvailable = true;
    glConfig.vertexBufferObjectAvailable = true;
    glConfig.mapBufferRangeAvailable = true;
    glConfig.drawElementsBaseVertexAvailable = true;
    glConfig.fragmentProgramAvailable = true;
    glConfig.glslAvailable = true;
    glConfig.uniformBufferAvailable = true;
    glConfig.twoSidedStencilAvailable = true;
    glConfig.depthBoundsTestAvailable = false; // @pjb: todo, check me
    glConfig.syncAvailable = true;
    glConfig.timerQueryAvailable = true;
    glConfig.occlusionQueryAvailable = true;
    glConfig.debugOutputAvailable = true;
    glConfig.swapControlTearAvailable = true;

    glConfig.stereo3Dmode = STEREO3D_OFF;
    glConfig.nativeScreenWidth = g_BufferState.swapChainDesc.Width;
    glConfig.nativeScreenHeight = g_BufferState.swapChainDesc.Height;
    glConfig.isStereoPixelFormat = false;
    glConfig.stereoPixelFormatAvailable = false;
    glConfig.multisamples = g_BufferState.swapChainDesc.SampleDesc.Count;
    glConfig.pixelAspect = glConfig.nativeScreenWidth / (float)glConfig.nativeScreenHeight;
    glConfig.global_vao = 0;

#if !defined(_ARM_) && !defined(D3D_NO_ENUM_DISPLAY)
	DEVMODE dm;
    memset( &dm, 0, sizeof( dm ) );
	dm.dmSize = sizeof( dm );
	if ( EnumDisplaySettingsEx( NULL, ENUM_CURRENT_SETTINGS, &dm, 0 ) )
	{
		glConfig.displayFrequency = dm.dmDisplayFrequency;
	}
#else
    // @pjb: todo: EnumDisplaySettingsEx doesn't exist.
    glConfig.displayFrequency = 60;
#endif
}
















#if 0 
void D3DDrv_SetGamma( unsigned char red[256], unsigned char green[256], unsigned char blue[256] )
{
    // @pjb: todo?
    if ( glConfig.deviceSupportsGamma )
    {
        common->FatalError( "D3D11 hardware gamma ramp not implemented." );
    }
}

void D3DDrv_SetProjection( const float* projMatrix )
{
    memcpy( g_RunState.vsConstants.projectionMatrix, projMatrix, sizeof(float) * 16 );
    g_RunState.vsDirtyConstants = true;
}

void D3DDrv_GetProjection( float* projMatrix )
{
    memcpy( projMatrix, g_RunState.vsConstants.projectionMatrix, sizeof(float) * 16 );
}

void D3DDrv_SetModelView( const float* modelViewMatrix )
{
    memcpy( g_RunState.vsConstants.modelViewMatrix, modelViewMatrix, sizeof(float) * 16 );
    g_RunState.vsDirtyConstants = true;
}

void D3DDrv_GetModelView( float* modelViewMatrix )
{
    memcpy( modelViewMatrix, g_RunState.vsConstants.modelViewMatrix, sizeof(float) * 16 );
}

void D3DDrv_ResetState2D( void )
{
    D3DDrv_SetModelView( s_identityMatrix );
    D3DDrv_SetState( GLS_DEPTHTEST_DISABLE |
			  GLS_SRCBLEND_SRC_ALPHA |
			  GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA );

    CommitRasterizerState( CT_TWO_SIDED, false, false );

    D3DDrv_SetPortalRendering( false, NULL, NULL );
    D3DDrv_SetDepthRange( 0, 0 );
}

void D3DDrv_ResetState3D( void )
{
    D3DDrv_SetModelView( s_identityMatrix );
    D3DDrv_SetState( GLS_DEFAULT );
    D3DDrv_SetDepthRange( 0, 1 );
}

void D3DDrv_SetPortalRendering( bool enabled, const float* flipMatrix, const float* plane )
{
    if ( enabled ) 
    {
        // Transform the supplied plane by the INVERSE of the flipMatrix
        // We can just transpose the flipMatrix because it's orthogonal
        // To clip, dot(vertex, plane) < 0
        g_RunState.psConstants.clipPlane[0] = flipMatrix[ 0] * plane[0] + flipMatrix[ 4] * plane[1] + flipMatrix[ 8] * plane[2] + flipMatrix[12] * plane[3];
        g_RunState.psConstants.clipPlane[1] = flipMatrix[ 1] * plane[0] + flipMatrix[ 5] * plane[1] + flipMatrix[ 9] * plane[2] + flipMatrix[13] * plane[3]; 
        g_RunState.psConstants.clipPlane[2] = flipMatrix[ 2] * plane[0] + flipMatrix[ 6] * plane[1] + flipMatrix[10] * plane[2] + flipMatrix[14] * plane[3]; 
        g_RunState.psConstants.clipPlane[3] = flipMatrix[ 3] * plane[0] + flipMatrix[ 7] * plane[1] + flipMatrix[11] * plane[2] + flipMatrix[15] * plane[3]; 
    }
    else
    {
        // Reset the clip plane
        g_RunState.psConstants.clipPlane[0] = 
        g_RunState.psConstants.clipPlane[1] = 
        g_RunState.psConstants.clipPlane[2] = 
        g_RunState.psConstants.clipPlane[3] = 0;
    }

    g_RunState.psDirtyConstants = true;
}

void D3DDrv_SetDepthRange( float minRange, float maxRange )
{
    g_RunState.vsConstants.depthRange[0] = minRange;
    g_RunState.vsConstants.depthRange[1] = maxRange - minRange;
    g_RunState.vsDirtyConstants = true;
}

void D3DDrv_SetDrawBuffer( int buffer )
{

}

void D3DDrv_MakeCurrent( bool current )
{
    
}

void D3DDrv_DebugSetOverdrawMeasureEnabled( bool enabled )
{

}

void D3DDrv_DebugSetTextureMode( const char* mode )
{

}

#endif