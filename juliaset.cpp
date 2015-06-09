//-----------------------------------------------------------------------------
// Name: 3D Julia Set
// Author: Melih Elibol (Modified from 
// Last Modified: 06/11/05
// Description: Hardware accelerated 3D julia set using point sprites in Direct3D.
//-----------------------------------------------------------------------------

#define STRICT
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <string>
#include <mmsystem.h>
#include <d3d9.h>
#include <d3dx9.h>
#include "resource.h"
#include <algorithm>
#include <vector>

//-----------------------------------------------------------------------------
// GLOBALS
//-----------------------------------------------------------------------------
HWND g_hWnd = NULL;
LPDIRECT3D9 g_pD3D = NULL;
LPDIRECT3DDEVICE9 g_pd3dDevice = NULL;
LPDIRECT3DVERTEXBUFFER9 g_pVertexBuffer = NULL;
LPDIRECT3DTEXTURE9 g_pTexture = NULL;

float g_fSpinX = 0.0f;
float g_fSpinY = 0.0f;
float g_fSpinXB = 0.0f;
float g_fSpinYB = 0.0f;

struct Vertex {

    D3DXVECTOR3 posit;
    D3DCOLOR color;
    
    enum FVF {
        FVF_Flags = D3DFVF_XYZ|D3DFVF_DIFFUSE
    };

};

struct Particle {
    D3DXVECTOR3 m_vCurPosNrml;
    D3DXVECTOR3 m_vCurPos;
    D3DCOLOR m_vColor;
};

const int MAX_PARTICLES = 3*3*3*3*3*3;
const int MAX_MULTIPLE = 729;
const int NUM_PARTICLES = MAX_PARTICLES*MAX_MULTIPLE;
Particle g_particles[NUM_PARTICLES];

float unit = 0.2f;
int vWidth = pow((float)NUM_PARTICLES, 1.0f/3.0f);
int vHeight = NUM_PARTICLES/vWidth/vWidth; // as a function of vWidth and tLength
int vDepth = NUM_PARTICLES/vHeight/vHeight; // as a function of vWidth, vHeight and tLength

//-----------------------------------------------------------------------------
// PROTOYPES
//-----------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow);
LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void init(void);
void shutDown(void);
void render(void);

void initPointSprites(void);
void updatePointSprites(void);
void renderPointSprites(void);
void initUpdateVars(void);

// Helper function to stuff a FLOAT into a DWORD argument
inline DWORD FtoDW( FLOAT f ) { return *((DWORD*)&f); }

//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: The application's entry point
//-----------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    
    WNDCLASSEX winClass;
    MSG uMsg;
    
    memset(&uMsg,0,sizeof(uMsg));
    
    winClass.lpszClassName = "MY_WINDOWS_CLASS";
    winClass.cbSize = sizeof(WNDCLASSEX);
    winClass.style = CS_HREDRAW | CS_VREDRAW;
    winClass.lpfnWndProc = WindowProc;
    winClass.hInstance = hInstance;
    winClass.hIcon = LoadIcon(hInstance, (LPCTSTR)IDI_DIRECTX_ICON);
    winClass.hIconSm = LoadIcon(hInstance, (LPCTSTR)IDI_DIRECTX_ICON);
    winClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    winClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    winClass.lpszMenuName = NULL;
    winClass.cbClsExtra = 0;
    winClass.cbWndExtra = 0;
    
    if(!RegisterClassEx(&winClass))
        return E_FAIL;

    g_hWnd = CreateWindowEx(NULL, "MY_WINDOWS_CLASS",
                            "Direct3D (DX9) - Point Sprites",
                            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                            0, 0, 500, 500, NULL, NULL, hInstance, NULL);
    
    if(g_hWnd == NULL)
        return E_FAIL;
    
    ShowWindow( g_hWnd, nCmdShow );
    UpdateWindow( g_hWnd );

    init();
    initPointSprites();
    
    while( uMsg.message != WM_QUIT ){
        if( PeekMessage( &uMsg, NULL, 0, 0, PM_REMOVE ) ){
            TranslateMessage( &uMsg );
            DispatchMessage( &uMsg );
        } else {
            render();
        }
    }
    
    shutDown();
    UnregisterClass( "MY_WINDOWS_CLASS", winClass.hInstance );
    return uMsg.wParam;
}

