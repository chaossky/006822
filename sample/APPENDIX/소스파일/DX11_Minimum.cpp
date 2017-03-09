//------------------------------------------------------------
// DX11_Minimum.cpp
// DirectX11 미니 프로그램
// Programmed by K.Kato.
// 
//------------------------------------------------------------

#include <D3D11.h>
#include <D3DX11.h>
#include <D3Dcompiler.h>
#include <xnamath.h>
#include <tchar.h>								// Unicode・멀티바이트 문자관계

// 링크 라이브러리
#pragma comment( lib, "d3d11.lib" )				// D3D11 라이브러리
#pragma comment( lib, "d3dx11.lib" )
#pragma comment( lib, "winmm.lib" )

#define VIEW_WIDTH			800					// 화면너비
#define VIEW_HEIGHT			600					// 화면높이

#define OBJECT_SIZE			1.5f				// 물체 크기

// 세이프 릴리스 매크로
#ifndef SAFE_RELEASE
#define SAFE_RELEASE( p )      { if ( p ) { ( p )->Release(); ( p )=NULL; } }
#endif


// 정점 구조체
struct CUSTOMVERTEX {
	XMFLOAT4	v4Pos;
	XMFLOAT2	v2UV;
};

// 셰이더 상수 구조체
struct CBNeverChanges
{
	XMMATRIX mView;
};


// 글로벌 변수
UINT  g_nClientWidth;							// 그릴 영역 너비
UINT  g_nClientHeight;							// 그릴 영역 높이

HWND        g_hWnd;         // 윈도우 핸들


ID3D11Device			*g_pd3dDevice;			// 디바이스
IDXGISwapChain			*g_pSwapChain;			// DXGI스왑체인
ID3D11DeviceContext		*g_pImmediateContext;	// 디바이스 컨텍스트
ID3D11RasterizerState	*g_pRS;					// 래스터라이저
ID3D11RenderTargetView	*g_pRTV;				// 렌더링 타깃
ID3D11Texture2D*        g_pDepthStencil = NULL;	// Z버퍼
ID3D11DepthStencilView* g_pDepthStencilView = NULL;	// Z버퍼 뷰
D3D_FEATURE_LEVEL       g_FeatureLevel;			// 피처레벨

ID3D11Buffer			*g_pVertexBuffer;		// 정점 버퍼
ID3D11Buffer			*g_pIndexBuffer;		// 인덱스 버퍼
ID3D11BlendState		*g_pbsAlphaBlend;		// 알파블렌드
ID3D11VertexShader		*g_pVertexShader;		// 정점셰이더
ID3D11PixelShader		*g_pPixelShader;		// 픽셀 셰이더
ID3D11InputLayout		*g_pInputLayout;		// 셰이더입력레이아웃
ID3D11SamplerState		*g_pSamplerState;		// 샘플러 스테이트

ID3D11Buffer			*g_pCBNeverChanges = NULL;

// 현재 텍스처
ID3D11ShaderResourceView	*g_pNowTexture = NULL;


