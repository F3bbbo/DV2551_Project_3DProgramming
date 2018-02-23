#include <SDL_events.h>
#include "Directx12Renderer.h"
#include "MaterialDX12.h"
#include "VertexBufferDX12.h"
#include "Texture2DDX12.h"
#include "RenderStateDX12.h"
#include "TechniqueDX12.h"
#include "MeshDX12.h"
#include "d3dUtil.h"
#include "pch.h"
#include "Sampler2DDX12.h"

DirectX12Renderer::DirectX12Renderer()
{
	createDevice();
	createFenceAndDescriptorSizes();
	createDescriptorHeaps();
	createCommandObject();
	pipelineState.setDevice(device);
	pipelineState.setCommandList(commandList);
	Root.setCommandList(commandList);
	Root.CreateRootsignature(getDevice());
	Root.bindRootSignature();

}

DirectX12Renderer::~DirectX12Renderer()
{
	shutdown();
}

Material * DirectX12Renderer::makeMaterial(const std::string & name)
{
	return new MaterialDX12(name, commandList,getDevice(), getShaderPath(), &Root);
}

Mesh * DirectX12Renderer::makeMesh()
{
	
	return new MeshDX12();
}

VertexBuffer * DirectX12Renderer::makeVertexBuffer(size_t size, VertexBuffer::DATA_USAGE usage)
{
	VertexBufferDX12* ptr = new VertexBufferDX12(device.Get(), commandList, &Root);
	ptr->createBuffer(device.Get(), size);
	return ptr;
}

Texture2D * DirectX12Renderer::makeTexture2D()
{	
	Texture2DDX12* texture = new Texture2DDX12(getDevice().Get(), commandList.Get(), &Root);
	commandList->Close();
	executeCommandList(); // To transform the texture into a shader resource
	waitForGPU();
	commandList->Reset(commandAllocator.Get(), nullptr);
	return texture;
}

Sampler2D * DirectX12Renderer::makeSampler2D()
{
	return new Sampler2DDX12();
}

RenderState * DirectX12Renderer::makeRenderState()
{
	return new RenderStateDX12();
}

std::string DirectX12Renderer::getShaderPath()
{
	return std::string("..\\assets\\DX5\\");
}

std::string DirectX12Renderer::getShaderExtension()
{
	return std::string(".hlsl");
}

ConstantBuffer * DirectX12Renderer::makeConstantBuffer(std::string NAME, unsigned int location)
{
	return new ConstantBufferDX12(device.Get(), NAME, location, commandList, &Root);
}

Technique * DirectX12Renderer::makeTechnique(Material *m, RenderState *r)
{
	return new TechniqueDX12(m, r);
}

HWND DirectX12Renderer::InitWindow(HINSTANCE hInstance,int width,int height)
{

	this->width = width;
	this->height = height;

		WNDCLASSEX wcex = { 0 };
		wcex.cbSize = sizeof(WNDCLASSEX);
		wcex.lpfnWndProc = WndProc;
		wcex.hInstance = hInstance;
		wcex.lpszClassName = L"BTH_D3D_12_DEMO";
		if (!RegisterClassEx(&wcex))
		{
			return false;
		}

		RECT rc = { 0, 0, width,height };
		AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, false);

		return CreateWindowEx(
			WS_EX_OVERLAPPEDWINDOW,
			L"BTH_D3D_12_DEMO",
			L"BTH Direct3D 12 Demo",
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			rc.right - rc.left,
			rc.bottom - rc.top,
			nullptr,
			nullptr,
			hInstance,
			nullptr);
}

LRESULT WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	SDL_Event sdlEvent;
	switch (message)
	{
	case WM_DESTROY:
		sdlEvent.type = SDL_QUIT;
		SDL_PushEvent(&sdlEvent);
		PostQuitMessage(0);
		break;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);

}

int DirectX12Renderer::initialize(unsigned int width, unsigned int height)
{
	msg = { 0 };
	wndHandle =  InitWindow(GetModuleHandle(NULL),width,height);
	createSwapChain(wndHandle);
	createRTV();
	createDepthStencil();
	createViewPortScissor();
	ShowWindow(wndHandle, SW_SHOW);
	return 0;
}

void DirectX12Renderer::setWinTitle(const char * title)
{
	size_t size = mbsrtowcs(NULL, &title, 0, NULL);
	wchar_t * buf = new wchar_t[size + 1]();
	size = mbsrtowcs(buf, &title, size + 1, NULL);
	SetWindowText(wndHandle, buf);
	delete[] buf;
}