//-----------------------------------------------------------------------------
// Name: WindowProc()
// Desc: The window's message handler
//-----------------------------------------------------------------------------
LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    
    static POINT ptLastMousePosit;
    static POINT ptCurrentMousePosit;
    static bool bMousing;
    
    switch(msg){
        case WM_KEYDOWN: {
            switch(wParam) {
                case VK_ESCAPE:
                    PostQuitMessage(0);
                    break;
            }
        }
        break;

        case WM_LBUTTONDOWN: {
            ptLastMousePosit.x = ptCurrentMousePosit.x = LOWORD (lParam);
            ptLastMousePosit.y = ptCurrentMousePosit.y = HIWORD (lParam);
            bMousing = true;
        }
        break;

        case WM_LBUTTONUP: {
            bMousing = false;
        }
        break;

        case WM_MOUSEMOVE: {
            ptCurrentMousePosit.x = LOWORD (lParam);
            ptCurrentMousePosit.y = HIWORD (lParam);
            if(bMousing) {
                g_fSpinX -= (ptCurrentMousePosit.x - ptLastMousePosit.x);
                g_fSpinY -= (ptCurrentMousePosit.y - ptLastMousePosit.y);
            }
            ptLastMousePosit.x = ptCurrentMousePosit.x;
            ptLastMousePosit.y = ptCurrentMousePosit.y;
        }
        break;

        case WM_CLOSE:{
            PostQuitMessage(0);
        }
        case WM_DESTROY: {
            PostQuitMessage(0);
        }
        break;

        default: {
            return DefWindowProc( hWnd, msg, wParam, lParam );
        }
        break;
        
    }
    return 0;
}

//-----------------------------------------------------------------------------
// Name: InitDirect3D()
// Desc: Initializes Direct3D
//-----------------------------------------------------------------------------
void init(void) {
    g_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    
    D3DDISPLAYMODE d3ddm;
    
    g_pD3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3ddm);

    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory(&d3dpp, sizeof(d3dpp));

    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = d3ddm.Format;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    
    g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, g_hWnd,
                         D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                         &d3dpp, &g_pd3dDevice);

    D3DXMATRIX matView;
    D3DXMatrixLookAtLH(&matView, &D3DXVECTOR3(0.0f, 0.0f, -40.0f),
                       &D3DXVECTOR3(0.0f, 0.0f, 0.0f),
                       &D3DXVECTOR3(0.0f, 1.0f, 0.0f));
    g_pd3dDevice->SetTransform(D3DTS_VIEW, &matView);

    D3DXMATRIX matProj;
    D3DXMatrixPerspectiveFovLH(&matProj, D3DXToRadian( 45.0f ), 1.0f, 1.0f, 300.0f);
    g_pd3dDevice->SetTransform(D3DTS_PROJECTION, &matProj);

    g_pd3dDevice->SetRenderState(D3DRS_LIGHTING,  FALSE);

}


//-----------------------------------------------------------------------------
// Name: shutDown()
// Desc: Releases all previously initialized objects
//-----------------------------------------------------------------------------
void shutDown( void )
{
    if( g_pVertexBuffer != NULL )
        g_pVertexBuffer->Release();

    if( g_pTexture != NULL )
        g_pTexture->Release();

    if( g_pd3dDevice != NULL )
        g_pd3dDevice->Release();

    if( g_pD3D != NULL )
        g_pD3D->Release();
}



//-----------------------------------------------------------------------------
//
// Particle Julia Set Imp.
//
//-----------------------------------------------------------------------------

bool oscDir = true;
double oscVel = 0.0f;
double lastOscVel = 0.0f;
double oscPos = -2.0f;

