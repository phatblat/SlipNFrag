#include "pch.h"
#include "MainPage.h"
#include "MainPage.g.cpp"
#include <Windows.UI.Xaml.Media.DXInterop.h>
#include "vid_uwp.h"
#include "sys_uwp.h"
#include "virtualkeymap.h"

using namespace winrt;
using namespace DirectX;
using namespace Windows::ApplicationModel;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Display;
using namespace Windows::Storage;
using namespace Windows::Storage::AccessCache;
using namespace Windows::Storage::FileProperties;
using namespace Windows::Storage::Streams;
using namespace Windows::System;
using namespace Windows::System::Threading;
using namespace Windows::UI;
using namespace Windows::UI::Core;
using namespace Windows::UI::Popups;
using namespace Windows::UI::ViewManagement;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Media::Imaging;

struct VertexShaderInput
{
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT2 texCoords;
};

static const int alignedConstantBufferSize = (sizeof(ModelViewProjectionConstantBuffer) + 255) & ~255;

static const DirectX::XMFLOAT4X4 Rotation0
(
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f
);

static const DirectX::XMFLOAT4X4 Rotation90
(
	0.0f, 1.0f, 0.0f, 0.0f,
	-1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f
);

static const DirectX::XMFLOAT4X4 Rotation180
(
	-1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, -1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f
);

static const DirectX::XMFLOAT4X4 Rotation270
(
	0.0f, -1.0f, 0.0f, 0.0f,
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f
);

namespace winrt::SlipNFrag_Windows::implementation
{
	MainPage::MainPage() : 
		defaultHeapProperties { D3D12_HEAP_TYPE_DEFAULT }, 
		uploadHeapProperties { D3D12_HEAP_TYPE_UPLOAD },
		screenTextureDesc { D3D12_RESOURCE_DIMENSION_TEXTURE2D },
		paletteTextureDesc{ D3D12_RESOURCE_DIMENSION_TEXTURE1D },
		screenSrvDesc { },
		paletteSrvDesc { },
		screenLocation { },
		screenUploadLocation { },
		paletteLocation { },
		paletteUploadLocation { }
	{
		InitializeComponent();
		defaultHeapProperties.CreationNodeMask = 1;
		defaultHeapProperties.VisibleNodeMask = 1;
		uploadHeapProperties.CreationNodeMask = 1;
		uploadHeapProperties.VisibleNodeMask = 1;
		screenTextureDesc.MipLevels = 1;
		screenTextureDesc.Format = DXGI_FORMAT_R8_UNORM;
		screenTextureDesc.DepthOrArraySize = 1;
		screenTextureDesc.SampleDesc.Count = 1;
		paletteTextureDesc.MipLevels = 1;
		paletteTextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		paletteTextureDesc.Width = 256;
		paletteTextureDesc.Height = 1;
		paletteTextureDesc.DepthOrArraySize = 1;
		paletteTextureDesc.SampleDesc.Count = 1;
		screenSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		screenSrvDesc.Format = screenTextureDesc.Format;
		screenSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		screenSrvDesc.Texture2D.MipLevels = screenTextureDesc.MipLevels;
		paletteSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		paletteSrvDesc.Format = paletteTextureDesc.Format;
		paletteSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
		paletteSrvDesc.Texture1D.MipLevels = paletteTextureDesc.MipLevels;
		screenLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		screenUploadLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		paletteLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		paletteUploadLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		Uri playBitmapImageUri(L"ms-appx:///Assets/play.png");
		BitmapImage playBitmapImage(playBitmapImageUri);
		Image playImage;
		playImage.Source(playBitmapImage);
		auto transparent = ColorHelper::FromArgb(0, 0, 0, 0);
		SolidColorBrush transparentBrush(transparent);
		Button playButton;
		playButton.Background(transparentBrush);
		playButton.HorizontalAlignment(HorizontalAlignment::Center);
		playButton.VerticalAlignment(VerticalAlignment::Center);
		playButton.Width(150);
		playButton.Height(150);
		playButton.Content(playImage);
		swapChainPanel().Children().Append(playButton);
		SymbolIcon settingsIcon;
		settingsIcon.Symbol(Symbol::Setting);
		Thickness settingsPadding;
		settingsPadding.Left = 12;
		settingsPadding.Top = 12;
		settingsPadding.Right = 12;
		settingsPadding.Bottom = 12;
		Button settingsButton;
		settingsButton.Background(transparentBrush);
		settingsButton.HorizontalAlignment(HorizontalAlignment::Right);
		settingsButton.VerticalAlignment(VerticalAlignment::Bottom);
		settingsButton.Width(42);
		settingsButton.Height(42);
		settingsButton.Content(settingsIcon);
		settingsButton.Margin(settingsPadding);
		swapChainPanel().Children().Append(settingsButton);
		playButton.Click([this, playButton, settingsButton](IInspectable const&, RoutedEventArgs const&)
			{
				uint32_t index;
				if (swapChainPanel().Children().IndexOf(playButton, index))
				{
					swapChainPanel().Children().RemoveAt(index);
				}
				if (swapChainPanel().Children().IndexOf(settingsButton, index))
				{
					swapChainPanel().Children().RemoveAt(index);
				}
				auto displayInformation = DisplayInformation::GetForCurrentView();
				displayInformation.DpiChanged([this](DisplayInformation const& sender, IInspectable const& e)
					{
						DpiChanged(sender, e);
					});
				displayInformation.OrientationChanged([this](DisplayInformation const& sender, IInspectable const& e)
					{
						OrientationChanged(sender, e);
					});
				displayInformation.DisplayContentsInvalidated([this](DisplayInformation const& sender, IInspectable const& e)
					{
						DisplayContentsInvalidated(sender, e);
					});
				auto folder = Package::Current().InstalledLocation();
				OpenVertexShaderFile(folder);
			});
		settingsButton.Click([this, playButton, settingsButton](IInspectable const&, RoutedEventArgs const&)
			{
				SettingsContentDialog settings;
				settings.ShowAsync(ContentDialogPlacement::InPlace);
			});
	}

	void MainPage::OpenVertexShaderFile(StorageFolder const& folder)
	{
		auto task = folder.GetFileAsync(L"VertexShader.cso");
		task.Completed([=](IAsyncOperation<StorageFile> const& operation, AsyncStatus const)
			{
				StorageFile file = operation.get();
				ReadVertexShaderFile(file, folder);
			});
	}

	void MainPage::ReadVertexShaderFile(StorageFile const& file, StorageFolder const& folder)
	{
		auto task = FileIO::ReadBufferAsync(file);
		task.Completed([=](IAsyncOperation<IBuffer> const& operation, AsyncStatus const)
			{
				auto vertexShaderFileBuffer = operation.get();
				vertexShaderBuffer.resize(vertexShaderFileBuffer.Length());
				DataReader::FromBuffer(vertexShaderFileBuffer).ReadBytes(vertexShaderBuffer);
				OpenPixelShaderFile(folder);
			});
	}

	void MainPage::OpenPixelShaderFile(StorageFolder const& folder)
	{
		auto task = folder.GetFileAsync(L"PixelShader.cso");
		task.Completed([=](IAsyncOperation<StorageFile> const& operation, AsyncStatus const)
			{
				StorageFile file = operation.get();
				ReadPixelShaderFile(file);
			});
	}

	void MainPage::ReadPixelShaderFile(StorageFile const& file)
	{
		auto pixelShaderReadTask = FileIO::ReadBufferAsync(file);
		pixelShaderReadTask.Completed([=](IAsyncOperation<IBuffer> const& operation, AsyncStatus const)
			{
				auto pixelShaderFileBuffer = operation.get();
				pixelShaderBuffer.resize(pixelShaderFileBuffer.Length());
				DataReader::FromBuffer(pixelShaderFileBuffer).ReadBytes(pixelShaderBuffer);
				Start();
			});
	}