// Direct3D 초기화
HRESULT InitD3D(void)
{
	HRESULT hr = S_OK;
	D3D_FEATURE_LEVEL  FeatureLevelsRequested[6] = { D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_3,
		D3D_FEATURE_LEVEL_9_2,
		D3D_FEATURE_LEVEL_9_1 };
	UINT               numLevelsRequested = 6;
	D3D_FEATURE_LEVEL  FeatureLevelsSupported;

	// 디바이스 생성
	hr = D3D11CreateDevice(NULL,
		D3D_DRIVER_TYPE_HARDWARE,
		NULL,
		0,
		FeatureLevelsRequested,
		numLevelsRequested,
		D3D11_SDK_VERSION,
		&g_pd3dDevice,
		&FeatureLevelsSupported,
		&g_pImmediateContext);
	if (FAILED(hr)) {
		return hr;
	}

	// 팩토리 취득
	IDXGIDevice * pDXGIDevice;
	hr = g_pd3dDevice->QueryInterface(__uuidof(IDXGIDevice), (void **)&pDXGIDevice);
	IDXGIAdapter * pDXGIAdapter;
	hr = pDXGIDevice->GetParent(__uuidof(IDXGIAdapter), (void **)&pDXGIAdapter);
	IDXGIFactory * pIDXGIFactory;
	pDXGIAdapter->GetParent(__uuidof(IDXGIFactory), (void **)&pIDXGIFactory);

	// 스왑체인 생성
	DXGI_SWAP_CHAIN_DESC	sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 1;
	sd.BufferDesc.Width = g_nClientWidth;
	sd.BufferDesc.Height = g_nClientHeight;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = g_hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;
	hr = pIDXGIFactory->CreateSwapChain(g_pd3dDevice, &sd, &g_pSwapChain);

	pDXGIDevice->Release();
	pDXGIAdapter->Release();
	pIDXGIFactory->Release();

	if (FAILED(hr)) {
		return hr;
	}

	// 렌더링 타깃 생성
	ID3D11Texture2D			*pBackBuffer = NULL;
	D3D11_TEXTURE2D_DESC BackBufferSurfaceDesc;
	hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
	if (FAILED(hr)) {
		MessageBox(NULL, _T("Can't get backbuffer."), _T("Error"), MB_OK);
		return hr;
	}
	pBackBuffer->GetDesc(&BackBufferSurfaceDesc);
	hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_pRTV);
	SAFE_RELEASE(pBackBuffer);
	if (FAILED(hr)) {
		MessageBox(NULL, _T("Can't create render target view."), _T("Error"), MB_OK);
		return hr;
	}

	// *** Create depth stencil texture ***
	D3D11_TEXTURE2D_DESC descDepth;
	RECT rc;
	GetClientRect(g_hWnd, &rc);
	ZeroMemory(&descDepth, sizeof(descDepth));
	descDepth.Width = rc.right - rc.left;
	descDepth.Height = rc.bottom - rc.top;
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	descDepth.SampleDesc.Count = 1;
	descDepth.SampleDesc.Quality = 0;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;
	hr = g_pd3dDevice->CreateTexture2D(&descDepth, NULL, &g_pDepthStencil);
	if (FAILED(hr))
		return hr;

	// *** Create the depth stencil view ***
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
	ZeroMemory(&descDSV, sizeof(descDSV));
	descDSV.Format = descDepth.Format;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;
	hr = g_pd3dDevice->CreateDepthStencilView(g_pDepthStencil, &descDSV, &g_pDepthStencilView);
	if (FAILED(hr))
		return hr;

	// *** 렌더링 타깃 설정 ***
	g_pImmediateContext->OMSetRenderTargets(1, &g_pRTV, g_pDepthStencilView);

	// 래스터라이저 설정
	D3D11_RASTERIZER_DESC drd;
	ZeroMemory(&drd, sizeof(drd));
	drd.FillMode = D3D11_FILL_SOLID;
	drd.CullMode = D3D11_CULL_NONE;				// 컬링없음
	drd.FrontCounterClockwise = FALSE;
	drd.DepthClipEnable = TRUE;
	hr = g_pd3dDevice->CreateRasterizerState(&drd, &g_pRS);
	if (FAILED(hr)) {
		MessageBox(NULL, _T("Can't create rasterizer state."), _T("Error"), MB_OK);
		return hr;
	}
	g_pImmediateContext->RSSetState(g_pRS);

	// 뷰포트 설정
	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)g_nClientWidth;
	vp.Height = (FLOAT)g_nClientHeight;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0.0f;
	vp.TopLeftY = 0.0f;
	g_pImmediateContext->RSSetViewports(1, &vp);

	return S_OK;
}


