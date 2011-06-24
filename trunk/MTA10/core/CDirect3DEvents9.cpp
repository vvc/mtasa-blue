/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.0
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        core/CDirect3DEvents9.cpp
*  PURPOSE:     Handler implementations for Direct3D 9 events
*  DEVELOPERS:  Cecill Etheredge <ijsf@gmx.net>
*               Christian Myhre Lundheim <>
*               Derek Abdine <>
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#include "StdInc.h"
#define DECLARE_PROFILER_SECTION_CDirect3DEvents9
#include "profiler/SharedUtil.Profiler.h"

#include <stdexcept>

// Variables used for screen shot saving
static IDirect3DSurface9*  ms_pSaveLockSurface  = NULL;
static int                 ms_bSaveStarted      = 0;
static long long           ms_LastSaveTime      = 0;
static void*               ms_LastSetTextures[4] = { NULL, NULL, NULL, NULL };


void CDirect3DEvents9::OnDirect3DDeviceCreate  ( IDirect3DDevice9 *pDevice )
{
    WriteDebugEvent ( "CDirect3DEvents9::OnDirect3DDeviceCreate" );

    // Create the GUI manager
    CCore::GetSingleton ( ).InitGUI ( pDevice );

    // Create all our fonts n stuff
    CGraphics::GetSingleton ().OnDeviceCreate ( pDevice );

    // Create the GUI elements
    CLocalGUI::GetSingleton().CreateObjects ( pDevice );    
}

void CDirect3DEvents9::OnDirect3DDeviceDestroy ( IDirect3DDevice9 *pDevice )
{
    WriteDebugEvent ( "CDirect3DEvents9::OnDirect3DDeviceDestroy" );

    // Unload the current mod
    CModManager::GetSingleton ().Unload ();

    // Destroy the GUI elements
    CLocalGUI::GetSingleton().DestroyObjects ();

    // De-initialize the GUI manager (destroying is done on Exit)
    CCore::GetSingleton ( ).DeinitGUI ();
}

void CDirect3DEvents9::OnBeginScene ( IDirect3DDevice9 *pDevice )
{
    // Notify core
    CCore::GetSingleton ().DoPreFramePulse ();
}

bool CDirect3DEvents9::OnEndScene ( IDirect3DDevice9 *pDevice )
{
    return true;
}

void CDirect3DEvents9::OnInvalidate ( IDirect3DDevice9 *pDevice )
{
    WriteDebugEvent ( "CDirect3DEvents9::OnInvalidate" );

    // Invalidate the VMR9 Manager
    //CVideoManager::GetSingleton ().OnLostDevice ();

    // Notify gui
    CLocalGUI::GetSingleton().Invalidate ();

    CGraphics::GetSingleton ().OnDeviceInvalidate ( pDevice );
}

void CDirect3DEvents9::OnRestore ( IDirect3DDevice9 *pDevice )
{
    WriteDebugEvent ( "CDirect3DEvents9::OnRestore" );

    // Restore the VMR9 manager
    //CVideoManager::GetSingleton ().OnResetDevice ( pDevice );

    // Restore the GUI
    CLocalGUI::GetSingleton().Restore ();

    // Restore the graphics manager
    CGraphics::GetSingleton ().OnDeviceRestore ( pDevice );
}