const double rangeMin = -3.0f;
const double rangeMax = 3.0f;
const double range[6] = {rangeMin, rangeMax, rangeMin, rangeMax, rangeMin, rangeMax};
//const double range[6] = {-2.0f, 2.0f, -2.0f, 2.0f, -2.0f, 2.0f};
const double fullRange[3] = {range[1]-range[0], range[3]-range[2], range[5]-range[4]};

double a, b, c, d;
double temp;
double abcdOrder[4] = {0.0f, 0.0f, 0.0f, 0.0f};

float px, py, pz;
std::vector<int> perm;
double loopItr = -1, maxItr = 64;

//color
const double split = 1.0f/3.0f;
double dst[4] = {0.0f, 0.0f, 0.0f, 0.0f};
int order[3] = {0,1,2};
double rng;
int index;

const long factorial(const long n) {
    long fn=1;
    if(n>1)
        fn=(n*factorial(n-1));
    return fn;
}

void initPointSprites(void) {
    // Load up the point sprite's texture...
    D3DXCreateTextureFromFile( g_pd3dDevice, "particle.bmp", &g_pTexture );

    g_pd3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    g_pd3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

    // Create a vertex bufer which can be used with point sprites...
    g_pd3dDevice->CreateVertexBuffer( 2048 * sizeof(Vertex),
                                      D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY | D3DUSAGE_POINTS,
                                      Vertex::FVF_Flags, D3DPOOL_DEFAULT,
                                      &g_pVertexBuffer, NULL );

    // If you want to know the max size that a point sprite can be set to,
    // and whether or not you can change the size of point sprites in hardware
    // by sending D3DFVF_PSIZE with the FVF, do this.
    float fMaxPointSize = 0.0f;
    bool  bDeviceSupportsPSIZE = false;

    D3DCAPS9 d3dCaps;
    g_pd3dDevice->GetDeviceCaps( &d3dCaps );

    fMaxPointSize = d3dCaps.MaxPointSize;

    if( d3dCaps.FVFCaps & D3DFVFCAPS_PSIZE )
        bDeviceSupportsPSIZE = true;
    else
        bDeviceSupportsPSIZE = false;

    int linearPos;
    for( int i = 0; i < NUM_PARTICLES; ++i ){

        linearPos = i;

        //compute from 0,0
        px = (linearPos%vWidth)
            , py = ((int)(linearPos/vWidth)%vHeight)
            , pz = ((int)(linearPos/vHeight/vHeight)%vDepth);

        g_particles[i].m_vCurPosNrml = D3DXVECTOR3(px/vWidth*fullRange[0]+range[0]
                                                   , py/vHeight*fullRange[1]+range[2]
                                                   , pz/vDepth*fullRange[2]+range[4]
                                                   );
        
        //center, uniform transformation for rotation axis
        px = px - (vWidth*0.5f);
        py = py - (vHeight*0.5f);
        pz = pz - (vDepth*0.5f);

        g_particles[i].m_vCurPos = D3DXVECTOR3(px*unit, py*unit, pz*unit);
        g_particles[i].m_vColor = D3DCOLOR_COLORVALUE(1.0f, 1.0f, 0.0f, 1.0f);
    }

    initUpdateVars();
}

void initUpdateVars( void ){

    perm.push_back(0), perm.push_back(1), perm.push_back(2), perm.push_back(3);
    sort(perm.begin(), perm.end());
    int permMax = factorial(4);
    int permPos = 0;

    while(--permPos > -1){
        next_permutation(perm.begin(), perm.end());
    }

}