	void MainPage::Start()
	{
		auto task = StorageApplicationPermissions::FutureAccessList().GetFolderAsync(L"basedir_text");
		task.Completed([this](IAsyncOperation<StorageFolder> const& operation, AsyncStatus const status)
			{
				std::string basedir_text;
				if (status == AsyncStatus::Completed)
				{
					auto folder = operation.GetResults();
					auto path = folder.Path();
					for (auto c : path)
					{
						basedir_text.push_back((char)c);
					}
				}
				std::vector<std::string> arguments;
				arguments.emplace_back("SlipNFrag.exe");
				if (basedir_text.length() > 0)
				{
					arguments.emplace_back("-basedir");
					arguments.push_back(basedir_text);
				}
				/*if ([NSUserDefaults.standardUserDefaults boolForKey : @"hipnotic_radio"])
				{
					arguments.emplace_back("-hipnotic");
				}
				else if ([NSUserDefaults.standardUserDefaults boolForKey : @"rogue_radio"])
				{
					arguments.emplace_back("-rogue");
				}
				else if ([NSUserDefaults.standardUserDefaults boolForKey : @"game_radio"])
				{
					auto game = [NSUserDefaults.standardUserDefaults stringForKey : @"game_text"];
					if (game != nil && ![game isEqualToString : @""])
					{
						arguments.emplace_back("-game");
						arguments.emplace_back([game cStringUsingEncoding : NSString.defaultCStringEncoding]);
					}
				}
				auto commandLine = [NSUserDefaults.standardUserDefaults stringForKey : @"command_line_text"];
				if (commandLine != nil && ![commandLine isEqualToString : @""])
				{
					NSArray<NSString*>* components = [commandLine componentsSeparatedByString : @" "];
						for (NSString* component : components)
						{
							auto componentAsString = [component cStringUsingEncoding : NSString.defaultCStringEncoding];
							arguments.emplace_back(componentAsString);
						}
				}*/
				sys_argc = (int)arguments.size();
				sys_argv = new char* [sys_argc];
				for (auto i = 0; i < arguments.size(); i++)
				{
					sys_argv[i] = new char[arguments[i].length() + 1];
					strcpy(sys_argv[i], arguments[i].c_str());
				}
				swapChainPanel().Dispatcher().RunAsync(CoreDispatcherPriority::High, [=]()
					{
						newWidth = (float)swapChainPanel().ActualWidth();
						newHeight = (float)swapChainPanel().ActualHeight();
						CreateDevice();
						std::thread filesThread([this]()
							{
								ProcessFiles();
							});
						filesThread.detach();
						vid_width = (int)newWidth;
						vid_height = (int)newHeight;
						byte* data = nullptr;
						check_hresult(screenUpload->Map(0, nullptr, reinterpret_cast<void**>(&data)));
						VID_Map(data);
						Sys_Init(sys_argc, sys_argv);
						if (DisplaySysErrorIfNeeded())
						{
							return;
						}
						Window::Current().CoreWindow().PointerCursor(nullptr);
						Window::Current().CoreWindow().KeyDown([](IInspectable const&, KeyEventArgs const& e)
							{
								auto mapped = virtualkeymap[(int)e.VirtualKey()];
								Key_Event(mapped, true);
							});
						Window::Current().CoreWindow().KeyUp([](IInspectable const&, KeyEventArgs const& e)
							{
								auto mapped = virtualkeymap[(int)e.VirtualKey()];
								Key_Event(mapped, false);
							});
						ApplicationView::GetForCurrentView().TryEnterFullScreenMode();
						renderLoopWorker = ThreadPool::RunAsync([=](IAsyncAction const& action)
							{
								RenderLoop(action);
							}, WorkItemPriority::High, WorkItemOptions::TimeSliced);
					});
			});
	}