void DirectX12Renderer::present()
{
	swapChain->Present(0, 0);
	waitForGPU();
	//Prep for next iteration
	commandAllocator->Reset();
	commandList->Reset(commandAllocator.Get(), nullptr);
	currBackBuffer = (currBackBuffer + 1) % SWAP_BUFFER_COUNT;
}

int DirectX12Renderer::shutdown()
{
	waitForGPU();
	return 0;
}

void DirectX12Renderer::setClearColor(float R, float G , float B, float T)
{
	clearColor[0] = R;
	clearColor[1] = G;
	clearColor[2] = B;
	clearColor[3] = T;
}

void DirectX12Renderer::clearBuffer(unsigned int opts)
{
	//Check for window messages because this is the first function called when rendering.
	if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	//Change state of back buffer to be abble to work on it. (Might wanna consider fixing this)
	d3dUtil::SetResourceTransitionBarrier(commandList.Get(),
		swapChainBuffers[currBackBuffer].Get(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET
	);

	//Clearing the backbuffer acording to input flags
	currDescHandle = getCurrBackBuffView();
	commandList->OMSetRenderTargets(1, &currDescHandle, true, &getDepthView());
	
	if (CLEAR_BUFFER_FLAGS::COLOR & opts)
	{
		commandList->ClearRenderTargetView(currDescHandle, clearColor, 0, nullptr);
	}
	if (CLEAR_BUFFER_FLAGS::DEPTH  & opts)
	{
		commandList->ClearDepthStencilView(getDepthView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	}

	commandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->RSSetViewports(1, &viewPort);
	commandList->RSSetScissorRects(1, &scissorRect);
}

void DirectX12Renderer::setRenderState(RenderState * ps)
{
	RenderStateDX12* rsPtr = dynamic_cast<RenderStateDX12*>(ps);
	if (rsPtr != nullptr)
	{
		pipelineState.setRenderState(rsPtr);
	}
	else
	{
		printf("Error: Not a RenderState DX12.");
	}
}
void DirectX12Renderer::submit(Mesh * mesh)
{
	if (perMat) 
		drawList2[mesh->technique].push_back(mesh);
	else
		drawList.push_back(mesh);
}

void DirectX12Renderer::frame()
{
	//Depends on drawing order
	if (perMat != 1)
	{
		for (auto mesh : drawList)
		{
			mesh->technique->enable(this);
			//Render meshes
		}
		drawList.clear();
	}
	else
	{
		for (auto work : drawList2)
		{
			work.first->enable(this);
			for (auto mesh : work.second)
			{
				//Render meshes
				size_t numOfVertices = mesh->geometryBuffers[0].numElements;
				//TODO bind textures
				for (auto t : mesh->textures)
				{
					t.second->bind(t.first);
				}
				//Bind vertex buffers
				for (auto ele : mesh->geometryBuffers)
				{
					mesh->bindIAVertexBuffer(ele.first);
				}
				//mesh->txBuffer->bind(work.first->getMaterial());
				//Bind the table
				Root.setRootTableData();
				//Draw
				commandList->DrawInstanced(numOfVertices, 1, 0, 0);
			}
		}
		drawList2.clear();
	}
			
	d3dUtil::SetResourceTransitionBarrier(commandList.Get(),
		swapChainBuffers[currBackBuffer].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT
	);	

	//End of recording
	commandList->Close();
	//Execute commandList
	executeCommandList();
}

void DirectX12Renderer::waitForGPU()
{
	const UINT64 fenceVal = fenceValue;
	commandQueue->Signal(fence.Get(), fenceVal);
	fenceValue++;

	if (fence->GetCompletedValue() < fenceVal)
	{
		fence->SetEventOnCompletion(fenceVal, eventHandle);
		WaitForSingleObject(eventHandle, INFINITY);
	}
}

Microsoft::WRL::ComPtr<ID3D12Device> DirectX12Renderer::getDevice()
{
	return device;
}

void DirectX12Renderer::setMaterialState(MaterialDX12 * material)
{
	pipelineState.setMaterial(material, Root.getRootSignature());
}


void DirectX12Renderer::createDevice()
{
	// Enable debug layer
	ID3D12Debug* debugController;
	D3D12GetDebugInterface(IID_PPV_ARGS(&debugController));
	debugController->EnableDebugLayer();
	debugController->Release();

	if (FAILED(D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))))
	{
		std::cout << "Failed to create device, trying to create warp device instead..." << std::endl;

		// Don't ask why
		Microsoft::WRL::ComPtr<IDXGIAdapter> warpAdapter;
		Microsoft::WRL::ComPtr<IDXGIFactory4> mdxgiFactory;
		CreateDXGIFactory1(IID_PPV_ARGS(&mdxgiFactory));
		mdxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter));
		//

		if (FAILED(D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))))
			std::cout << "Failed to create warp device" << std::endl;
		else
			std::cout << "Managed to create warp device." << std::endl;
	}
}