//-----------------------------------------------------------------------------
// Name: updatePointSprites()
// Desc:
//-----------------------------------------------------------------------------
void updatePointSprites( void ) {

    lastOscVel = oscVel;
    if(oscDir){
        oscVel += .0005;
        oscPos += oscVel;
        if(oscPos > 0){
            oscDir = !oscDir;
        }
    } else {
        oscVel -= .0005;
        oscPos += oscVel;
        if(oscPos < 0) {
            oscDir = !oscDir;
        }
    }

    if(oscVel < 0 && lastOscVel > 0 || oscVel > 0 && lastOscVel < 0){
        //rotate
        order[0] ^= order[1];
        order[1] ^= order[0];
        order[0] ^= order[1];
        order[1] ^= order[2];
        order[2] ^= order[1];
        order[1] ^= order[2];
    }

    for( int i = 0; i < NUM_PARTICLES; ++i ){
        /*
        a = oscPos;
        b = g_particles[i].m_vCurPosNrml.z;
        c = g_particles[i].m_vCurPosNrml.x;
        d = g_particles[i].m_vCurPosNrml.y;

        loopItr = -1;
        while(++loopItr<maxItr && a*a + b*b < 8.0){
            temp = a*a - b*b + c;
            b = 2.0*a*b + d;
            a = temp;
        }

        rng = loopItr/maxItr;
        dst[0] = 0.0f, dst[1] = 0.0f, dst[2] = 0.0f, dst[3] = 0.0f;
        dst[3] = rng;
        if(rng > split){
            dst[0] = 1.0f*dst[3];
            rng -= split;
        } else {
            dst[0] = rng*dst[3];
            rng = 0;
        }
        if(rng > split){
            dst[1] = 1.0f*dst[3];
            rng -= split;
        } else {
            dst[1] = rng*dst[3];
            rng = 0;
        }
        if(rng > split){
            dst[2] = 1.0f*dst[3];
            rng -= split;
        } else {
            dst[2] = rng*dst[3];
            rng = 0;
        }
        g_particles[i].m_vColor = D3DCOLOR_COLORVALUE(dst[0], dst[1], dst[2], 1.0f);
        //*/

        ///*
        abcdOrder[0] = g_particles[i].m_vCurPosNrml.x;
        abcdOrder[1] = g_particles[i].m_vCurPosNrml.y;
        abcdOrder[2] = g_particles[i].m_vCurPosNrml.z;
        abcdOrder[3] = oscPos; //w
        a = abcdOrder[3], b = abcdOrder[2], c = abcdOrder[0], d = abcdOrder[1];

        loopItr = -1;
        while(++loopItr<maxItr && a*a + b*b < 8.0){
            temp = a*a - b*b + c;
            b = 2.0*a*b + d;
            a = temp;
        }

        index = 0;

        rng = loopItr/maxItr;
        dst[0] = 0.0f, dst[1] = 0.0f, dst[2] = 0.0f, dst[3] = 0.0f;
        //dst[3] = (loopItr/maxItr)*(1.0-rng*.9884);
        dst[3] = rng;//.013+.997*loopItr*(maxItr-loopItr)/pow(maxItr, 2);

        while(rng > split){
            dst[order[index]] = 1.0f*dst[3];
            rng -= split;
            index++;
        }
        dst[order[index]] = rng*3.0f*dst[3];
        //dst[order[0]] = dst[order[0]] - (dst[order[1]] + dst[order[2]])*.5;
        //dst[order[1]] = dst[order[1]] - dst[order[2]];
        //dst[order[0]] = dst[order[0]] - dst[order[2]];
        //dst[order[1]] = dst[order[1]] - dst[order[2]];
        //dst[order[0]] *= .1;
        dst[order[0]] = dst[order[0]] - dst[order[1]];
        dst[order[1]] = dst[order[1]] - dst[order[2]];
        dst[order[0]] *= .5;
        dst[order[2]] *= .02;

        g_particles[i].m_vColor = D3DCOLOR_COLORVALUE(dst[0], dst[1], dst[2], 1.0f);
        //*/
    }

}