	void MainPage::CreateDevice()
	{
		UINT dxgiFactoryFlags = 0;
#if defined(_DEBUG)
		com_ptr<ID3D12Debug> debugController0;
		com_ptr<ID3D12Debug1> debugController1;
		check_hresult(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController0)));
		debugController0->EnableDebugLayer();
		debugController1 = debugController0.as<ID3D12Debug1>();
		debugController1->SetEnableGPUBasedValidation(true);
		debugController1->SetEnableSynchronizedCommandQueueValidation(true);
		dxgiFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif
		check_hresult(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));
		com_ptr<IDXGIAdapter1> adapter = nullptr;
		for (auto i = 0; dxgiFactory->EnumAdapters1(i, adapter.put()) != DXGI_ERROR_NOT_FOUND; i++)
		{
			DXGI_ADAPTER_DESC1 descriptor;
			adapter->GetDesc1(&descriptor);
			if ((descriptor.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
			{
				continue;
			}
			if (SUCCEEDED(D3D12CreateDevice(adapter.get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)))
			{
				break;
			}
		}
		if (FAILED(D3D12CreateDevice(adapter.get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3dDevice))))
		{
			com_ptr<IDXGIAdapter> warpAdapter;
			check_hresult(dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));
			check_hresult(D3D12CreateDevice(adapter.get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3dDevice)));
		}
		D3D12_COMMAND_QUEUE_DESC queueDesc { D3D12_COMMAND_LIST_TYPE_DIRECT, 0, D3D12_COMMAND_QUEUE_FLAG_NONE };
		check_hresult(d3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)));
		commandQueue->SetName(L"commandQueue");
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc { D3D12_DESCRIPTOR_HEAP_TYPE_RTV, frameCount };
		check_hresult(d3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap)));
		rtvDescriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc { D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1 };
		check_hresult(d3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap)));
		D3D12_DESCRIPTOR_HEAP_DESC cbvSrvHeapDesc { D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, frameCount * 3, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE };
		check_hresult(d3dDevice->CreateDescriptorHeap(&cbvSrvHeapDesc, IID_PPV_ARGS(&cbvSrvHeap)));
		cbvSrvHeap->SetName(L"cbvSrvHeap");
		for (auto i = 0; i < frameCount; i++)
		{
			check_hresult(d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocators[i])));
		}
		check_hresult(d3dDevice->CreateFence(fenceValues[0], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
		fenceValues[0]++;
		fenceEvent = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
		auto displayInformation = DisplayInformation::GetForCurrentView();
		Size outputSize;
		CreateOutputSize(displayInformation, outputSize);
		DXGI_MODE_ROTATION rotation = DXGI_MODE_ROTATION_UNSPECIFIED;
		CreateRotation(displayInformation, rotation);
		Size renderTargetSize;
		CreateRenderTargetSize(rotation, outputSize, renderTargetSize);
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc { };
		swapChainDesc.Width = lround(renderTargetSize.Width);
		swapChainDesc.Height = lround(renderTargetSize.Height);
		swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = frameCount;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
		com_ptr<IDXGISwapChain1> newSwapChain;
		check_hresult(dxgiFactory->CreateSwapChainForComposition(commandQueue.get(), &swapChainDesc, nullptr, newSwapChain.put()));
		newSwapChain.as(swapChain);
		com_ptr<ISwapChainPanelNative> panelNative;
		swapChainPanel().as(panelNative);
		check_hresult(panelNative->SetSwapChain(swapChain.get()));
		XMFLOAT4X4 orientationTransform3D;
		CreateOrientationTransform3D(rotation, orientationTransform3D);
		UpdateSwapChainTransform(rotation, swapChainPanel().CompositionScaleX(), swapChainPanel().CompositionScaleY());
		currentFrame = swapChain->GetCurrentBackBufferIndex();
		CreateRenderTargetViews();
		CreateDepthStencilBuffer(swapChainDesc.Width, swapChainDesc.Height);
		viewport = { 0, 0, renderTargetSize.Width, renderTargetSize.Height, 0, 1 };
		D3D12_DESCRIPTOR_RANGE1 ranges[2] { };
		ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		ranges[0].NumDescriptors = 1;
		ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		ranges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
		ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		ranges[1].NumDescriptors = 2;
		ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		ranges[1].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
		D3D12_ROOT_PARAMETER1 parameters[2] { };
		parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		parameters[0].DescriptorTable.NumDescriptorRanges = 1;
		parameters[0].DescriptorTable.pDescriptorRanges = &ranges[0];
		parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		parameters[1].DescriptorTable.NumDescriptorRanges = 1;
		parameters[1].DescriptorTable.pDescriptorRanges = &ranges[1];
		D3D12_STATIC_SAMPLER_DESC samplers[2] { };
		samplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		samplers[0].MaxLOD = D3D12_FLOAT32_MAX;
		samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		samplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		samplers[1].MaxLOD = D3D12_FLOAT32_MAX;
		samplers[1].ShaderRegister = 1;
		samplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		D3D12_VERSIONED_ROOT_SIGNATURE_DESC descRootSignature { };
		descRootSignature.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
		descRootSignature.Desc_1_1.NumParameters = 2;
		descRootSignature.Desc_1_1.pParameters = &parameters[0];
		descRootSignature.Desc_1_1.NumStaticSamplers = 2;
		descRootSignature.Desc_1_1.pStaticSamplers = &samplers[0];
		descRootSignature.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
		com_ptr<ID3DBlob> signature;
		com_ptr<ID3DBlob> error;
		check_hresult(D3D12SerializeVersionedRootSignature(&descRootSignature, signature.put(), error.put()));
		check_hresult(d3dDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));
		rootSignature->SetName(L"rootSignature");
		static const D3D12_INPUT_ELEMENT_DESC inputLayout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		D3D12_GRAPHICS_PIPELINE_STATE_DESC state { };
		state.InputLayout = { inputLayout, _countof(inputLayout) };
		state.pRootSignature = rootSignature.get();
		state.VS.BytecodeLength = vertexShaderBuffer.size();
		state.VS.pShaderBytecode = vertexShaderBuffer.data();
		state.PS.BytecodeLength = pixelShaderBuffer.size();
		state.PS.pShaderBytecode = pixelShaderBuffer.data();
		state.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		state.RasterizerState.DepthClipEnable = true;
		const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
		{
			FALSE,FALSE,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_LOGIC_OP_NOOP,
			D3D12_COLOR_WRITE_ENABLE_ALL,
		};
		for (auto i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
		{
			state.BlendState.RenderTarget[i] = defaultRenderTargetBlendDesc;
		}
		state.DepthStencilState.DepthEnable = true;
		state.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		state.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		state.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
		state.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
		const D3D12_DEPTH_STENCILOP_DESC defaultStencilOp =
		{
			D3D12_STENCIL_OP_KEEP,
			D3D12_STENCIL_OP_KEEP,
			D3D12_STENCIL_OP_KEEP,
			D3D12_COMPARISON_FUNC_ALWAYS
		};
		state.DepthStencilState.FrontFace = defaultStencilOp;
		state.DepthStencilState.BackFace = defaultStencilOp;
		state.SampleMask = UINT_MAX;
		state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		state.NumRenderTargets = 1;
		state.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
		state.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		state.SampleDesc.Count = 1;
		check_hresult(d3dDevice->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&pipelineState)));
		pipelineState->SetName(L"pipelineState");
		check_hresult(d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators[0].get(), pipelineState.get(), IID_PPV_ARGS(&commandList)));
		commandList->SetName(L"commandList");
		VertexShaderInput screenPlaneVertices[] =
		{
			{ XMFLOAT3(-1, 1, -0.5f), XMFLOAT2(0, 0) },
			{ XMFLOAT3(-1, -1, -0.5f), XMFLOAT2(0, 1) },
			{ XMFLOAT3(1, 1, -0.5f), XMFLOAT2(1, 0) },
			{ XMFLOAT3(1, -1, -0.5f), XMFLOAT2(1, 1) }
		};
		auto vertexBufferSize = sizeof(screenPlaneVertices);
		D3D12_RESOURCE_DESC vertexBufferDesc { D3D12_RESOURCE_DIMENSION_BUFFER };
		vertexBufferDesc.Width = vertexBufferSize;
		vertexBufferDesc.Height = 1;
		vertexBufferDesc.DepthOrArraySize = 1;
		vertexBufferDesc.MipLevels = 1;
		vertexBufferDesc.SampleDesc.Count = 1;
		vertexBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		check_hresult(d3dDevice->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &vertexBufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&vertexBuffer)));
		vertexBuffer->SetName(L"vertexBuffer");
		com_ptr<ID3D12Resource> vertexBufferUpload;
		check_hresult(d3dDevice->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &vertexBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertexBufferUpload)));
		vertexBufferUpload->SetName(L"vertexBufferUpload");
		D3D12_SUBRESOURCE_DATA vertexData;
		vertexData.pData = screenPlaneVertices;
		vertexData.RowPitch = vertexBufferSize;
		vertexData.SlicePitch = vertexData.RowPitch;
		UploadBufferToGPU(vertexData, vertexBuffer, vertexBufferUpload, vertexBufferDesc);
		D3D12_RESOURCE_DESC constantBufferDesc { D3D12_RESOURCE_DIMENSION_BUFFER };
		constantBufferDesc.Width = frameCount * alignedConstantBufferSize;
		constantBufferDesc.Height = 1;
		constantBufferDesc.DepthOrArraySize = 1;
		constantBufferDesc.MipLevels = 1;
		constantBufferDesc.SampleDesc.Count = 1;
		constantBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		check_hresult(d3dDevice->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &constantBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&constantBuffer)));
		constantBuffer->SetName(L"constantBuffer");
		check_hresult(d3dDevice->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &paletteTextureDesc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&palette)));
		palette->SetName(L"palette");
		paletteLocation.pResource = palette.get();
		UINT64 uploadBufferSize = 0;
		d3dDevice->GetCopyableFootprints(&paletteTextureDesc, 0, 1, 0, nullptr, nullptr, nullptr, &uploadBufferSize);
		D3D12_HEAP_PROPERTIES paletteUploadHeapProperties { D3D12_HEAP_TYPE_UPLOAD };
		paletteUploadHeapProperties.CreationNodeMask = 1;
		paletteUploadHeapProperties.VisibleNodeMask = 1;
		D3D12_RESOURCE_DESC paletteUploadBufferDesc { D3D12_RESOURCE_DIMENSION_BUFFER };
		paletteUploadBufferDesc.Width = uploadBufferSize;
		paletteUploadBufferDesc.Height = 1;
		paletteUploadBufferDesc.DepthOrArraySize = 1;
		paletteUploadBufferDesc.MipLevels = 1;
		paletteUploadBufferDesc.SampleDesc.Count = 1;
		paletteUploadBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		check_hresult(d3dDevice->CreateCommittedResource(&paletteUploadHeapProperties, D3D12_HEAP_FLAG_NONE, &paletteUploadBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&paletteUpload)));
		paletteUpload->SetName(L"paletteUpload");
		paletteUploadLocation.pResource = paletteUpload.get();
		CreateScreen();
		auto address = constantBuffer->GetGPUVirtualAddress();
		auto handle = cbvSrvHeap->GetCPUDescriptorHandleForHeapStart();
		auto increment = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		for (auto i = 0; i < frameCount; i++)
		{
			D3D12_CONSTANT_BUFFER_VIEW_DESC desc { };
			desc.BufferLocation = address;
			desc.SizeInBytes = alignedConstantBufferSize;
			d3dDevice->CreateConstantBufferView(&desc, handle);
			address += desc.SizeInBytes;
			handle.ptr += increment;
			d3dDevice->CreateShaderResourceView(screen.get(), &screenSrvDesc, handle);
			handle.ptr += increment;
			d3dDevice->CreateShaderResourceView(palette.get(), &paletteSrvDesc, handle);
			handle.ptr += increment;
		}
		D3D12_RANGE readRange { };
		check_hresult(constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&mappedConstantBuffer)));
		memset(mappedConstantBuffer, 0, frameCount * alignedConstantBufferSize);
		vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
		vertexBufferView.StrideInBytes = sizeof(VertexShaderInput);
		vertexBufferView.SizeInBytes = (UINT)vertexBufferSize;
		check_hresult(commandList->Close());
		ID3D12CommandList* ppCommandLists[] = { commandList.get() };
		commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
		WaitForGPU(0, "WaitForGPU failed while creating device");
		UpdateModelViewProjectionMatrix(orientationTransform3D);
		deviceRemoved = false;
		firstResize = true;
	}

	void MainPage::RemoveDevice()
	{
		deviceRemoved = true;
		swapChainPanel().Dispatcher().RunAsync(CoreDispatcherPriority::High, [this]()
			{
				com_ptr<ISwapChainPanelNative> panelNative;
				swapChainPanel().as(panelNative);
				check_hresult(panelNative->SetSwapChain(nullptr));
			});
	}

	void MainPage::swapChainPanel_SizeChanged(IInspectable const&, SizeChangedEventArgs const& e)
	{
		if (deviceRemoved)
		{
			return;
		}
		std::lock_guard lock(renderLoopMutex);
		WaitForGPU(currentFrame, "WaitForGPU failed while resizing swap chain panel");
		CleanupFrameData();
		auto displayInformation = DisplayInformation::GetForCurrentView();
		newWidth = e.NewSize().Width;
		newHeight = e.NewSize().Height;
		Size outputSize;
		CreateOutputSize(displayInformation, outputSize);
		DXGI_MODE_ROTATION rotation = DXGI_MODE_ROTATION_UNSPECIFIED;
		CreateRotation(displayInformation, rotation);
		Size renderTargetSize;
		CreateRenderTargetSize(rotation, outputSize, renderTargetSize);
		auto result = swapChain->ResizeBuffers(frameCount, lround(renderTargetSize.Width), lround(renderTargetSize.Height), DXGI_FORMAT_B8G8R8A8_UNORM, 0);
		if (result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET)
		{
			RemoveDevice();
			return;
		}
		check_hresult(result);
		XMFLOAT4X4 orientationTransform3D;
		CreateOrientationTransform3D(rotation, orientationTransform3D);
		UpdateSwapChainTransform(rotation, swapChainPanel().CompositionScaleX(), swapChainPanel().CompositionScaleY());
		currentFrame = swapChain->GetCurrentBackBufferIndex();
		CreateRenderTargetViews();
		depthStencil = nullptr;
		CreateDepthStencilBuffer(lround(renderTargetSize.Width), lround(renderTargetSize.Height));
		viewport = { 0, 0, renderTargetSize.Width, renderTargetSize.Height, 0, 1 };
		screen = nullptr;
		screenUpload = nullptr;
		CreateScreen();
		auto handle = cbvSrvHeap->GetCPUDescriptorHandleForHeapStart();
		auto increment = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		handle.ptr += increment;
		for (auto i = 0; i < frameCount; i++)
		{
			d3dDevice->CreateShaderResourceView(screen.get(), &screenSrvDesc, handle);
			handle.ptr += increment * 3;
		}
		UpdateModelViewProjectionMatrix(orientationTransform3D);
	}

	void MainPage::swapChainPanel_CompositionScaleChanged(SwapChainPanel const& sender, IInspectable const&)
	{
		if (deviceRemoved)
		{
			return;
		}
		std::lock_guard lock(renderLoopMutex);
		WaitForGPU(currentFrame, "WaitForGPU failed while setting composition scale");
		CleanupFrameData();
		auto displayInformation = DisplayInformation::GetForCurrentView();
		newWidth = (float)swapChainPanel().ActualWidth();
		newHeight = (float)swapChainPanel().ActualHeight();
		Size outputSize;
		CreateOutputSize(displayInformation, outputSize);
		DXGI_MODE_ROTATION rotation = DXGI_MODE_ROTATION_UNSPECIFIED;
		CreateRotation(displayInformation, rotation);
		Size renderTargetSize;
		CreateRenderTargetSize(rotation, outputSize, renderTargetSize);
		auto result = swapChain->ResizeBuffers(frameCount, lround(renderTargetSize.Width), lround(renderTargetSize.Height), DXGI_FORMAT_B8G8R8A8_UNORM, 0);
		if (result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET)
		{
			RemoveDevice();
			return;
		}
		check_hresult(result);
		XMFLOAT4X4 orientationTransform3D;
		CreateOrientationTransform3D(rotation, orientationTransform3D);
		UpdateSwapChainTransform(rotation, sender.CompositionScaleX(), sender.CompositionScaleY());
		currentFrame = swapChain->GetCurrentBackBufferIndex();
		CreateRenderTargetViews();
		depthStencil = nullptr;
		CreateDepthStencilBuffer(lround(renderTargetSize.Width), lround(renderTargetSize.Height));
		viewport = { 0, 0, renderTargetSize.Width, renderTargetSize.Height, 0, 1 };
		screen = nullptr;
		screenUpload = nullptr;
		CreateScreen();
		auto handle = cbvSrvHeap->GetCPUDescriptorHandleForHeapStart();
		auto increment = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		handle.ptr += increment;
		for (auto i = 0; i < frameCount; i++)
		{
			d3dDevice->CreateShaderResourceView(screen.get(), &screenSrvDesc, handle);
			handle.ptr += increment * 3;
		}
		UpdateModelViewProjectionMatrix(orientationTransform3D);
	}

	void MainPage::DpiChanged(Windows::Graphics::Display::DisplayInformation const& sender, IInspectable const&)
	{
		if (deviceRemoved)
		{
			return;
		}
		std::lock_guard lock(renderLoopMutex);
		WaitForGPU(currentFrame, "WaitForGPU failed while setting screen DPI");
		CleanupFrameData();
		newWidth = (float)swapChainPanel().ActualWidth();
		newHeight = (float)swapChainPanel().ActualHeight();
		Size outputSize;
		CreateOutputSize(sender, outputSize);
		DXGI_MODE_ROTATION rotation = DXGI_MODE_ROTATION_UNSPECIFIED;
		CreateRotation(sender, rotation);
		Size renderTargetSize;
		CreateRenderTargetSize(rotation, outputSize, renderTargetSize);
		auto result = swapChain->ResizeBuffers(frameCount, lround(renderTargetSize.Width), lround(renderTargetSize.Height), DXGI_FORMAT_B8G8R8A8_UNORM, 0);
		if (result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET)
		{
			RemoveDevice();
			return;
		}
		check_hresult(result);
		XMFLOAT4X4 orientationTransform3D;
		CreateOrientationTransform3D(rotation, orientationTransform3D);
		UpdateSwapChainTransform(rotation, swapChainPanel().CompositionScaleX(), swapChainPanel().CompositionScaleX());
		currentFrame = swapChain->GetCurrentBackBufferIndex();
		CreateRenderTargetViews();
		depthStencil = nullptr;
		CreateDepthStencilBuffer(lround(renderTargetSize.Width), lround(renderTargetSize.Height));
		viewport = { 0, 0, renderTargetSize.Width, renderTargetSize.Height, 0, 1 };
		screen = nullptr;
		screenUpload = nullptr;
		CreateScreen();
		auto handle = cbvSrvHeap->GetCPUDescriptorHandleForHeapStart();
		auto increment = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		handle.ptr += increment;
		for (auto i = 0; i < frameCount; i++)
		{
			d3dDevice->CreateShaderResourceView(screen.get(), &screenSrvDesc, handle);
			handle.ptr += increment * 3;
		}
		UpdateModelViewProjectionMatrix(orientationTransform3D);
	}

	void MainPage::OrientationChanged(Windows::Graphics::Display::DisplayInformation const& sender, IInspectable const&)
	{
		if (deviceRemoved)
		{
			return;
		}
		std::lock_guard lock(renderLoopMutex);
		WaitForGPU(currentFrame, "WaitForGPU failed while setting screen orientation");
		CleanupFrameData();
		newWidth = (float)swapChainPanel().ActualWidth();
		newHeight = (float)swapChainPanel().ActualHeight();
		Size outputSize;
		CreateOutputSize(sender, outputSize);
		DXGI_MODE_ROTATION rotation = DXGI_MODE_ROTATION_UNSPECIFIED;
		CreateRotation(sender, rotation);
		Size renderTargetSize;
		CreateRenderTargetSize(rotation, outputSize, renderTargetSize);
		auto result = swapChain->ResizeBuffers(frameCount, lround(renderTargetSize.Width), lround(renderTargetSize.Height), DXGI_FORMAT_B8G8R8A8_UNORM, 0);
		if (result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET)
		{
			RemoveDevice();
			return;
		}
		check_hresult(result);
		XMFLOAT4X4 orientationTransform3D;
		CreateOrientationTransform3D(rotation, orientationTransform3D);
		UpdateSwapChainTransform(rotation, swapChainPanel().CompositionScaleX(), swapChainPanel().CompositionScaleX());
		currentFrame = swapChain->GetCurrentBackBufferIndex();
		CreateRenderTargetViews();
		depthStencil = nullptr;
		CreateDepthStencilBuffer(lround(renderTargetSize.Width), lround(renderTargetSize.Height));
		viewport = { 0, 0, renderTargetSize.Width, renderTargetSize.Height, 0, 1 };
		screen = nullptr;
		screenUpload = nullptr;
		CreateScreen();
		auto handle = cbvSrvHeap->GetCPUDescriptorHandleForHeapStart();
		auto increment = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		handle.ptr += increment;
		for (auto i = 0; i < frameCount; i++)
		{
			d3dDevice->CreateShaderResourceView(screen.get(), &screenSrvDesc, handle);
			handle.ptr += increment * 3;
		}
		UpdateModelViewProjectionMatrix(orientationTransform3D);
	}

	void MainPage::DisplayContentsInvalidated(Windows::Graphics::Display::DisplayInformation const&, IInspectable const&)
	{
		if (deviceRemoved)
		{
			return;
		}
		std::lock_guard lock(renderLoopMutex);
		if (FAILED(d3dDevice->GetDeviceRemovedReason()))
		{
			RemoveDevice();
			return;
		}
		DXGI_ADAPTER_DESC previousDesc;
		com_ptr<IDXGIAdapter1> previousDefaultAdapter;
		check_hresult(dxgiFactory->EnumAdapters1(0, previousDefaultAdapter.put()));
		check_hresult(previousDefaultAdapter->GetDesc(&previousDesc));
		DXGI_ADAPTER_DESC currentDesc;
		com_ptr<IDXGIFactory4> currentDxgiFactory;
		check_hresult(CreateDXGIFactory1(IID_PPV_ARGS(&currentDxgiFactory)));
		com_ptr<IDXGIAdapter1> currentDefaultAdapter;
		check_hresult(currentDxgiFactory->EnumAdapters1(0, currentDefaultAdapter.put()));
		check_hresult(currentDefaultAdapter->GetDesc(&currentDesc));
		if (previousDesc.AdapterLuid.LowPart != currentDesc.AdapterLuid.LowPart || previousDesc.AdapterLuid.HighPart != currentDesc.AdapterLuid.HighPart)
		{
			RemoveDevice();
		}
	}

	void MainPage::RenderLoop(IAsyncAction const& action)
	{
		if (action.Status() != AsyncStatus::Started)
		{
			return;
		}
		if (deviceRemoved)
		{
			constantBuffer->Unmap(0, nullptr);
			screenUpload->Unmap(0, nullptr);
			screen = nullptr;
			palette = nullptr;
			depthStencil = nullptr;
			CreateDevice();
#if defined(_DEBUG)
			com_ptr<IDXGIDebug1> dxgiDebug;
			if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
			{
				dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
			}
#endif
		}
		else
		{
			std::lock_guard lock(renderLoopMutex);
			PIXBeginEvent(commandQueue.get(), 0, L"Update");
			auto updated = Update();
			PIXEndEvent(commandQueue.get());
			if (updated)
			{
				PIXBeginEvent(commandQueue.get(), 0, L"Render");
				Render();
				PIXEndEvent(commandQueue.get());
			}
		}
		renderLoopWorker = ThreadPool::RunAsync([=](IAsyncAction const& action)
			{
				RenderLoop(action);
			}, WorkItemPriority::High, WorkItemOptions::TimeSliced);
	}

	bool MainPage::Update()
	{
		if (deviceRemoved)
		{
			return false;
		}
		if (previousTime.QuadPart == 0)
		{
			QueryPerformanceCounter(&previousTime);
			return false;
		}
		LARGE_INTEGER time;
		QueryPerformanceCounter(&time);
		LARGE_INTEGER frequency;
		QueryPerformanceFrequency(&frequency);
		auto elapsed = (float)(time.QuadPart - previousTime.QuadPart) / (float)frequency.QuadPart;
		UINT8* destination = mappedConstantBuffer + currentFrame * alignedConstantBufferSize;
		memcpy(destination, &constantBufferData, sizeof(constantBufferData));
		if (firstResize || vid_width != (int)newWidth || vid_height != (int)newHeight || vid_rowbytes != (int)newRowbytes)
		{
			vid_width = (int)newWidth;
			vid_height = (int)newHeight;
			vid_rowbytes = (int)newRowbytes;
			screenUpload->Unmap(0, nullptr);
			VID_Resize();
			byte* data = nullptr;
			check_hresult(screenUpload->Map(0, nullptr, reinterpret_cast<void**>(&data)));
			VID_Map(data);
			firstResize = false;
		}
		Sys_Frame(elapsed);
		previousTime = time;
		return true;
	}

	void MainPage::Render()
	{
		check_hresult(commandAllocators[currentFrame]->Reset());
		check_hresult(commandList->Reset(commandAllocators[currentFrame].get(), pipelineState.get()));
		PIXBeginEvent(commandList.get(), 0, L"Draw screen contents");
		commandList->SetGraphicsRootSignature(rootSignature.get());
		ID3D12DescriptorHeap* ppHeaps[] = { cbvSrvHeap.get() };
		commandList->SetDescriptorHeaps(1, ppHeaps);
		D3D12_GPU_DESCRIPTOR_HANDLE handle = cbvSrvHeap->GetGPUDescriptorHandleForHeapStart();
		auto increment = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		handle.ptr += currentFrame * 3 * increment;
		commandList->SetGraphicsRootDescriptorTable(0, handle);
		handle.ptr += increment;
		commandList->SetGraphicsRootDescriptorTable(1, handle);
		commandList->RSSetViewports(1, &viewport);
		commandList->RSSetScissorRects(1, &scissorRect);
		D3D12_RESOURCE_BARRIER renderTargetResourceBarrier { };
		renderTargetResourceBarrier.Transition.pResource = renderTargets[currentFrame].get();
		renderTargetResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		renderTargetResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		renderTargetResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList->ResourceBarrier(1, &renderTargetResourceBarrier);
		D3D12_RESOURCE_BARRIER transition { };
		transition.Transition.pResource = screen.get();
		transition.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		transition.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
		transition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList->ResourceBarrier(1, &transition);
		d3dDevice->GetCopyableFootprints(&screenTextureDesc, 0, 1, 0, &screenUploadLocation.PlacedFootprint, nullptr, nullptr, nullptr);
		commandList->CopyTextureRegion(&screenLocation, 0, 0, 0, &screenUploadLocation, nullptr);
		transition.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		transition.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		commandList->ResourceBarrier(1, &transition);
		if (vid_palettechanged)
		{
			transition.Transition.pResource = palette.get();
			transition.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			transition.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
			commandList->ResourceBarrier(1, &transition);
			d3dDevice->GetCopyableFootprints(&paletteTextureDesc, 0, 1, 0, &paletteUploadLocation.PlacedFootprint, nullptr, nullptr, nullptr);
			BYTE* data = nullptr;
			check_hresult(paletteUpload->Map(0, nullptr, reinterpret_cast<void**>(&data)));
			memcpy(data, d_8to24table, 1024);
			paletteUpload->Unmap(0, nullptr);
			commandList->CopyTextureRegion(&paletteLocation, 0, 0, 0, &paletteUploadLocation, nullptr);
			transition.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
			transition.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			commandList->ResourceBarrier(1, &transition);
			vid_palettechanged = false;
		}
		D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView { rtvHeap->GetCPUDescriptorHandleForHeapStart().ptr + currentFrame * rtvDescriptorSize };
		D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView { dsvHeap->GetCPUDescriptorHandleForHeapStart().ptr };
		commandList->ClearDepthStencilView(depthStencilView, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		commandList->OMSetRenderTargets(1, &renderTargetView, false, &depthStencilView);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		commandList->DrawInstanced(4, 1, 0, 0);
		D3D12_RESOURCE_BARRIER presentResourceBarrier { };
		presentResourceBarrier.Transition.pResource = renderTargets[currentFrame].get();
		presentResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		presentResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		presentResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList->ResourceBarrier(1, &presentResourceBarrier);
		PIXEndEvent(commandList.get());
		check_hresult(commandList->Close());
		ID3D12CommandList* ppCommandLists[] = { commandList.get() };
		commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
		auto result = swapChain->Present(1, 0);
		if (result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET)
		{
			RemoveDevice();
			return;
		}
		check_hresult(result);
		auto currentFenceValue = fenceValues[currentFrame];
		check_hresult(commandQueue->Signal(fence.get(), currentFenceValue));
		currentFrame = swapChain->GetCurrentBackBufferIndex();
		if (fence->GetCompletedValue() < fenceValues[currentFrame])
		{
			check_hresult(fence->SetEventOnCompletion(fenceValues[currentFrame], fenceEvent));
			if (WaitForSingleObjectEx(fenceEvent, 5000, FALSE) != WAIT_OBJECT_0)
			{
				throw std::runtime_error("WaitForSingleObjectEx failed at rendering stage.");
			}
		}
		fenceValues[currentFrame] = currentFenceValue + 1;
	}

	void MainPage::WaitForGPU(int frame, const char* timeoutError)
	{
		check_hresult(commandQueue->Signal(fence.get(), fenceValues[frame]));
		check_hresult(fence->SetEventOnCompletion(fenceValues[frame], fenceEvent));
		if (WaitForSingleObjectEx(fenceEvent, 5000, FALSE) != WAIT_OBJECT_0)
		{
			throw std::runtime_error(timeoutError);
		}
		fenceValues[frame]++;
	}

	void MainPage::UploadBufferToGPU(D3D12_SUBRESOURCE_DATA data, com_ptr<ID3D12Resource> const& buffer, com_ptr<ID3D12Resource> const& bufferUpload, D3D12_RESOURCE_DESC const& descriptor)
	{
		UINT64 MemToAlloc = static_cast<UINT64>(sizeof(D3D12_PLACED_SUBRESOURCE_FOOTPRINT) + sizeof(UINT) + sizeof(UINT64));
		void* pMem = HeapAlloc(GetProcessHeap(), 0, static_cast<SIZE_T>(MemToAlloc));
		if (pMem == NULL)
		{
			throw std::bad_alloc();
		}
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT* pLayouts = reinterpret_cast<D3D12_PLACED_SUBRESOURCE_FOOTPRINT*>(pMem);
		auto pRowSizesInBytes = (UINT64*)(pLayouts + 1);
		auto pNumRows = (UINT*)(pRowSizesInBytes + 1);
		UINT64 RequiredSize = 0;
		d3dDevice->GetCopyableFootprints(&descriptor, 0, 1, 0, pLayouts, pNumRows, pRowSizesInBytes, &RequiredSize);
		BYTE* pData = nullptr;
		check_hresult(bufferUpload->Map(0, NULL, reinterpret_cast<void**>(&pData)));
		for (auto i = 0; i < 1; i++)
		{
			D3D12_MEMCPY_DEST DestData { pData + pLayouts[i].Offset, pLayouts[i].Footprint.RowPitch, pLayouts[i].Footprint.RowPitch * pNumRows[i] };
			for (UINT j = 0; j < pLayouts[i].Footprint.Depth; j++)
			{
				BYTE* pDestSlice = reinterpret_cast<BYTE*>(DestData.pData) + DestData.SlicePitch * j;
				const BYTE* pSrcSlice = reinterpret_cast<const BYTE*>(data.pData) + data.SlicePitch * j;
				for (UINT k = 0; k < pNumRows[i]; k++)
				{
					memcpy(pDestSlice + DestData.RowPitch * k, pSrcSlice + data.RowPitch * k, pRowSizesInBytes[i]);
				}
			}
		}
		bufferUpload->Unmap(0, NULL);
		commandList->CopyBufferRegion(buffer.get(), 0, bufferUpload.get(), pLayouts[0].Offset, pLayouts[0].Footprint.Width);
		HeapFree(GetProcessHeap(), 0, pMem);
		D3D12_RESOURCE_BARRIER transition { };
		transition.Transition.pResource = buffer.get();
		transition.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		transition.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		transition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList->ResourceBarrier(1, &transition);
	}

	void MainPage::CleanupFrameData()
	{
		for (auto i = 0; i < frameCount; i++)
		{
			renderTargets[i] = nullptr;
			fenceValues[i] = fenceValues[currentFrame];
		}
	}

	void MainPage::CreateOutputSize(DisplayInformation const& displayInformation, Size& outputSize)
	{
		auto width = max(1, floor(newWidth * displayInformation.LogicalDpi() / 96 + 0.5f));
		auto height = max(1, floor(newHeight * displayInformation.LogicalDpi() / 96 + 0.5f));
		outputSize.Width = width;
		outputSize.Height = height;
	}

	void MainPage::CreateRotation(DisplayInformation const& displayInformation, DXGI_MODE_ROTATION& rotation)
	{
		switch (displayInformation.NativeOrientation())
		{
		case DisplayOrientations::Landscape:
			switch (displayInformation.CurrentOrientation())
			{
			case DisplayOrientations::Landscape:
				rotation = DXGI_MODE_ROTATION_IDENTITY;
				break;

			case DisplayOrientations::Portrait:
				rotation = DXGI_MODE_ROTATION_ROTATE270;
				break;

			case DisplayOrientations::LandscapeFlipped:
				rotation = DXGI_MODE_ROTATION_ROTATE180;
				break;

			case DisplayOrientations::PortraitFlipped:
				rotation = DXGI_MODE_ROTATION_ROTATE90;
				break;
			}
			break;
		case DisplayOrientations::Portrait:
			switch (displayInformation.CurrentOrientation())
			{
			case DisplayOrientations::Landscape:
				rotation = DXGI_MODE_ROTATION_ROTATE90;
				break;

			case DisplayOrientations::Portrait:
				rotation = DXGI_MODE_ROTATION_IDENTITY;
				break;

			case DisplayOrientations::LandscapeFlipped:
				rotation = DXGI_MODE_ROTATION_ROTATE270;
				break;

			case DisplayOrientations::PortraitFlipped:
				rotation = DXGI_MODE_ROTATION_ROTATE180;
				break;
			}
			break;
		}
	}

	void MainPage::CreateRenderTargetSize(DXGI_MODE_ROTATION& rotation, Size const& size, Size& renderTargetSize)
	{
		if (rotation == DXGI_MODE_ROTATION_ROTATE90 || rotation == DXGI_MODE_ROTATION_ROTATE270)
		{
			renderTargetSize.Width = size.Height;
			renderTargetSize.Height = size.Width;
		}
		else
		{
			renderTargetSize.Width = size.Width;
			renderTargetSize.Height = size.Height;
		}
	}

	void MainPage::CreateOrientationTransform3D(DXGI_MODE_ROTATION rotation, XMFLOAT4X4& orientationTransform3D)
	{
		switch (rotation)
		{
		case DXGI_MODE_ROTATION_IDENTITY:
			orientationTransform3D = Rotation0;
			break;

		case DXGI_MODE_ROTATION_ROTATE90:
			orientationTransform3D = Rotation270;
			break;

		case DXGI_MODE_ROTATION_ROTATE180:
			orientationTransform3D = Rotation180;
			break;

		case DXGI_MODE_ROTATION_ROTATE270:
			orientationTransform3D = Rotation90;
			break;

		default:
			throw std::runtime_error("Unknown rotation mode.");
		}
	}

	void MainPage::UpdateSwapChainTransform(DXGI_MODE_ROTATION rotation, float compositionScaleX, float compositionScaleY)
	{
		check_hresult(swapChain->SetRotation(rotation));
		DXGI_MATRIX_3X2_F inverseScale { };
		inverseScale._11 = 1.0f / compositionScaleX;
		inverseScale._22 = 1.0f / compositionScaleY;
		check_hresult(swapChain->SetMatrixTransform(&inverseScale));
	}

	void MainPage::CreateRenderTargetViews()
	{
		auto handle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
		for (auto i = 0; i < frameCount; i++)
		{
			check_hresult(swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i])));
			d3dDevice->CreateRenderTargetView(renderTargets[i].get(), nullptr, handle);
			handle.ptr += rtvDescriptorSize;
			renderTargets[i]->SetName(std::wstring(L"renderTarget" + std::to_wstring(i)).c_str());
		}
	}

	void MainPage::CreateDepthStencilBuffer(UINT width, UINT height)
	{
		D3D12_HEAP_PROPERTIES depthHeapProperties { D3D12_HEAP_TYPE_DEFAULT };
		depthHeapProperties.CreationNodeMask = 1;
		depthHeapProperties.VisibleNodeMask = 1;
		D3D12_RESOURCE_DESC depthResourceDesc { D3D12_RESOURCE_DIMENSION_TEXTURE2D };
		depthResourceDesc.Width = width;
		depthResourceDesc.Height = height;
		depthResourceDesc.DepthOrArraySize = 1;
		depthResourceDesc.Format = DXGI_FORMAT_D32_FLOAT;
		depthResourceDesc.SampleDesc.Count = 1;
		depthResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		D3D12_CLEAR_VALUE depthOptimizedClearValue { DXGI_FORMAT_D32_FLOAT };
		depthOptimizedClearValue.DepthStencil.Depth = 1;
		check_hresult(d3dDevice->CreateCommittedResource(&depthHeapProperties, D3D12_HEAP_FLAG_NONE, &depthResourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthOptimizedClearValue, IID_PPV_ARGS(&depthStencil)));
		depthStencil->SetName(L"depthStencil");
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc { DXGI_FORMAT_D32_FLOAT, D3D12_DSV_DIMENSION_TEXTURE2D };
		d3dDevice->CreateDepthStencilView(depthStencil.get(), &dsvDesc, dsvHeap->GetCPUDescriptorHandleForHeapStart());
	}

	void MainPage::CreateScreen()
	{
		screenTextureDesc.Width = lround(newWidth);
		screenTextureDesc.Height = lround(newHeight);
		check_hresult(d3dDevice->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &screenTextureDesc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&screen)));
		screen->SetName(L"screen");
		screenLocation.pResource = screen.get();
		UINT64 uploadBufferSize = 0;
		d3dDevice->GetCopyableFootprints(&screenTextureDesc, 0, 1, 0, nullptr, nullptr, nullptr, &uploadBufferSize);
		D3D12_RESOURCE_DESC uploadBufferDesc { D3D12_RESOURCE_DIMENSION_BUFFER };
		uploadBufferDesc.Width = uploadBufferSize;
		uploadBufferDesc.Height = 1;
		uploadBufferDesc.DepthOrArraySize = 1;
		uploadBufferDesc.MipLevels = 1;
		uploadBufferDesc.SampleDesc.Count = 1;
		uploadBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		check_hresult(d3dDevice->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &uploadBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&screenUpload)));
		screenUpload->SetName(L"screenUpload");
		screenUploadLocation.pResource = screenUpload.get();
		d3dDevice->GetCopyableFootprints(&screenTextureDesc, 0, 1, 0, &screenLocation.PlacedFootprint, nullptr, nullptr, nullptr);
		newRowbytes = (float)screenLocation.PlacedFootprint.Footprint.RowPitch;
	}

	void MainPage::UpdateModelViewProjectionMatrix(XMFLOAT4X4 const& orientationTransform3D)
	{
		scissorRect = { 0, 0, (LONG)viewport.Width, (LONG)viewport.Height };
		float matrix[16];
		memset(matrix, 0, sizeof(matrix));
		matrix[0] = 1;
		matrix[5] = 1;
		matrix[10] = -1;
		matrix[15] = 1;
		XMMATRIX perspectiveMatrix(matrix);
		auto orientationMatrix = XMLoadFloat4x4(&orientationTransform3D);
		XMStoreFloat4x4(&constantBufferData.modelViewProjection, XMMatrixTranspose(perspectiveMatrix * orientationMatrix));
	}

	bool MainPage::DisplaySysErrorIfNeeded()
	{
		if (sys_errormessage.length() > 0)
		{
			if (sys_nogamedata)
			{
				MessageDialog msg(L"Go to Preferences and set Game directory(-basedir) to your copy of the game files, or add it to the Command line.", L"Game data not found");
				Windows::UI::Popups::UICommand goToPreferences(L"Go to Preferences", [this](IUICommand const &) {
					//AppDelegate* appDelegate = NSApplication.sharedApplication.delegate;
					//[appDelegate preferences : nil] ;
					Application::Current().Exit();
					});
				Windows::UI::Popups::UICommand buyGame(L"Where to buy the game", [this](IUICommand const&) {
					Launcher::LaunchUriAsync(Uri(L"https://www.google.com/search?q=buy+quake+1+game"));
					});
				Windows::UI::Popups::UICommand downloadSharewareGame(L"Where to get shareware episode", [this](IUICommand const&) {
					Launcher::LaunchUriAsync(Uri(L"https://www.google.com/search?q=download+quake+1+shareware+episode"));
					});
				msg.Commands().Append(goToPreferences);
				msg.Commands().Append(buyGame);
				msg.Commands().Append(downloadSharewareGame);
				msg.DefaultCommandIndex(0);
				msg.CancelCommandIndex(0);
				msg.ShowAsync();
			}
			else
			{
				std::wstring toDisplay;
				for (auto c : sys_errormessage)
				{
					toDisplay.push_back((wchar_t)c);
				}
				MessageDialog msg(toDisplay, L"Sys_Error");
				Windows::UI::Popups::UICommand okButton(L"Ok", [this](IUICommand const&) {
					//AppDelegate* appDelegate = NSApplication.sharedApplication.delegate;
					//[appDelegate preferences : nil] ;
					Application::Current().Exit();
					});
				msg.Commands().Append(okButton);
				msg.DefaultCommandIndex(0);
				msg.CancelCommandIndex(0);
				msg.ShowAsync();
			}
			return true;
		}
		return false;
	}

	void MainPage::ProcessFiles()
	{
		do
		{
			std::this_thread::yield();
			if (fileOperationRunning)
			{
				continue;
			}
			if (sys_fileoperation == fo_openread)
			{
				OpenFileForRead();
			}
			else if (sys_fileoperation == fo_openwrite)
			{
				OpenFileForWrite();
			}
			else if (sys_fileoperation == fo_openappend)
			{
				OpenFileForAppend();
			}
			else if (sys_fileoperation == fo_read)
			{
				ReadFile();
			}
			else if (sys_fileoperation == fo_write)
			{
				WriteToFile();
			}
			else if (sys_fileoperation == fo_time)
			{
				GetFileTime();
			}
		} while (true);
	}

	void MainPage::OpenFileForRead()
	{
		if (!StorageApplicationPermissions::FutureAccessList().ContainsItem(L"basedir_text"))
		{
			sys_fileoperationerror = "basedir_text token not found in FutureAccessList";
			sys_fileoperation = fo_error;
			return;
		}
		fileOperationRunning = true;
		auto task = StorageApplicationPermissions::FutureAccessList().GetFolderAsync(L"basedir_text");
		task.Completed([this](IAsyncOperation<StorageFolder> const& operation, AsyncStatus const status)
			{
				if (status == AsyncStatus::Error)
				{
					sys_fileoperationerror = std::to_string(operation.ErrorCode());
					sys_fileoperation = fo_error;
					fileOperationRunning = false;
					return;
				}
				auto folder = operation.GetResults();
				auto path = folder.Path();
				auto i = 0;
				for (; i < (int)sys_fileoperationname.length() && i < (int)path.size(); i++)
				{
					auto c = (wchar_t)sys_fileoperationname[i];
					if (c != path[i])
					{
						break;
					}
				}
				if (i != (int)path.size() || i >= (int)sys_fileoperationname.length())
				{
					sys_fileoperationerror = "Filename is not a subpath of specified folder";
					sys_fileoperation = fo_error;
					fileOperationRunning = false;
					return;
				}
				if (sys_fileoperationname[i] == '/' || sys_fileoperationname[i] == '\\')
				{
					i++;
				}
				std::wstring remaining;
				for (; i < sys_fileoperationname.length(); i++)
				{
					auto c = (wchar_t)sys_fileoperationname[i];
					if (c == '/')
					{
						c = '\\';
					}
					remaining.push_back(c);
				}
				auto task = folder.GetFileAsync(remaining);
				task.Completed([this](IAsyncOperation<StorageFile> const& operation, AsyncStatus const status)
					{
						if (status == AsyncStatus::Error)
						{
							sys_fileoperationerror = std::to_string(operation.ErrorCode());
							sys_fileoperation = fo_error;
							fileOperationRunning = false;
							return;
						}
						sys_files[sys_fileoperationindex].file = operation.GetResults();
						auto task = sys_files[sys_fileoperationindex].file.GetBasicPropertiesAsync();
						task.Completed([this](IAsyncOperation<BasicProperties> const& operation, AsyncStatus const status)
							{
								if (status == AsyncStatus::Error)
								{
									sys_fileoperationerror = std::to_string(operation.ErrorCode());
									sys_fileoperation = fo_error;
									fileOperationRunning = false;
									return;
								}
								auto basicProperties = operation.GetResults();
								sys_fileoperationsize = (int)basicProperties.Size();
								auto task = sys_files[sys_fileoperationindex].file.OpenAsync(FileAccessMode::Read);
								task.Completed([this](IAsyncOperation<IRandomAccessStream> const& operation, AsyncStatus const status)
									{
										if (status == AsyncStatus::Error)
										{
											sys_fileoperationerror = std::to_string(operation.ErrorCode());
											sys_fileoperation = fo_error;
											fileOperationRunning = false;
											return;
										}
										sys_files[sys_fileoperationindex].stream = operation.GetResults();
										sys_fileoperation = fo_idle;
										fileOperationRunning = false;
									});
							});
					});
			});
	}

	void MainPage::OpenFileForWrite()
	{
		if (!StorageApplicationPermissions::FutureAccessList().ContainsItem(L"basedir_text"))
		{
			sys_fileoperationerror = "basedir_text token not found in FutureAccessList";
			sys_fileoperation = fo_error;
			return;
		}
		fileOperationRunning = true;
		auto task = StorageApplicationPermissions::FutureAccessList().GetFolderAsync(L"basedir_text");
		task.Completed([this](IAsyncOperation<StorageFolder> const& operation, AsyncStatus const status)
			{
				if (status == AsyncStatus::Error)
				{
					sys_fileoperationerror = std::to_string(operation.ErrorCode());
					sys_fileoperation = fo_error;
					fileOperationRunning = false;
					return;
				}
				auto folder = operation.GetResults();
				auto path = folder.Path();
				auto i = 0;
				for (; i < (int)sys_fileoperationname.length() && i < (int)path.size(); i++)
				{
					auto c = (wchar_t)sys_fileoperationname[i];
					if (c != path[i])
					{
						break;
					}
				}
				if (i != (int)path.size() || i >= (int)sys_fileoperationname.length())
				{
					sys_fileoperationerror = "Filename is not a subpath of specified folder";
					sys_fileoperation = fo_error;
					fileOperationRunning = false;
					return;
				}
				if (sys_fileoperationname[i] == '/' || sys_fileoperationname[i] == '\\')
				{
					i++;
				}
				std::wstring remaining;
				for (; i < sys_fileoperationname.length(); i++)
				{
					auto c = (wchar_t)sys_fileoperationname[i];
					if (c == '/')
					{
						c = '\\';
					}
					remaining.push_back(c);
				}
				auto task = folder.GetFileAsync(remaining);
				task.Completed([this](IAsyncOperation<StorageFile> const& operation, AsyncStatus const status)
					{
						if (status == AsyncStatus::Error)
						{
							sys_fileoperationerror = std::to_string(operation.ErrorCode());
							sys_fileoperation = fo_error;
							fileOperationRunning = false;
							return;
						}
						sys_files[sys_fileoperationindex].file = operation.GetResults();
						auto task = sys_files[sys_fileoperationindex].file.OpenAsync(FileAccessMode::ReadWrite);
						task.Completed([this](IAsyncOperation<IRandomAccessStream> const& operation, AsyncStatus const status)
							{
								if (status == AsyncStatus::Error)
								{
									sys_fileoperationerror = std::to_string(operation.ErrorCode());
									sys_fileoperation = fo_error;
									fileOperationRunning = false;
									return;
								}
								sys_files[sys_fileoperationindex].stream = operation.GetResults();
								sys_fileoperation = fo_idle;
								fileOperationRunning = false;
							});
					});
			});
	}

	void MainPage::OpenFileForAppend()
	{
		if (!StorageApplicationPermissions::FutureAccessList().ContainsItem(L"basedir_text"))
		{
			sys_fileoperationerror = "basedir_text token not found in FutureAccessList";
			sys_fileoperation = fo_error;
			return;
		}
		fileOperationRunning = true;
		auto task = StorageApplicationPermissions::FutureAccessList().GetFolderAsync(L"basedir_text");
		task.Completed([this](IAsyncOperation<StorageFolder> const& operation, AsyncStatus const status)
			{
				if (status == AsyncStatus::Error)
				{
					sys_fileoperationerror = std::to_string(operation.ErrorCode());
					sys_fileoperation = fo_error;
					fileOperationRunning = false;
					return;
				}
				auto folder = operation.GetResults();
				auto path = folder.Path();
				auto i = 0;
				for (; i < (int)sys_fileoperationname.length() && i < (int)path.size(); i++)
				{
					auto c = (wchar_t)sys_fileoperationname[i];
					if (c != path[i])
					{
						break;
					}
				}
				if (i != (int)path.size() || i >= (int)sys_fileoperationname.length())
				{
					sys_fileoperationerror = "Filename is not a subpath of specified folder";
					sys_fileoperation = fo_error;
					fileOperationRunning = false;
					return;
				}
				if (sys_fileoperationname[i] == '/' || sys_fileoperationname[i] == '\\')
				{
					i++;
				}
				std::wstring remaining;
				for (; i < sys_fileoperationname.length(); i++)
				{
					auto c = (wchar_t)sys_fileoperationname[i];
					if (c == '/')
					{
						c = '\\';
					}
					remaining.push_back(c);
				}
				auto task = folder.GetFileAsync(remaining);
				task.Completed([this](IAsyncOperation<StorageFile> const& operation, AsyncStatus const status)
					{
						if (status == AsyncStatus::Error)
						{
							sys_fileoperationerror = std::to_string(operation.ErrorCode());
							sys_fileoperation = fo_error;
							fileOperationRunning = false;
							return;
						}
						sys_files[sys_fileoperationindex].file = operation.GetResults();
						auto task = sys_files[sys_fileoperationindex].file.GetBasicPropertiesAsync();
						task.Completed([this](IAsyncOperation<BasicProperties> const& operation, AsyncStatus const status)
							{
								if (status == AsyncStatus::Error)
								{
									sys_fileoperationerror = std::to_string(operation.ErrorCode());
									sys_fileoperation = fo_error;
									fileOperationRunning = false;
									return;
								}
								auto basicProperties = operation.GetResults();
								sys_fileoperationsize = (int)basicProperties.Size();
								auto task = sys_files[sys_fileoperationindex].file.OpenAsync(FileAccessMode::ReadWrite);
								task.Completed([this](IAsyncOperation<IRandomAccessStream> const& operation, AsyncStatus const status)
									{
										if (status == AsyncStatus::Error)
										{
											sys_fileoperationerror = std::to_string(operation.ErrorCode());
											sys_fileoperation = fo_error;
											fileOperationRunning = false;
											return;
										}
										sys_files[sys_fileoperationindex].stream = operation.GetResults();
										sys_files[sys_fileoperationindex].stream.Seek(sys_fileoperationsize);
										sys_fileoperation = fo_idle;
										fileOperationRunning = false;
									});
							});
					});
			});
	}

	void MainPage::ReadFile()
	{
		Buffer buffer(sys_fileoperationsize);
		sys_fileoperationsize = 0;
		fileOperationRunning = true;
		auto task = sys_files[sys_fileoperationindex].stream.ReadAsync(buffer, buffer.Capacity(), InputStreamOptions::None);
		task.Progress([this](IAsyncOperationWithProgress<IBuffer, uint32_t> const& operation, uint32_t const)
			{
				auto buffer = operation.GetResults();
				memcpy(sys_fileoperationbuffer + sys_fileoperationsize, buffer.data(), buffer.Length());
				sys_fileoperationsize += buffer.Length();
			});
		task.Completed([this](IAsyncOperationWithProgress<IBuffer, uint32_t> const& operation, AsyncStatus const status)
			{
				if (status == AsyncStatus::Error)
				{
					sys_fileoperationerror = std::to_string(operation.ErrorCode());
					sys_fileoperation = fo_error;
					fileOperationRunning = false;
					return;
				}
				auto buffer = operation.GetResults();
				memcpy(sys_fileoperationbuffer + sys_fileoperationsize, buffer.data(), buffer.Length());
				sys_fileoperationsize += buffer.Length();
				sys_fileoperation = fo_idle;
				fileOperationRunning = false;
			});
	}

	void MainPage::WriteToFile()
	{
		Buffer buffer(sys_fileoperationsize);
		memcpy(buffer.data(), sys_fileoperationbuffer, sys_fileoperationsize);
		fileOperationRunning = true;
		auto task = sys_files[sys_fileoperationindex].stream.WriteAsync(buffer);
		task.Completed([this](IAsyncOperationWithProgress<uint32_t, uint32_t> const& operation, AsyncStatus const status)
			{
				if (status == AsyncStatus::Error)
				{
					sys_fileoperationerror = std::to_string(operation.ErrorCode());
					sys_fileoperation = fo_error;
					fileOperationRunning = false;
					return;
				}
				sys_fileoperation = fo_idle;
				fileOperationRunning = false;
			});
	}

	void MainPage::GetFileTime()
	{
		if (!StorageApplicationPermissions::FutureAccessList().ContainsItem(L"basedir_text"))
		{
			sys_fileoperationerror = "basedir_text token not found in FutureAccessList";
			sys_fileoperation = fo_error;
			return;
		}
		fileOperationRunning = true;
		auto task = StorageApplicationPermissions::FutureAccessList().GetFolderAsync(L"basedir_text");
		task.Completed([this](IAsyncOperation<StorageFolder> const& operation, AsyncStatus const status)
			{
				if (status == AsyncStatus::Error)
				{
					sys_fileoperationerror = std::to_string(operation.ErrorCode());
					sys_fileoperation = fo_error;
					fileOperationRunning = false;
					return;
				}
				auto folder = operation.GetResults();
				auto path = folder.Path();
				auto i = 0;
				for (; i < (int)sys_fileoperationname.length() && i < (int)path.size(); i++)
				{
					auto c = (wchar_t)sys_fileoperationname[i];
					if (c != path[i])
					{
						break;
					}
				}
				if (i != (int)path.size() || i >= (int)sys_fileoperationname.length())
				{
					sys_fileoperationerror = "Filename is not a subpath of specified folder";
					sys_fileoperation = fo_error;
					fileOperationRunning = false;
					return;
				}
				if (sys_fileoperationname[i] == '/' || sys_fileoperationname[i] == '\\')
				{
					i++;
				}
				std::wstring remaining;
				for (; i < sys_fileoperationname.length(); i++)
				{
					auto c = (wchar_t)sys_fileoperationname[i];
					if (c == '/')
					{
						c = '\\';
					}
					remaining.push_back(c);
				}
				auto task = folder.GetFileAsync(remaining);
				task.Completed([this](IAsyncOperation<StorageFile> const& operation, AsyncStatus const status)
					{
						if (status == AsyncStatus::Error)
						{
							sys_fileoperationerror = std::to_string(operation.ErrorCode());
							sys_fileoperation = fo_error;
							fileOperationRunning = false;
							return;
						}
						sys_files[sys_fileoperationindex].file = operation.GetResults();
						auto task = sys_files[sys_fileoperationindex].file.GetBasicPropertiesAsync();
						task.Completed([this](IAsyncOperation<BasicProperties> const& operation, AsyncStatus const status)
							{
								if (status == AsyncStatus::Error)
								{
									sys_fileoperationerror = std::to_string(operation.ErrorCode());
									sys_fileoperation = fo_error;
									fileOperationRunning = false;
									return;
								}
								auto basicProperties = operation.GetResults();
								sys_fileoperationtime = (int)basicProperties.ItemDate().time_since_epoch().count();
								sys_fileoperation = fo_idle;
								fileOperationRunning = false;
							});
					});
			});
	}
}