// 프로그래머블 셰이더 작성
HRESULT MakeShaders(void)
{
	HRESULT hr;
	ID3DBlob* pVertexShaderBuffer = NULL;
	ID3DBlob* pPixelShaderBuffer = NULL;
	ID3DBlob* pError = NULL;

	DWORD dwShaderFlags = 0;
#ifdef _DEBUG
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif
	// 컴파일
	hr = D3DX11CompileFromFile(_T("Basic_3D_Tex.fx"), NULL, NULL, "VS", "vs_4_0_level_9_1",
		dwShaderFlags, 0, NULL, &pVertexShaderBuffer, &pError, NULL);
	if (FAILED(hr)) {
		MessageBox(NULL, _T("Can't open Basic_3D_Tex.fx"), _T("Error"), MB_OK);
		SAFE_RELEASE(pError);
		return hr;
	}
	hr = D3DX11CompileFromFile(_T("Basic_3D_Tex.fx"), NULL, NULL, "PS", "ps_4_0_level_9_1",
		dwShaderFlags, 0, NULL, &pPixelShaderBuffer, &pError, NULL);
	if (FAILED(hr)) {
		SAFE_RELEASE(pVertexShaderBuffer);
		SAFE_RELEASE(pError);
		return hr;
	}
	SAFE_RELEASE(pError);

	// VertexShader 생성
	hr = g_pd3dDevice->CreateVertexShader(pVertexShaderBuffer->GetBufferPointer(),
		pVertexShaderBuffer->GetBufferSize(),
		NULL, &g_pVertexShader);
	if (FAILED(hr)) {
		SAFE_RELEASE(pVertexShaderBuffer);
		SAFE_RELEASE(pPixelShaderBuffer);
		return hr;
	}
	// PixelShader 생성
	hr = g_pd3dDevice->CreatePixelShader(pPixelShaderBuffer->GetBufferPointer(),
		pPixelShaderBuffer->GetBufferSize(),
		NULL, &g_pPixelShader);
	if (FAILED(hr)) {
		SAFE_RELEASE(pVertexShaderBuffer);
		SAFE_RELEASE(pPixelShaderBuffer);
		return hr;
	}

	// 입력버퍼 입력형식
	D3D11_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXTURE",  0, DXGI_FORMAT_R32G32_FLOAT,       0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	UINT numElements = ARRAYSIZE(layout);
	// 입력버퍼 입력형식 생성
	hr = g_pd3dDevice->CreateInputLayout(layout, numElements,
		pVertexShaderBuffer->GetBufferPointer(),
		pVertexShaderBuffer->GetBufferSize(),
		&g_pInputLayout);
	SAFE_RELEASE(pVertexShaderBuffer);
	SAFE_RELEASE(pPixelShaderBuffer);
	if (FAILED(hr)) {
		return hr;
	}

	// 셰이더 상수 버퍼 생성
	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(CBNeverChanges);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	hr = g_pd3dDevice->CreateBuffer(&bd, NULL, &g_pCBNeverChanges);
	if (FAILED(hr))
		return hr;

	// 변환행렬
	CBNeverChanges	cbNeverChanges;
	XMMATRIX		mScreen;
	mScreen = XMMatrixIdentity();
	mScreen._11 = 2.0f / g_nClientWidth;
	mScreen._22 = -2.0f / g_nClientHeight;
	mScreen._41 = -1.0f;
	mScreen._42 = 1.0f;
	cbNeverChanges.mView = XMMatrixTranspose(mScreen);
	g_pImmediateContext->UpdateSubresource(g_pCBNeverChanges, 0, NULL, &cbNeverChanges, 0, 0);

	return S_OK;
}


// 드로우 모드 오브젝트 초기화
int InitDrawModes(void)
{
	HRESULT				hr;

	// 블렌드 스테이트
	D3D11_BLEND_DESC BlendDesc;
	BlendDesc.AlphaToCoverageEnable = FALSE;
	BlendDesc.IndependentBlendEnable = FALSE;
	BlendDesc.RenderTarget[0].BlendEnable = TRUE;
	BlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	BlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	BlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	BlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	BlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	BlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = g_pd3dDevice->CreateBlendState(&BlendDesc, &g_pbsAlphaBlend);
	if (FAILED(hr)) {
		return hr;
	}

	// 샘플러
	D3D11_SAMPLER_DESC samDesc;
	ZeroMemory(&samDesc, sizeof(samDesc));
	samDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	samDesc.MaxLOD = D3D11_FLOAT32_MAX;
	hr = g_pd3dDevice->CreateSamplerState(&samDesc, &g_pSamplerState);
	if (FAILED(hr)) {
		return hr;
	}

	return S_OK;
}