//-----------------------------------------------------------------------------
// Name: renderPointSprites()
// Desc:
//-----------------------------------------------------------------------------
void renderPointSprites(int startIndex, int endIndex) {
    // Setting D3DRS_ZWRITEENABLE to FALSE makes the Z-Buffer read-only, which
    // helps remove graphical artifacts generated from  rendering a list of
    // particles that haven't been sorted by distance to the eye.
    //
    // Setting D3DRS_ALPHABLENDENABLE to TRUE allows particles, which overlap,
    // to alpha blend with each other correctly.

    g_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );

    g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    g_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );

    // Set up the render states for using point sprites...
    g_pd3dDevice->SetRenderState( D3DRS_POINTSPRITEENABLE, TRUE ); // Turn on point sprites
    g_pd3dDevice->SetRenderState( D3DRS_POINTSCALEENABLE, TRUE ); // Allow sprites to be scaled with distance
    g_pd3dDevice->SetRenderState( D3DRS_POINTSIZE, FtoDW(1.0) ); // Float value that specifies the size to use for point size computation in cases where point size is not specified for each vertex.
    g_pd3dDevice->SetRenderState( D3DRS_POINTSIZE_MIN, FtoDW(1.0f) ); // Float value that specifies the minimum size of point primitives. Point primitives are clamped to this size during rendering.
    g_pd3dDevice->SetRenderState( D3DRS_POINTSCALE_A, FtoDW(0.0f) ); // Default 1.0
    g_pd3dDevice->SetRenderState( D3DRS_POINTSCALE_B, FtoDW(0.0f) ); // Default 0.0
    g_pd3dDevice->SetRenderState( D3DRS_POINTSCALE_C, FtoDW(1.0f) ); // Default 0.0

    // Lock the vertex buffer, and set up our point sprites in accordance with
    // our particles that we're keeping track of in our application.

    Vertex *pPointVertices;

    g_pVertexBuffer->Lock(0, MAX_PARTICLES * sizeof(Vertex),
                          (void**)&pPointVertices, D3DLOCK_DISCARD);
    
    for(int i = startIndex-1; ++i < endIndex;) {
        pPointVertices->posit = g_particles[i].m_vCurPos;
        pPointVertices->color = g_particles[i].m_vColor;
        pPointVertices++;
    }
    
    g_pVertexBuffer->Unlock();

    // Render point sprites...

    g_pd3dDevice->SetStreamSource(0, g_pVertexBuffer, 0, sizeof(Vertex));
    g_pd3dDevice->SetFVF(Vertex::FVF_Flags);
    g_pd3dDevice->DrawPrimitive(D3DPT_POINTLIST, 0, MAX_PARTICLES);

    // Reset render states...

    g_pd3dDevice->SetRenderState(D3DRS_POINTSPRITEENABLE, FALSE);
    g_pd3dDevice->SetRenderState(D3DRS_POINTSCALEENABLE, FALSE);

    g_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
}

//-----------------------------------------------------------------------------
// Name: render()
// Desc: render the scene
//-----------------------------------------------------------------------------
void render(void) {
    g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_COLORVALUE(0.0f,0.0f,0.0f,1.0f), 1.0f, 0);

    D3DXMATRIX matTrans;
    D3DXMATRIX matRot;
    D3DXMATRIX matWorld;

    D3DXMatrixTranslation( &matTrans, 0.0f, 0.0f, -20.0f );

    g_fSpinXB += (g_fSpinX-g_fSpinXB)*.1;
    g_fSpinYB += (g_fSpinY-g_fSpinYB)*.1;

    D3DXMatrixRotationYawPitchRoll( &matRot,
                                    D3DXToRadian(g_fSpinXB),
                                    D3DXToRadian(g_fSpinYB),
                                    0.0f );
    matWorld = matRot * matTrans;
    g_pd3dDevice->SetTransform(D3DTS_WORLD, &matWorld);

    updatePointSprites();

    g_pd3dDevice->BeginScene();

    g_pd3dDevice->SetTexture(0, g_pTexture);

    for(int i = -1;++i<MAX_MULTIPLE;){
        renderPointSprites(i*MAX_PARTICLES, (i+1)*MAX_PARTICLES);
    }

    g_pd3dDevice->EndScene();
    g_pd3dDevice->Present(NULL, NULL, NULL, NULL);
}