void CDirect3DEvents9::OnPresent ( IDirect3DDevice9 *pDevice )
{
    // Start a new scene. This isn't ideal and is not really recommended by MSDN.
    // I tried disabling EndScene from GTA and just end it after this code ourselves
    // before present, but that caused graphical issues randomly with the sky.
    pDevice->BeginScene ();

    // Create a state block.
    IDirect3DStateBlock9 * pDeviceState = NULL;
    pDevice->CreateStateBlock ( D3DSBT_ALL, &pDeviceState );

    // Make sure linear sampling is enabled
    pDevice->SetSamplerState ( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    pDevice->SetSamplerState ( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    pDevice->SetSamplerState ( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );

    // Maybe capture screen
    CGraphics::GetSingleton ().GetRenderItemManager ()->UpdateBackBufferCopy ();

    // Notify core
    CCore::GetSingleton ().DoPostFramePulse ();

    // Draw pre-GUI primitives
    CGraphics::GetSingleton ().DrawPreGUIQueue ();

    // Draw the GUI
    CLocalGUI::GetSingleton().Draw ();

    // Draw post-GUI primitives
    CGraphics::GetSingleton ().DrawPostGUIQueue ();

    // Redraw the mouse cursor so it will always be over other elements
    CLocalGUI::GetSingleton().DrawMouseCursor();

    // Restore the render states
    if ( pDeviceState )
    {
        pDeviceState->Apply ( );
        pDeviceState->Release ( );
    }
    
    // End the scene that we started.
    pDevice->EndScene ();
    
    // Tidyup after last screenshot
    if ( ms_bSaveStarted )
    {
        // Done yet?
        if ( !CScreenShot::IsSaving () )
        {
            ms_pSaveLockSurface->UnlockRect ();
            ms_pSaveLockSurface->Release ();
            ms_pSaveLockSurface = NULL;
            ms_bSaveStarted = false;
        }
    }

    // Make a screenshot if needed
    if ( CCore::GetSingleton().bScreenShot && ms_bSaveStarted == false ) {

        // Max one screenshot per second
        if ( GetTickCount64_ () - ms_LastSaveTime < 1000 )
            return;
        ms_LastSaveTime = GetTickCount64_ ();

        RECT ScreenSize;

        IDirect3DSurface9 *pSurface = NULL;

        // Define a screen rectangle
        ScreenSize.top = ScreenSize.left = 0;
        ScreenSize.right = CDirect3DData::GetSingleton ().GetViewportWidth ();
        ScreenSize.bottom = CDirect3DData::GetSingleton ().GetViewportHeight ();        
        pDevice->GetRenderTarget ( 0, &pSurface );

        // Create a new render target
        if ( !pSurface || pDevice->CreateRenderTarget ( ScreenSize.right, ScreenSize.bottom, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &ms_pSaveLockSurface, NULL ) != D3D_OK ) {
            CCore::GetSingleton ().GetConsole ()->Printf("Couldn't create a new render target.");
        } else {
            unsigned long ulBeginTime = GetTickCount32 ();

            // Copy data from surface to surface
            if ( pDevice->StretchRect ( pSurface, &ScreenSize, ms_pSaveLockSurface, &ScreenSize, D3DTEXF_NONE ) != D3D_OK ) {
                CCore::GetSingleton ().GetConsole ()->Printf("Couldn't copy the surface.");
            }

            // Lock the surface
            D3DLOCKED_RECT LockedRect;
            if ( ms_pSaveLockSurface->LockRect ( &LockedRect, NULL, D3DLOCK_READONLY | D3DLOCK_NOSYSLOCK ) != D3D_OK ) {
                CCore::GetSingleton ().GetConsole ()->Printf("Couldn't lock the surface.");
            }

            // Call the pre-screenshot function 
            SString strFileName = CScreenShot::PreScreenShot ();

            // Start the save thread
            CScreenShot::BeginSave ( strFileName, LockedRect.pBits, LockedRect.Pitch, ScreenSize );
            ms_bSaveStarted = true;

            // Call the post-screenshot function
            CScreenShot::PostScreenShot ( strFileName );

            CCore::GetSingleton ().GetConsole ()->Printf ( "Screenshot capture took %.2f seconds.", (float)(GetTickCount32 () - ulBeginTime) / 1000.0f );
        }

        CCore::GetSingleton().bScreenShot = false;

        if ( pSurface )
        {
            if ( FAILED ( pSurface->Release () ) )
                std::runtime_error ( "Failed to release the ScreenShot rendertaget surface" );
            pSurface = NULL;
        }
    }
}


HRESULT CDirect3DEvents9::OnSetTexture ( IDirect3DDevice9 *pDevice, DWORD Stage, IDirect3DBaseTexture9* pTexture )
{
    // Save texture pointer for later
    if ( Stage < NUMELMS( ms_LastSetTextures ) )
        ms_LastSetTextures [ Stage ] = pTexture;

    return pDevice->SetTexture ( Stage, pTexture );
}


HRESULT CDirect3DEvents9::OnDrawIndexedPrimitive ( IDirect3DDevice9 *pDevice, D3DPRIMITIVETYPE PrimitiveType,INT BaseVertexIndex,UINT MinVertexIndex,UINT NumVertices,UINT startIndex,UINT primCount )
{
    // Any shader for this texture ?
    CShaderItem* pShaderItem = CGraphics::GetSingleton ().GetRenderItemManager ()->GetAppliedShaderForD3DData ( ms_LastSetTextures[0] );

    if ( !pShaderItem )
    {
        // No shader for this texture
        return pDevice->DrawIndexedPrimitive ( PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount );
    }
    else
    {
        // Yes shader for this texture
        CShaderInstance* pShaderInstance = pShaderItem->m_pShaderInstance;

        // Apply custom parameters
        pShaderInstance->ApplyShaderParameters ();
        // Apply common parameters
        pShaderInstance->m_pEffectWrap->ApplyCommonHandles ();

        // Do shader passes
        ID3DXEffect* pD3DEffect = pShaderInstance->m_pEffectWrap->m_pD3DEffect;

        DWORD dwFlags = 0;      // D3DXFX_DONOTSAVE(SHADER|SAMPLER)STATE
        uint uiNumPasses = 0;
        pD3DEffect->Begin ( &uiNumPasses, dwFlags );

        for ( uint uiPass = 0 ; uiPass < uiNumPasses ; uiPass++ )
        {
            pD3DEffect->BeginPass ( uiPass );
            pDevice->DrawIndexedPrimitive ( PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount );
            pD3DEffect->EndPass ();
        }
        pD3DEffect->End ();
        return D3D_OK;
    }
}