// 지오메트리 초기화
HRESULT InitGeometry(void)
{
	HRESULT hr = S_OK;
	// 정점 데이터
	CUSTOMVERTEX		cvVertices[4] = { { XMFLOAT4(0.0f,        -OBJECT_SIZE,  -OBJECT_SIZE, 1.0f),
		XMFLOAT2(0.0f,  0.0f) },
		{ XMFLOAT4(OBJECT_SIZE, OBJECT_SIZE,          0.0f, 1.0f),
		XMFLOAT2(1.0f,  0.0f) },
		{ XMFLOAT4(-OBJECT_SIZE, OBJECT_SIZE,          0.0f, 1.0f),
		XMFLOAT2(0.0f,  1.0f) },
		{ XMFLOAT4(0.0f,        -OBJECT_SIZE,   OBJECT_SIZE, 1.0f),
		XMFLOAT2(1.0f,  1.0f) }
	};
	// 인덱스 데이터
	WORD					wIndices[6] = { 0, 1, 2,
		2, 1, 3, };


	// 정점 버퍼 생성
	D3D11_BUFFER_DESC BufferDesc;
	BufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	BufferDesc.ByteWidth = sizeof(CUSTOMVERTEX) * 4;
	BufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	BufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	BufferDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA SubResourceData;
	SubResourceData.pSysMem = cvVertices;
	SubResourceData.SysMemPitch = 0;
	SubResourceData.SysMemSlicePitch = 0;
	hr = g_pd3dDevice->CreateBuffer(&BufferDesc, &SubResourceData, &g_pVertexBuffer);
	if (FAILED(hr)) {
		return hr;
	}

	// 인덱스 버퍼 생성
	BufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	BufferDesc.ByteWidth = sizeof(WORD) * 6;
	BufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	BufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	BufferDesc.MiscFlags = 0;

	SubResourceData.pSysMem = wIndices;
	hr = g_pd3dDevice->CreateBuffer(&BufferDesc, &SubResourceData, &g_pIndexBuffer);
	if (FAILED(hr))
		return hr;

	// 텍스처 생성
	D3DX11_IMAGE_LOAD_INFO		liLoadInfo;
	ID3D11Texture2D				*pTexture;

	ZeroMemory(&liLoadInfo, sizeof(D3DX11_IMAGE_LOAD_INFO));
	liLoadInfo.Width = 512;
	liLoadInfo.Height = 512;
	liLoadInfo.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	liLoadInfo.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	hr = D3DX11CreateShaderResourceViewFromFile(g_pd3dDevice, _T("3.bmp"), &liLoadInfo,
		NULL, &g_pNowTexture, NULL);
	if (FAILED(hr)) {
		MessageBox(NULL, _T("Can't open 3.bmp"), _T("Error"), MB_OK);
		return hr;
	}

	return S_OK;
}


// 종료처리
int Cleanup(void)
{
	SAFE_RELEASE(g_pSamplerState);
	SAFE_RELEASE(g_pbsAlphaBlend);
	SAFE_RELEASE(g_pInputLayout);
	SAFE_RELEASE(g_pPixelShader);
	SAFE_RELEASE(g_pVertexShader);
	SAFE_RELEASE(g_pCBNeverChanges);

	SAFE_RELEASE(g_pRS);									// 래스터라이저

															// 스테이터스 클리어
	if (g_pImmediateContext) {
		g_pImmediateContext->ClearState();
		g_pImmediateContext->Flush();
	}

	SAFE_RELEASE(g_pRTV);									// 렌더링 타깃
	SAFE_RELEASE(g_pDepthStencil);						// Z버퍼
	SAFE_RELEASE(g_pDepthStencilView);					// Z버퍼 뷰

														// 스왑체인
	if (g_pSwapChain != NULL) {
		g_pSwapChain->SetFullscreenState(FALSE, 0);
	}
	SAFE_RELEASE(g_pSwapChain);

	SAFE_RELEASE(g_pImmediateContext);					// 디바이스 컨텍스트
	SAFE_RELEASE(g_pd3dDevice);							// 디바이스

	return 0;
}


// 윈도우 프로시저
LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}


// 변환행렬 설정
HRESULT SetupMatrices(void)
{
	CBNeverChanges	cbNeverChanges;
	XMMATRIX		mWorld;
	XMMATRIX		mView;
	XMMATRIX		mProjection;

	// Initialize the world matrix
	mWorld = XMMatrixRotationY(2.0f * 3.1415f * (float)(timeGetTime() % 2000) / 2000.0f);

	// Initialize the view matrix
	XMVECTOR Eye = XMVectorSet(0.0f, 3.0f, -5.0f, 0.0f);
	XMVECTOR At = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	mView = XMMatrixLookAtLH(Eye, At, Up);

	// Initialize the projection matrix
	mProjection = XMMatrixPerspectiveFovLH(XM_PIDIV4, VIEW_WIDTH / (FLOAT)VIEW_HEIGHT, 0.01f, 100.0f);

	cbNeverChanges.mView = XMMatrixTranspose(mWorld * mView * mProjection);
	g_pImmediateContext->UpdateSubresource(g_pCBNeverChanges, 0, NULL, &cbNeverChanges, 0, 0);

	return S_OK;
}