void DirectX12Renderer::createFenceAndDescriptorSizes()
{
	if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
		std::cout << "Failed to create fence." << std::endl;

	RTVDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	DSVDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	CBVSRVDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void DirectX12Renderer::createCommandObject()
{
	D3D12_COMMAND_QUEUE_DESC queueDesc;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.NodeMask = NULL;
	queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;

	if (FAILED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue))))
		std::cout << "Failed to create command queue." << std::endl;

	if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator))))
		std::cout << "Failed to create command allocator." << std::endl;

	if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList))))
		std::cout << "Failed to create command list." << std::endl;
}

void DirectX12Renderer::createSwapChain(HWND& wndHandle)
{
	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	swapChainDesc.BufferDesc.Width = width;
	swapChainDesc.BufferDesc.Height = height;
	swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
	swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = SWAP_BUFFER_COUNT;
	swapChainDesc.OutputWindow = wndHandle;
	swapChainDesc.Windowed = true;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	Microsoft::WRL::ComPtr<IDXGIFactory4> mdxgiFactory;
	CreateDXGIFactory1(IID_PPV_ARGS(&mdxgiFactory));
	if (FAILED(mdxgiFactory->CreateSwapChain(commandQueue.Get(), &swapChainDesc, &swapChain)))
		std::cout << "Failed to create swap chain." << std::endl;
}

void DirectX12Renderer::createRTV()
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(RTVHeap->GetCPUDescriptorHandleForHeapStart());

	for (UINT i = 0; i < SWAP_BUFFER_COUNT; i++)
	{
		DX::ThrowIfFailed(swapChain->GetBuffer(i, IID_PPV_ARGS(swapChainBuffers[i].GetAddressOf())));

		device->CreateRenderTargetView(swapChainBuffers[i].Get(), nullptr, rtvHeapHandle);

		rtvHeapHandle.Offset(1, RTVDescriptorSize);
	}

}

void DirectX12Renderer::createViewPortScissor()
{
	//Create viewport
	viewPort.TopLeftX = 0.0f;
	viewPort.TopLeftY = 0.0f;
	viewPort.Width = static_cast<float>(width);
	viewPort.Height = static_cast<float>(height);
	viewPort.MinDepth = 0.0f;
	viewPort.MaxDepth = 1.0f;
	//Create scissor rect
	scissorRect.top = 0;
	scissorRect.left = 0;
	scissorRect.bottom = height;
	scissorRect.right = width;
}

void DirectX12Renderer::createDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.NumDescriptors = SWAP_BUFFER_COUNT;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;

	DX::ThrowIfFailed(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(RTVHeap.GetAddressOf())));
	

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	DX::ThrowIfFailed(device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(DSVHeap.GetAddressOf())));
}

void DirectX12Renderer::createDepthStencil()
{
	//Depth Stencil Desc
	D3D12_RESOURCE_DESC depthStencilDesc = { };
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = width;
	depthStencilDesc.Height = height;
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.Format = DEPTH_STENCIL_FORMAT;
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	depthStencilDesc.SampleDesc.Count = 1;
	depthStencilDesc.SampleDesc.Quality = 0;
	//Clear Value Desc
	D3D12_CLEAR_VALUE optClear;
	optClear.Format = DEPTH_STENCIL_FORMAT;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;

	//Create DepthStencil
	DX::ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&depthStencilDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&optClear,
		IID_PPV_ARGS(depthStencilBuffer.GetAddressOf())));

	//Create descriptor to mip level 0
	device->CreateDepthStencilView(depthStencilBuffer.Get(), nullptr, getDepthView());
	//Transition the resource from its initial state to be used as a depth buffer
	d3dUtil::SetResourceTransitionBarrier(
		commandList.Get(),
		depthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_STATE_DEPTH_WRITE
	);
}

void DirectX12Renderer::executeCommandList()
{
	ID3D12CommandList* cmdList[] = { commandList.Get() };
	commandQueue->ExecuteCommandLists(ARRAYSIZE(cmdList), cmdList);
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectX12Renderer::getCurrBackBuffView()
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(RTVHeap->GetCPUDescriptorHandleForHeapStart(), currBackBuffer, RTVDescriptorSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectX12Renderer::getDepthView()
{
	return DSVHeap->GetCPUDescriptorHandleForHeapStart();
}