// 렌더링 
HRESULT Render(void)
{
	// 화면 클리어
	XMFLOAT4	v4Color = XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);
	g_pImmediateContext->ClearRenderTargetView(g_pRTV, (float *)&v4Color);
	// *** Z버퍼 클리어 ***
	g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

	// 샘플러・래스터라이저 설정
	g_pImmediateContext->PSSetSamplers(0, 1, &g_pSamplerState);
	g_pImmediateContext->RSSetState(g_pRS);

	// 그리기 설정
	UINT nStrides = sizeof(CUSTOMVERTEX);
	UINT nOffsets = 0;
	g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &nStrides, &nOffsets);
	g_pImmediateContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
	g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	g_pImmediateContext->IASetInputLayout(g_pInputLayout);

	// 셰이더설정
	g_pImmediateContext->VSSetShader(g_pVertexShader, NULL, 0);
	g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pCBNeverChanges);
	g_pImmediateContext->PSSetShader(g_pPixelShader, NULL, 0);

	// 변환행렬
	SetupMatrices();

	// 그리기
	g_pImmediateContext->OMSetBlendState(NULL, NULL, 0xFFFFFFFF);
	g_pImmediateContext->PSSetShaderResources(0, 1, &g_pNowTexture);
	g_pImmediateContext->DrawIndexed(6, 0, 0);

	return S_OK;
}


// 엔트리포인트
int WINAPI _tWinMain(HINSTANCE hInst, HINSTANCE, LPTSTR, int)
{
	LARGE_INTEGER			nNowTime, nLastTime;		// 현재와 직전시각
	LARGE_INTEGER			nTimeFreq;					// 시간단위

														// 화면크기
	g_nClientWidth = VIEW_WIDTH;						// 너비
	g_nClientHeight = VIEW_HEIGHT;						// 높이

														// Register the window class
	WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, MsgProc, 0L, 0L,
		GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
		_T("D3D Sample"), NULL };
	RegisterClassEx(&wc);

	RECT rcRect;
	SetRect(&rcRect, 0, 0, g_nClientWidth, g_nClientHeight);
	AdjustWindowRect(&rcRect, WS_OVERLAPPEDWINDOW, FALSE);
	g_hWnd = CreateWindow(_T("D3D Sample"), _T("DX11_Minimum"),
		WS_OVERLAPPEDWINDOW, 100, 20, rcRect.right - rcRect.left, rcRect.bottom - rcRect.top,
		GetDesktopWindow(), NULL, wc.hInstance, NULL);

	// Initialize Direct3D
	if (SUCCEEDED(InitD3D()) && SUCCEEDED(MakeShaders()))
	{
		// Create the shaders
		if (SUCCEEDED(InitDrawModes()))
		{
			if (SUCCEEDED(InitGeometry())) {					// 지오메트리 작성

																// Show the window
				ShowWindow(g_hWnd, SW_SHOWDEFAULT);
				UpdateWindow(g_hWnd);

				QueryPerformanceFrequency(&nTimeFreq);			// 시간단위
				QueryPerformanceCounter(&nLastTime);				// 1프레임전 시각 초기화

																	// Enter the message loop
				MSG msg;
				ZeroMemory(&msg, sizeof(msg));
				while (msg.message != WM_QUIT)
				{
					Render();
					do {
						if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
						{
							TranslateMessage(&msg);
							DispatchMessage(&msg);
						}
						QueryPerformanceCounter(&nNowTime);
					} while (((nNowTime.QuadPart - nLastTime.QuadPart) < (nTimeFreq.QuadPart / 90)) &&
						(msg.message != WM_QUIT));
					while (((nNowTime.QuadPart - nLastTime.QuadPart) < (nTimeFreq.QuadPart / 60)) &&
						(msg.message != WM_QUIT))
					{
						QueryPerformanceCounter(&nNowTime);
					}
					nLastTime = nNowTime;
					g_pSwapChain->Present(0, 0);					// 표시
				}
			}
		}
	}

	// Clean up everything and exit the app
	Cleanup();
	UnregisterClass(_T("D3D Sample"), wc.hInstance);
	return 0;
}
