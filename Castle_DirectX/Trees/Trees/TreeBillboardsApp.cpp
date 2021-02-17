//***************************************************************************************
// TreeBillboardsApp.cpp by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"
#include "../../Common/GeometryGenerator.h"
#include "../../Common/Camera.h"
#include "FrameResource.h"
#include "Waves.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

const int gNumFrameResources = 3;

// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.
struct RenderItem
{
	RenderItem() = default;

    // World matrix of the shape that describes the object's local space
    // relative to the world space, which defines the position, orientation,
    // and scale of the object in the world.
    XMFLOAT4X4 World = MathHelper::Identity4x4();

	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

    // Primitive topology.
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // DrawIndexedInstanced parameters.
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
};

enum class RenderLayer : int
{
	Opaque = 0,
	Transparent,
	AlphaTested,
	AlphaTestedTreeSprites,
	Count
};

class TreeBillboardsApp : public D3DApp
{
public:
    TreeBillboardsApp(HINSTANCE hInstance);
    TreeBillboardsApp(const TreeBillboardsApp& rhs) = delete;
    TreeBillboardsApp& operator=(const TreeBillboardsApp& rhs) = delete;
    ~TreeBillboardsApp();

    virtual bool Initialize()override;

private:
    virtual void OnResize()override;
    virtual void Update(const GameTimer& gt)override;
    virtual void Draw(const GameTimer& gt)override;

    virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

    void OnKeyboardInput(const GameTimer& gt);
	void UpdateCamera(const GameTimer& gt);
	void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateWaves(const GameTimer& gt); 

	void LoadTextures();
    void BuildRootSignature();
	void BuildDescriptorHeaps();
    void BuildShadersAndInputLayouts();
    void BuildLandGeometry();
    void BuildWavesGeometry();
	void BuildBoxGeometry();
	void BuildTreeSpritesGeometry();
	void BuildShapeGeometry();
    void BuildPSOs();
    void BuildFrameResources();
    void BuildMaterials();
    void BuildRenderItems();
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

    float GetHillsHeight(float x, float z)const;
    XMFLOAT3 GetHillsNormal(float x, float z)const;

private:
	void BuildBox(int cbIndex, const XMMATRIX& tranlate, const XMMATRIX& scale);

    std::vector<std::unique_ptr<FrameResource>> mFrameResources;
    FrameResource* mCurrFrameResource = nullptr;
    int mCurrFrameResourceIndex = 0;

    UINT mCbvSrvDescriptorSize = 0;

    ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mStdInputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mTreeSpriteInputLayout;

    RenderItem* mWavesRitem = nullptr;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	// Render items divided by PSO.
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

	std::unique_ptr<Waves> mWaves;

    PassConstants mMainPassCB;

	//XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	//XMFLOAT4X4 mView = MathHelper::Identity4x4();
	//XMFLOAT4X4 mProj = MathHelper::Identity4x4();

   // float mTheta = 1.5f*XM_PI;
    //float mPhi = XM_PIDIV2 - 0.1f;
   // float mRadius = 50.0f;

	Camera mCamera;

    POINT mLastMousePos;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
    PSTR cmdLine, int showCmd)
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    try
    {
        TreeBillboardsApp theApp(hInstance);
        if(!theApp.Initialize())
            return 0;

        return theApp.Run();
    }
    catch(DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}

TreeBillboardsApp::TreeBillboardsApp(HINSTANCE hInstance)
    : D3DApp(hInstance)
{
}

TreeBillboardsApp::~TreeBillboardsApp()
{
    if(md3dDevice != nullptr)
        FlushCommandQueue();
}

bool TreeBillboardsApp::Initialize()
{
    if(!D3DApp::Initialize())
        return false;

    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    // Get the increment size of a descriptor in this heap type.  This is hardware specific, 
	// so we have to query this information.
    mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    mWaves = std::make_unique<Waves>(128, 128, 1.0f, 0.03f, 4.0f, 0.2f);
 
	LoadTextures();
    BuildRootSignature();
	BuildDescriptorHeaps();
    BuildShadersAndInputLayouts();
    BuildLandGeometry();
    BuildWavesGeometry();
	BuildBoxGeometry();
	BuildTreeSpritesGeometry();
	BuildShapeGeometry();
	BuildMaterials();
    BuildRenderItems();
    BuildFrameResources();
    BuildPSOs();

    // Execute the initialization commands.
    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Wait until initialization is complete.
    FlushCommandQueue();

    return true;
}
 
void TreeBillboardsApp::OnResize()
{
    D3DApp::OnResize();

    // The window resized, so update the aspect ratio and recompute the projection matrix.
    //XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    //XMStoreFloat4x4(&mProj, P);
}

void TreeBillboardsApp::Update(const GameTimer& gt)
{
    OnKeyboardInput(gt);
	//UpdateCamera(gt);

    // Cycle through the circular frame resource array.
    mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
    mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

    // Has the GPU finished processing the commands of the current frame resource?
    // If not, wait until the GPU has completed commands up to this fence point.
    if(mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

	AnimateMaterials(gt);
	UpdateObjectCBs(gt);
	UpdateMaterialCBs(gt);
	UpdateMainPassCB(gt);
    UpdateWaves(gt);
}

void TreeBillboardsApp::Draw(const GameTimer& gt)
{
    auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU.
    ThrowIfFailed(cmdListAlloc->Reset());

    // A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.
    ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    // Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    // Clear the back buffer and depth buffer.
    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), (float*)&mMainPassCB.FogColor, 0, nullptr);
    mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // Specify the buffers we are going to render to.
    mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

    DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

	mCommandList->SetPipelineState(mPSOs["alphaTested"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::AlphaTested]);

	mCommandList->SetPipelineState(mPSOs["treeSprites"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::AlphaTestedTreeSprites]);

	mCommandList->SetPipelineState(mPSOs["transparent"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Transparent]);

    // Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // Done recording commands.
    ThrowIfFailed(mCommandList->Close());

    // Add the command list to the queue for execution.
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Swap the back and front buffers
    ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    // Advance the fence value to mark commands up to this fence point.
    mCurrFrameResource->Fence = ++mCurrentFence;

    // Add an instruction to the command queue to set a new fence point. 
    // Because we are on the GPU timeline, the new fence point won't be 
    // set until the GPU finishes processing all the commands prior to this Signal().
    mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void TreeBillboardsApp::OnMouseDown(WPARAM btnState, int x, int y)
{
    mLastMousePos.x = x;
    mLastMousePos.y = y;

    SetCapture(mhMainWnd);
}

void TreeBillboardsApp::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void TreeBillboardsApp::OnMouseMove(WPARAM btnState, int x, int y)
{
    if((btnState & MK_LBUTTON) != 0)
    {
        // Make each pixel correspond to a quarter of a degree.
        float dx = XMConvertToRadians(0.25f*static_cast<float>(x - mLastMousePos.x));
        float dy = XMConvertToRadians(0.25f*static_cast<float>(y - mLastMousePos.y));

        // Update angles based on input to orbit camera around box.
        //mTheta += dx;
        //mPhi += dy;

        // Restrict the angle mPhi.
        //mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
		mCamera.Pitch(dy);
		mCamera.RotateY(dx);
    }
    /*else if((btnState & MK_RBUTTON) != 0)
    {
        // Make each pixel correspond to 0.2 unit in the scene.
        float dx = 0.2f*static_cast<float>(x - mLastMousePos.x);
        float dy = 0.2f*static_cast<float>(y - mLastMousePos.y);

        // Update the camera radius based on input.
        mRadius += dx - dy;

        // Restrict the radius.
        mRadius = MathHelper::Clamp(mRadius, 5.0f, 150.0f);
    }*/

    mLastMousePos.x = x;
    mLastMousePos.y = y;
}
 
void TreeBillboardsApp::OnKeyboardInput(const GameTimer& gt)
{
	const float dt = gt.DeltaTime();

	//GetAsyncKeyState returns a short (2 bytes)
	if (GetAsyncKeyState('W') & 0x8000) //most significant bit (MSB) is 1 when key is pressed (1000 000 000 000)
		mCamera.Walk(20.0f*dt);

	if (GetAsyncKeyState('S') & 0x8000)
		mCamera.Walk(-20.0f*dt);

	if (GetAsyncKeyState('A') & 0x8000)
		mCamera.Strafe(-20.0f*dt);

	if (GetAsyncKeyState('D') & 0x8000)
		mCamera.Strafe(20.0f*dt);

	mCamera.UpdateViewMatrix();
}
 
/*void TreeBillboardsApp::UpdateCamera(const GameTimer& gt)
{
	// Convert Spherical to Cartesian coordinates.
	mEyePos.x = mRadius*sinf(mPhi)*cosf(mTheta);
	mEyePos.z = mRadius*sinf(mPhi)*sinf(mTheta);
	mEyePos.y = mRadius*cosf(mPhi);

	// Build the view matrix.
	XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);
}*/

void TreeBillboardsApp::AnimateMaterials(const GameTimer& gt)
{
	// Scroll the water material texture coordinates.
	auto waterMat = mMaterials["water"].get();

	float& tu = waterMat->MatTransform(3, 0);
	float& tv = waterMat->MatTransform(3, 1);

	tu += 0.1f * gt.DeltaTime();
	tv += 0.02f * gt.DeltaTime();

	if(tu >= 1.0f)
		tu -= 1.0f;

	if(tv >= 1.0f)
		tv -= 1.0f;

	waterMat->MatTransform(3, 0) = tu;
	waterMat->MatTransform(3, 1) = tv;

	// Material has changed, so need to update cbuffer.
	waterMat->NumFramesDirty = gNumFrameResources;
}

void TreeBillboardsApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for(auto& e : mAllRitems)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if(e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}
}

void TreeBillboardsApp::UpdateMaterialCBs(const GameTimer& gt)
{
	auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
	for(auto& e : mMaterials)
	{
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = e.second.get();
		if(mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialConstants matConstants;
			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstants.FresnelR0 = mat->FresnelR0;
			matConstants.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));

			currMaterialCB->CopyData(mat->MatCBIndex, matConstants);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

void TreeBillboardsApp::UpdateMainPassCB(const GameTimer& gt)
{
	//XMMATRIX view = XMLoadFloat4x4(&mView);
	//XMMATRIX proj = XMLoadFloat4x4(&mProj);

	XMMATRIX view = mCamera.GetView();
	XMMATRIX proj = mCamera.GetProj();

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	//mMainPassCB.EyePosW = mEyePos;
	mMainPassCB.EyePosW = mCamera.GetPosition3f();
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();
	mMainPassCB.AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };
	mMainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[0].Strength = { 0.0f, 1.0f, 0.0f };
	//mMainPassCB.Lights[0].Color = { 0.0f, 1.0f, 0.0f };
	mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[1].Strength = { 1.0f, 0.0f, 0.0f };
	//mMainPassCB.Lights[1].Color = { 1.0f, 0.0f, 0.0f };
	mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.Lights[2].Strength = { 0.0f, 0.0f, 1.0f };
	//mMainPassCB.Lights[2].Color = { 0.0f, 0.0f, 1.0f };
	//mMainPassCB.Lights[0].Position = { 0.0f, 0.0f, 20.0f };

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void TreeBillboardsApp::UpdateWaves(const GameTimer& gt)
{
	// Every quarter second, generate a random wave.
	static float t_base = 0.0f;
	if((mTimer.TotalTime() - t_base) >= 0.25f)
	{
		t_base += 0.25f;

		int i = MathHelper::Rand(4, mWaves->RowCount() - 5);
		int j = MathHelper::Rand(4, mWaves->ColumnCount() - 5);

		float r = MathHelper::RandF(0.2f, 0.5f);

		mWaves->Disturb(i, j, r);
	}

	// Update the wave simulation.
	mWaves->Update(gt.DeltaTime());

	// Update the wave vertex buffer with the new solution.
	auto currWavesVB = mCurrFrameResource->WavesVB.get();
	for(int i = 0; i < mWaves->VertexCount(); ++i)
	{
		Vertex v;

		v.Pos = mWaves->Position(i);
		v.Normal = mWaves->Normal(i);
		
		// Derive tex-coords from position by 
		// mapping [-w/2,w/2] --> [0,1]
		v.TexC.x = 0.5f + v.Pos.x / mWaves->Width();
		v.TexC.y = 0.5f - v.Pos.z / mWaves->Depth();

		currWavesVB->CopyData(i, v);
	}

	// Set the dynamic VB of the wave renderitem to the current frame VB.
	mWavesRitem->Geo->VertexBufferGPU = currWavesVB->Resource();
}

void TreeBillboardsApp::LoadTextures()
{
	auto grassTex = std::make_unique<Texture>();
	grassTex->Name = "grassTex";
	grassTex->Filename = L"../../Textures/grass.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), grassTex->Filename.c_str(),
		grassTex->Resource, grassTex->UploadHeap));

	auto waterTex = std::make_unique<Texture>();
	waterTex->Name = "waterTex";
	waterTex->Filename = L"../../Textures/water1.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), waterTex->Filename.c_str(),
		waterTex->Resource, waterTex->UploadHeap));

	auto fenceTex = std::make_unique<Texture>();
	fenceTex->Name = "fenceTex";
	fenceTex->Filename = L"../../Textures/WireFence.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), fenceTex->Filename.c_str(),
		fenceTex->Resource, fenceTex->UploadHeap));

	auto treeArrayTex = std::make_unique<Texture>();
	treeArrayTex->Name = "treeArrayTex";
	treeArrayTex->Filename = L"../../Textures/treeArray2.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), treeArrayTex->Filename.c_str(),
		treeArrayTex->Resource, treeArrayTex->UploadHeap));

	auto brickTex = std::make_unique<Texture>();
	brickTex->Name = "brickTex";
	brickTex->Filename = L"../../Textures/bricks.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), brickTex->Filename.c_str(),
		brickTex->Resource, brickTex->UploadHeap));

	auto ballTex = std::make_unique<Texture>();
	ballTex->Name = "ballTex";
	ballTex->Filename = L"../../Textures/sphere.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), ballTex->Filename.c_str(),
		ballTex->Resource, ballTex->UploadHeap));

	auto darkBrickTex = std::make_unique<Texture>();
	darkBrickTex->Name = "darkBrickTex";
	darkBrickTex->Filename = L"../../Textures/bricks.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), darkBrickTex->Filename.c_str(),
		darkBrickTex->Resource, darkBrickTex->UploadHeap));

	auto darkLightBrickTex = std::make_unique<Texture>();
	darkLightBrickTex->Name = "darkLightBrickTex";
	darkLightBrickTex->Filename = L"../../Textures/bricks2.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), darkLightBrickTex->Filename.c_str(),
		darkLightBrickTex->Resource, darkLightBrickTex->UploadHeap));

	auto lightBrickTex = std::make_unique<Texture>();
	lightBrickTex->Name = "lightBrickTex";
	lightBrickTex->Filename = L"../../Textures/bricks3.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), lightBrickTex->Filename.c_str(),
		lightBrickTex->Resource, lightBrickTex->UploadHeap));

	auto redTileTex = std::make_unique<Texture>();
	redTileTex->Name = "redTileTex";
	redTileTex->Filename = L"../../Textures/redTile.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), redTileTex->Filename.c_str(),
		redTileTex->Resource, redTileTex->UploadHeap));

	auto glassTex = std::make_unique<Texture>();
	glassTex->Name = "glassTex";
	glassTex->Filename = L"../../Textures/glass.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), glassTex->Filename.c_str(),
		glassTex->Resource, glassTex->UploadHeap));

	auto sandTex = std::make_unique<Texture>();
	sandTex->Name = "sandTex";
	sandTex->Filename = L"../../Textures/stone.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), sandTex->Filename.c_str(),
		sandTex->Resource, sandTex->UploadHeap));

	mTextures[grassTex->Name] = std::move(grassTex);
	mTextures[waterTex->Name] = std::move(waterTex);
	mTextures[fenceTex->Name] = std::move(fenceTex);
	mTextures[treeArrayTex->Name] = std::move(treeArrayTex);
	mTextures[brickTex->Name] = std::move(brickTex);
	mTextures[ballTex->Name] = std::move(ballTex);
	mTextures[darkBrickTex->Name] = std::move(darkBrickTex);
	mTextures[darkLightBrickTex->Name] = std::move(darkLightBrickTex);
	mTextures[lightBrickTex->Name] = std::move(lightBrickTex);
	mTextures[redTileTex->Name] = std::move(redTileTex);
	mTextures[glassTex->Name] = std::move(glassTex);
	mTextures[sandTex->Name] = std::move(sandTex);
}

void TreeBillboardsApp::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
    slotRootParameter[1].InitAsConstantBufferView(0);
    slotRootParameter[2].InitAsConstantBufferView(1);
    slotRootParameter[3].InitAsConstantBufferView(2);

	auto staticSamplers = GetStaticSamplers();

    // A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    // create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

    if(errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void TreeBillboardsApp::BuildDescriptorHeaps()
{
	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 12;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	auto grassTex = mTextures["grassTex"]->Resource;
	auto waterTex = mTextures["waterTex"]->Resource;
	auto fenceTex = mTextures["fenceTex"]->Resource;
	auto treeArrayTex = mTextures["treeArrayTex"]->Resource;
	auto brickTex = mTextures["brickTex"]->Resource;
	auto ballTex = mTextures["ballTex"]->Resource;
	auto darkBrickTex = mTextures["darkBrickTex"]->Resource;
	auto darkLightBrickTex = mTextures["darkLightBrickTex"]->Resource;
	auto lightBrickTex = mTextures["lightBrickTex"]->Resource;
	auto redTileTex = mTextures["redTileTex"]->Resource;
	auto glassTex = mTextures["glassTex"]->Resource;
	auto sandTex = mTextures["sandTex"]->Resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = grassTex->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = -1;
	md3dDevice->CreateShaderResourceView(grassTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = waterTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(waterTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = fenceTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(fenceTex.Get(), &srvDesc, hDescriptor);


	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = brickTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(brickTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = ballTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(ballTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = darkBrickTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(darkBrickTex.Get(), &srvDesc, hDescriptor);


	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = darkLightBrickTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(darkLightBrickTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = lightBrickTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(lightBrickTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = redTileTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(redTileTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = glassTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(glassTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = sandTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(sandTex.Get(), &srvDesc, hDescriptor);


	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	auto desc = treeArrayTex->GetDesc();
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
	srvDesc.Format = treeArrayTex->GetDesc().Format;
	srvDesc.Texture2DArray.MostDetailedMip = 0;
	srvDesc.Texture2DArray.MipLevels = -1;
	srvDesc.Texture2DArray.FirstArraySlice = 0;
	srvDesc.Texture2DArray.ArraySize = treeArrayTex->GetDesc().DepthOrArraySize;
	md3dDevice->CreateShaderResourceView(treeArrayTex.Get(), &srvDesc, hDescriptor);


}

void TreeBillboardsApp::BuildShadersAndInputLayouts()
{
	const D3D_SHADER_MACRO defines[] =
	{
		"FOG", "1",
		NULL, NULL
	};

	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"FOG", "1",
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", defines, "PS", "ps_5_0");
	mShaders["alphaTestedPS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", alphaTestDefines, "PS", "ps_5_0");
	
	mShaders["treeSpriteVS"] = d3dUtil::CompileShader(L"Shaders\\TreeSprite.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["treeSpriteGS"] = d3dUtil::CompileShader(L"Shaders\\TreeSprite.hlsl", nullptr, "GS", "gs_5_0");
	mShaders["treeSpritePS"] = d3dUtil::CompileShader(L"Shaders\\TreeSprite.hlsl", alphaTestDefines, "PS", "ps_5_0");

    mStdInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

	mTreeSpriteInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "SIZE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}

void TreeBillboardsApp::BuildLandGeometry()
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData grid = geoGen.CreateGrid(160.0f, 160.0f, 50, 50);

    //
    // Extract the vertex elements we are interested and apply the height function to
    // each vertex.  In addition, color the vertices based on their height so we have
    // sandy looking beaches, grassy low hills, and snow mountain peaks.
    //

    std::vector<Vertex> vertices(grid.Vertices.size());
    for(size_t i = 0; i < grid.Vertices.size(); ++i)
    {
        auto& p = grid.Vertices[i].Position;
        vertices[i].Pos = p;
        vertices[i].Pos.y = GetHillsHeight(p.x, p.z);
        vertices[i].Normal = GetHillsNormal(p.x, p.z);
		vertices[i].TexC = grid.Vertices[i].TexC;
    }

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

    std::vector<std::uint16_t> indices = grid.GetIndices16();
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "landGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["grid"] = submesh;

	mGeometries["landGeo"] = std::move(geo);
}

void TreeBillboardsApp::BuildWavesGeometry()
{
    std::vector<std::uint16_t> indices(3 * mWaves->TriangleCount()); // 3 indices per face
	assert(mWaves->VertexCount() < 0x0000ffff);

    // Iterate over each quad.
    int m = mWaves->RowCount();
    int n = mWaves->ColumnCount();
    int k = 0;
    for(int i = 0; i < m - 1; ++i)
    {
        for(int j = 0; j < n - 1; ++j)
        {
            indices[k] = i*n + j;
            indices[k + 1] = i*n + j + 1;
            indices[k + 2] = (i + 1)*n + j;

            indices[k + 3] = (i + 1)*n + j;
            indices[k + 4] = i*n + j + 1;
            indices[k + 5] = (i + 1)*n + j + 1;

            k += 6; // next quad
        }
    }

	UINT vbByteSize = mWaves->VertexCount()*sizeof(Vertex);
	UINT ibByteSize = (UINT)indices.size()*sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "waterGeo";

	// Set dynamically.
	geo->VertexBufferCPU = nullptr;
	geo->VertexBufferGPU = nullptr;

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["grid"] = submesh;

	mGeometries["waterGeo"] = std::move(geo);
}

void TreeBillboardsApp::BuildBoxGeometry()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(8.0f, 8.0f, 8.0f, 3);

	std::vector<Vertex> vertices(box.Vertices.size());
	for (size_t i = 0; i < box.Vertices.size(); ++i)
	{
		auto& p = box.Vertices[i].Position;
		vertices[i].Pos = p;
		vertices[i].Normal = box.Vertices[i].Normal;
		vertices[i].TexC = box.Vertices[i].TexC;
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

	std::vector<std::uint16_t> indices = box.GetIndices16();
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "boxGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["box"] = submesh;

	mGeometries["boxGeo"] = std::move(geo);
}

void TreeBillboardsApp::BuildTreeSpritesGeometry()
{
	struct TreeSpriteVertex
	{
		XMFLOAT3 Pos;
		XMFLOAT2 Size;
	};

	//static const int treeCount = 16;
	std::array<TreeSpriteVertex, 16> vertices;
	//for(UINT i = 0; i < treeCount; ++i)
	//{
	//	float x = MathHelper::RandF(-50.0f, -25.0f);
	//	float z = MathHelper::RandF(25.f, 50.f);
	//	//float y = GetHillsHeight(x, z);
	//	float y = (8.0f);
	//	// Move tree slightly above land height.
	//	//y += 8.0f;

	//	vertices[i].Pos = XMFLOAT3(x, y, z);
	//	vertices[i].Size = XMFLOAT2(20.0f, 20.0f);
	//}

	vertices[0].Pos = XMFLOAT3(-62, 8.5, 62);
	vertices[0].Size = XMFLOAT2(20.0f, 20.0f);
	vertices[1].Pos = XMFLOAT3(-62, 8.5, -62);
	vertices[1].Size = XMFLOAT2(20.0f, 20.0f);
	vertices[2].Pos = XMFLOAT3(62, 8.5, 62);
	vertices[2].Size = XMFLOAT2(20.0f, 20.0f);
	vertices[3].Pos = XMFLOAT3(62, 8.5, -62);
	vertices[3].Size = XMFLOAT2(20.0f, 20.0f);
	vertices[4].Pos = XMFLOAT3(-60, 8.5, 0);
	vertices[4].Size = XMFLOAT2(20.0f, 20.0f);
	vertices[5].Pos = XMFLOAT3(60, 8.5, 0);
	vertices[5].Size = XMFLOAT2(20.0f, 20.0f);

	std::array<std::uint16_t, 16> indices =
	{
		0, 1, 2, 3, 4, 5, 6, 7,
		8, 9, 10, 11, 12, 13, 14, 15
	};

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(TreeSpriteVertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "treeSpritesGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(TreeSpriteVertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["points"] = submesh;

	mGeometries["treeSpritesGeo"] = std::move(geo);
}

void TreeBillboardsApp::BuildShapeGeometry()

{

	GeometryGenerator geoGen;

	GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 0);

	GeometryGenerator::MeshData grid = geoGen.CreateGrid(40.0f, 40.0f, 60, 40);

	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);

	GeometryGenerator::MeshData geoSphere = geoGen.CreateGeosphere(1.0f, 50);

	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.5f, 3.0f, 20, 20);

	GeometryGenerator::MeshData pyramid = geoGen.CreatePyrimid(0.5f);

	GeometryGenerator::MeshData diamond = geoGen.CreateDiamond(1.0f, 1.0f, 1.0f, 1.0f, 0);

	GeometryGenerator::MeshData triangularPrism = geoGen.CreateTriangularPrisim(1.0f, 1.0f, 1.0f, 0);

	GeometryGenerator::MeshData cone = geoGen.CreateCone(0.5f, 3.0f, 20, 20);

	GeometryGenerator::MeshData tetrahedron = geoGen.CreateTetrahedron(0.5f);

	GeometryGenerator::MeshData wedge = geoGen.CreateWedge(1.0f, 1.0f, 1.0f, 0);

	GeometryGenerator::MeshData quad = geoGen.CreateQuad(1.0f, 1.0f, 1.0f, 1.0f, 1.0f);




	//

	// We are concatenating all the geometry into one big vertex/index buffer.  So

	// define the regions in the buffer each submesh covers.

	//



	// Cache the vertex offsets to each object in the concatenated vertex buffer.

	UINT boxVertexOffset = 0;

	UINT gridVertexOffset = (UINT)box.Vertices.size();

	UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();

	UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();

	UINT pyramidVertexOffset = cylinderVertexOffset + (UINT)cylinder.Vertices.size(); //1

	UINT diamondVertexOffset = pyramidVertexOffset + (UINT)pyramid.Vertices.size();

	UINT triangularPrismVertexOffset = diamondVertexOffset + (UINT)diamond.Vertices.size();

	UINT coneVertexOffset = triangularPrismVertexOffset + (UINT)triangularPrism.Vertices.size();

	UINT tetrahedronVertexOffset = coneVertexOffset + (UINT)cone.Vertices.size();

	UINT wedgeVertexOffset = tetrahedronVertexOffset + (UINT)tetrahedron.Vertices.size();

	UINT geoSphereVertexOffset = wedgeVertexOffset + (UINT)wedge.Vertices.size();

	UINT quadVertexOffset = geoSphereVertexOffset + (UINT)geoSphere.Vertices.size();



	// Cache the starting index for each object in the concatenated index buffer.

	UINT boxIndexOffset = 0;

	UINT gridIndexOffset = (UINT)box.Indices32.size();

	UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();

	UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();

	UINT pyramidIndexOffset = cylinderIndexOffset + (UINT)cylinder.Indices32.size(); //2

	UINT diamondIndexOffset = pyramidIndexOffset + (UINT)pyramid.Indices32.size();

	UINT triangularPrismIndexOffset = diamondIndexOffset + (UINT)diamond.Indices32.size();

	UINT coneIndexOffset = triangularPrismIndexOffset + (UINT)triangularPrism.Indices32.size();

	UINT tetrahedronIndexOffset = coneIndexOffset + (UINT)cone.Indices32.size();

	UINT wedgeIndexOffset = tetrahedronIndexOffset + (UINT)tetrahedron.Indices32.size();

	UINT geoSphereIndexOffset = wedgeIndexOffset + (UINT)wedge.Indices32.size();

	UINT quadIndexOffset = geoSphereIndexOffset + (UINT)geoSphere.Indices32.size();

	// Define the SubmeshGeometry that cover different

	// regions of the vertex/index buffers.



	SubmeshGeometry boxSubmesh;

	boxSubmesh.IndexCount = (UINT)box.Indices32.size();

	boxSubmesh.StartIndexLocation = boxIndexOffset;

	boxSubmesh.BaseVertexLocation = boxVertexOffset;



	SubmeshGeometry gridSubmesh;

	gridSubmesh.IndexCount = (UINT)grid.Indices32.size();

	gridSubmesh.StartIndexLocation = gridIndexOffset;

	gridSubmesh.BaseVertexLocation = gridVertexOffset;



	SubmeshGeometry sphereSubmesh;

	sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();

	sphereSubmesh.StartIndexLocation = sphereIndexOffset;

	sphereSubmesh.BaseVertexLocation = sphereVertexOffset;



	SubmeshGeometry cylinderSubmesh;

	cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();

	cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;

	cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;


	SubmeshGeometry pyramidSubmesh;

	pyramidSubmesh.IndexCount = (UINT)pyramid.Indices32.size();

	pyramidSubmesh.StartIndexLocation = pyramidIndexOffset;

	pyramidSubmesh.BaseVertexLocation = pyramidVertexOffset;


	SubmeshGeometry diamondSubmesh;

	diamondSubmesh.IndexCount = (UINT)diamond.Indices32.size();

	diamondSubmesh.StartIndexLocation = diamondIndexOffset;

	diamondSubmesh.BaseVertexLocation = diamondVertexOffset;


	SubmeshGeometry triangularPrismSubmesh;

	triangularPrismSubmesh.IndexCount = (UINT)triangularPrism.Indices32.size();

	triangularPrismSubmesh.StartIndexLocation = triangularPrismIndexOffset;

	triangularPrismSubmesh.BaseVertexLocation = triangularPrismVertexOffset;


	SubmeshGeometry coneSubmesh;

	coneSubmesh.IndexCount = (UINT)cone.Indices32.size();

	coneSubmesh.StartIndexLocation = coneIndexOffset;

	coneSubmesh.BaseVertexLocation = coneVertexOffset;


	SubmeshGeometry tetrahedronSubmesh;

	tetrahedronSubmesh.IndexCount = (UINT)tetrahedron.Indices32.size();

	tetrahedronSubmesh.StartIndexLocation = tetrahedronIndexOffset;

	tetrahedronSubmesh.BaseVertexLocation = tetrahedronVertexOffset;


	SubmeshGeometry wedgeSubmesh;

	wedgeSubmesh.IndexCount = (UINT)wedge.Indices32.size();

	wedgeSubmesh.StartIndexLocation = wedgeIndexOffset;

	wedgeSubmesh.BaseVertexLocation = wedgeVertexOffset;


	SubmeshGeometry geoSphereSubmesh;

	geoSphereSubmesh.IndexCount = (UINT)geoSphere.Indices32.size();

	geoSphereSubmesh.StartIndexLocation = geoSphereIndexOffset;

	geoSphereSubmesh.BaseVertexLocation = geoSphereVertexOffset;


	SubmeshGeometry quadSubmesh;

	quadSubmesh.IndexCount = (UINT)quad.Indices32.size();

	quadSubmesh.StartIndexLocation = quadIndexOffset;

	quadSubmesh.BaseVertexLocation = quadVertexOffset;


	//

	// Extract the vertex elements we are interested in and pack the

	// vertices of all the meshes into one vertex buffer.

	//



	auto totalVertexCount =

		box.Vertices.size() +

		grid.Vertices.size() +

		sphere.Vertices.size() +

		cylinder.Vertices.size() +

		pyramid.Vertices.size() +

		diamond.Vertices.size() +

		triangularPrism.Vertices.size() +

		cone.Vertices.size() +

		tetrahedron.Vertices.size() +

		wedge.Vertices.size() +

		geoSphere.Vertices.size() +

		quad.Vertices.size();



	std::vector<Vertex> vertices(totalVertexCount);



	UINT k = 0;

	for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)

	{

		vertices[k].Pos = box.Vertices[i].Position;

		vertices[k].Normal = box.Vertices[i].Normal;
		vertices[k].TexC = box.Vertices[i].TexC;

	}



	for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)

	{

		vertices[k].Pos = grid.Vertices[i].Position;

		vertices[k].Normal = grid.Vertices[i].Normal;
		vertices[k].TexC = grid.Vertices[i].TexC;
	}



	for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)

	{

		vertices[k].Pos = sphere.Vertices[i].Position;

		vertices[k].Normal = sphere.Vertices[i].Normal;
		vertices[k].TexC = sphere.Vertices[i].TexC;

	}



	for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)

	{

		vertices[k].Pos = cylinder.Vertices[i].Position;

		vertices[k].Normal = cylinder.Vertices[i].Normal;
		vertices[k].TexC = cylinder.Vertices[i].TexC;

	}

	for (size_t i = 0; i < pyramid.Vertices.size(); ++i, ++k)

	{

		vertices[k].Pos = pyramid.Vertices[i].Position;

		vertices[k].Normal = pyramid.Vertices[i].Normal;
		vertices[k].TexC = pyramid.Vertices[i].TexC;
	}

	for (size_t i = 0; i < diamond.Vertices.size(); ++i, ++k)

	{

		vertices[k].Pos = diamond.Vertices[i].Position;

		vertices[k].Normal = diamond.Vertices[i].Normal;
		vertices[k].TexC = diamond.Vertices[i].TexC;
	}

	for (size_t i = 0; i < triangularPrism.Vertices.size(); ++i, ++k)

	{

		vertices[k].Pos = triangularPrism.Vertices[i].Position;

		vertices[k].Normal = triangularPrism.Vertices[i].Normal;
		vertices[k].TexC = triangularPrism.Vertices[i].TexC;
	}

	for (size_t i = 0; i < cone.Vertices.size(); ++i, ++k)

	{

		vertices[k].Pos = cone.Vertices[i].Position;

		vertices[k].Normal = cone.Vertices[i].Normal;
		vertices[k].TexC = cone.Vertices[i].TexC;
	}

	for (size_t i = 0; i < tetrahedron.Vertices.size(); ++i, ++k)

	{

		vertices[k].Pos = tetrahedron.Vertices[i].Position;

		vertices[k].Normal = tetrahedron.Vertices[i].Normal;
		vertices[k].TexC = tetrahedron.Vertices[i].TexC;
	}

	for (size_t i = 0; i < wedge.Vertices.size(); ++i, ++k)

	{

		vertices[k].Pos = wedge.Vertices[i].Position;

		vertices[k].Normal = wedge.Vertices[i].Normal;
		vertices[k].TexC = wedge.Vertices[i].TexC;
	}

	for (size_t i = 0; i < geoSphere.Vertices.size(); ++i, ++k)

	{

		vertices[k].Pos = geoSphere.Vertices[i].Position;

		vertices[k].Normal = geoSphere.Vertices[i].Normal;
		vertices[k].TexC = geoSphere.Vertices[i].TexC;
	}

	for (size_t i = 0; i < quad.Vertices.size(); ++i, ++k)

	{

		vertices[k].Pos = quad.Vertices[i].Position;

		vertices[k].Normal = quad.Vertices[i].Normal;
		vertices[k].TexC = quad.Vertices[i].TexC;
	}



	std::vector<std::uint16_t> indices;

	indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));

	indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));

	indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));

	indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));

	indices.insert(indices.end(), std::begin(pyramid.GetIndices16()), std::end(pyramid.GetIndices16()));

	indices.insert(indices.end(), std::begin(diamond.GetIndices16()), std::end(diamond.GetIndices16()));

	indices.insert(indices.end(), std::begin(triangularPrism.GetIndices16()), std::end(triangularPrism.GetIndices16()));

	indices.insert(indices.end(), std::begin(cone.GetIndices16()), std::end(cone.GetIndices16()));

	indices.insert(indices.end(), std::begin(tetrahedron.GetIndices16()), std::end(tetrahedron.GetIndices16()));

	indices.insert(indices.end(), std::begin(wedge.GetIndices16()), std::end(wedge.GetIndices16()));

	indices.insert(indices.end(), std::begin(geoSphere.GetIndices16()), std::end(geoSphere.GetIndices16()));

	indices.insert(indices.end(), std::begin(quad.GetIndices16()), std::end(quad.GetIndices16()));



	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);



	auto geo = std::make_unique<MeshGeometry>();

	geo->Name = "shapeGeo";



	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));

	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);



	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));

	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);



	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),

		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);



	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),

		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);



	geo->VertexByteStride = sizeof(Vertex);

	geo->VertexBufferByteSize = vbByteSize;

	geo->IndexFormat = DXGI_FORMAT_R16_UINT;

	geo->IndexBufferByteSize = ibByteSize;



	geo->DrawArgs["box"] = boxSubmesh;

	geo->DrawArgs["grid"] = gridSubmesh;

	geo->DrawArgs["sphere"] = sphereSubmesh;

	geo->DrawArgs["cylinder"] = cylinderSubmesh;

	geo->DrawArgs["pyramid"] = pyramidSubmesh;

	geo->DrawArgs["diamond"] = diamondSubmesh;

	geo->DrawArgs["triangularPrism"] = triangularPrismSubmesh;

	geo->DrawArgs["cone"] = coneSubmesh;

	geo->DrawArgs["tetrahedron"] = tetrahedronSubmesh;

	geo->DrawArgs["wedge"] = wedgeSubmesh;

	geo->DrawArgs["geoSphere"] = geoSphereSubmesh;

	geo->DrawArgs["quad"] = quadSubmesh;



	mGeometries[geo->Name] = std::move(geo);

}


void TreeBillboardsApp::BuildPSOs()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

	//
	// PSO for opaque objects.
	//
    ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { mStdInputLayout.data(), (UINT)mStdInputLayout.size() };
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS = 
	{ 
		reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()), 
		mShaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS = 
	{ 
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));

	//
	// PSO for transparent objects
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentPsoDesc = opaquePsoDesc;

	D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc;
	transparencyBlendDesc.BlendEnable = true;
	transparencyBlendDesc.LogicOpEnable = false;
	transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	transparencyBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	transparentPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&transparentPsoDesc, IID_PPV_ARGS(&mPSOs["transparent"])));

	//
	// PSO for alpha tested objects
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC alphaTestedPsoDesc = opaquePsoDesc;
	alphaTestedPsoDesc.PS = 
	{ 
		reinterpret_cast<BYTE*>(mShaders["alphaTestedPS"]->GetBufferPointer()),
		mShaders["alphaTestedPS"]->GetBufferSize()
	};
	alphaTestedPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&alphaTestedPsoDesc, IID_PPV_ARGS(&mPSOs["alphaTested"])));

	//
	// PSO for tree sprites
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC treeSpritePsoDesc = opaquePsoDesc;
	treeSpritePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["treeSpriteVS"]->GetBufferPointer()),
		mShaders["treeSpriteVS"]->GetBufferSize()
	};
	treeSpritePsoDesc.GS =
	{
		reinterpret_cast<BYTE*>(mShaders["treeSpriteGS"]->GetBufferPointer()),
		mShaders["treeSpriteGS"]->GetBufferSize()
	};
	treeSpritePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["treeSpritePS"]->GetBufferPointer()),
		mShaders["treeSpritePS"]->GetBufferSize()
	};
	treeSpritePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	treeSpritePsoDesc.InputLayout = { mTreeSpriteInputLayout.data(), (UINT)mTreeSpriteInputLayout.size() };
	treeSpritePsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&treeSpritePsoDesc, IID_PPV_ARGS(&mPSOs["treeSprites"])));
}

void TreeBillboardsApp::BuildFrameResources()
{
    for(int i = 0; i < gNumFrameResources; ++i)
    {
        mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
            1, (UINT)mAllRitems.size(), (UINT)mMaterials.size(), mWaves->VertexCount()));
    }
}

void TreeBillboardsApp::BuildMaterials()
{
	auto grass = std::make_unique<Material>();
	grass->Name = "grass";
	grass->MatCBIndex = 0;
	grass->DiffuseSrvHeapIndex = 0;
	grass->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	grass->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	grass->Roughness = 0.125f;

	// This is not a good water material definition, but we do not have all the rendering
	// tools we need (transparency, environment reflection), so we fake it for now.
	auto water = std::make_unique<Material>();
	water->Name = "water";
	water->MatCBIndex = 1;
	water->DiffuseSrvHeapIndex = 1;
	water->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.5f);
	water->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	water->Roughness = 0.0f;

	auto wirefence = std::make_unique<Material>();
	wirefence->Name = "wirefence";
	wirefence->MatCBIndex = 2;
	wirefence->DiffuseSrvHeapIndex = 2;
	wirefence->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	wirefence->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	wirefence->Roughness = 0.25f;

	auto treeSprites = std::make_unique<Material>();
	treeSprites->Name = "treeSprites";
	treeSprites->MatCBIndex = 11;
	treeSprites->DiffuseSrvHeapIndex = 11;
	treeSprites->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	treeSprites->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	treeSprites->Roughness = 0.125f;

	auto brick = std::make_unique<Material>();
	brick->Name = "brick";
	brick->MatCBIndex = 3;
	brick->DiffuseSrvHeapIndex = 3;
	brick->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	brick->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	brick->Roughness = 0.125f;

	auto ball = std::make_unique<Material>();
	ball->Name = "ball";
	ball->MatCBIndex = 4;
	ball->DiffuseSrvHeapIndex = 4;
	ball->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	ball->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	ball->Roughness = 0.125f;

	auto darkBrick = std::make_unique<Material>();
	darkBrick->Name = "darkBrick";
	darkBrick->MatCBIndex = 5;
	darkBrick->DiffuseSrvHeapIndex = 5;
	darkBrick->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	darkBrick->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	darkBrick->Roughness = 0.125f;

	auto darkLightBrick = std::make_unique<Material>();
	darkLightBrick->Name = "darkLightBrick";
	darkLightBrick->MatCBIndex = 6;
	darkLightBrick->DiffuseSrvHeapIndex = 6;
	darkLightBrick->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	darkLightBrick->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	darkLightBrick->Roughness = 0.125f;

	auto lightBrick = std::make_unique<Material>();
	lightBrick->Name = "lightBrick";
	lightBrick->MatCBIndex = 7;
	lightBrick->DiffuseSrvHeapIndex = 7;
	lightBrick->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	lightBrick->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	lightBrick->Roughness = 0.125f;

	auto redTile = std::make_unique<Material>();
	redTile->Name = "redTile";
	redTile->MatCBIndex = 8;
	redTile->DiffuseSrvHeapIndex = 8;
	redTile->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	redTile->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	redTile->Roughness = 0.125f;

	auto glass = std::make_unique<Material>();
	glass->Name = "glass";
	glass->MatCBIndex = 9;
	glass->DiffuseSrvHeapIndex = 9;
	glass->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	glass->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	glass->Roughness = 0.125f;

	auto sand = std::make_unique<Material>();
	sand->Name = "sand";
	sand->MatCBIndex = 10;
	sand->DiffuseSrvHeapIndex = 10;
	sand->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	sand->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	sand->Roughness = 0.125f;

	mMaterials["grass"] = std::move(grass);
	mMaterials["water"] = std::move(water);
	mMaterials["wirefence"] = std::move(wirefence);
	mMaterials["treeSprites"] = std::move(treeSprites);
	mMaterials["brick"] = std::move(brick);
	mMaterials["ball"] = std::move(ball);
	mMaterials["darkBrick"] = std::move(darkBrick);
	mMaterials["darkLightBrick"] = std::move(darkLightBrick);
	mMaterials["lightBrick"] = std::move(lightBrick);
	mMaterials["redTile"] = std::move(redTile);
	mMaterials["glass"] = std::move(glass);
	mMaterials["sand"] = std::move(sand);
}

void TreeBillboardsApp::BuildRenderItems()
{
    auto wavesRitem = std::make_unique<RenderItem>();
    wavesRitem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&wavesRitem->TexTransform, XMMatrixScaling(50.0f, 50.0f, 1.0f));
	XMStoreFloat4x4(&wavesRitem->World, XMMatrixTranslation(1.0f, 0.0f, 1.0f) * XMMatrixScaling(5.0f, 1.0f, 5.0f));
	wavesRitem->ObjCBIndex = 0;
	wavesRitem->Mat = mMaterials["water"].get();
	wavesRitem->Geo = mGeometries["waterGeo"].get();
	wavesRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	wavesRitem->IndexCount = wavesRitem->Geo->DrawArgs["grid"].IndexCount;
	wavesRitem->StartIndexLocation = wavesRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	wavesRitem->BaseVertexLocation = wavesRitem->Geo->DrawArgs["grid"].BaseVertexLocation;

    mWavesRitem = wavesRitem.get();

	mRitemLayer[(int)RenderLayer::Transparent].push_back(wavesRitem.get());

	mAllRitems.push_back(std::move(wavesRitem));

    /*auto gridRitem = std::make_unique<RenderItem>();
    gridRitem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&gridRitem->TexTransform, XMMatrixScaling(5.0f, 5.0f, 1.0f));
	gridRitem->ObjCBIndex = 1;
	gridRitem->Mat = mMaterials["grass"].get();
	gridRitem->Geo = mGeometries["landGeo"].get();
	gridRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
    gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
    gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::Opaque].push_back(gridRitem.get());

	mAllRitems.push_back(std::move(gridRitem));*/

	auto boxRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem->World, XMMatrixTranslation(0.0f, 10.0f, 0.0f) * XMMatrixScaling(102, 0.15f, 102));
	boxRitem->ObjCBIndex = 1;
	boxRitem->Mat = mMaterials["grass"].get();
	boxRitem->Geo = mGeometries["shapeGeo"].get();
	boxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::Opaque].push_back(boxRitem.get());

	mAllRitems.push_back(std::move(boxRitem));

	auto treeSpritesRitem = std::make_unique<RenderItem>();
	treeSpritesRitem->World = MathHelper::Identity4x4();
	treeSpritesRitem->ObjCBIndex = 2;
	treeSpritesRitem->Mat = mMaterials["treeSprites"].get();
	treeSpritesRitem->Geo = mGeometries["treeSpritesGeo"].get();
	treeSpritesRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
	treeSpritesRitem->IndexCount = treeSpritesRitem->Geo->DrawArgs["points"].IndexCount;
	treeSpritesRitem->StartIndexLocation = treeSpritesRitem->Geo->DrawArgs["points"].StartIndexLocation;
	treeSpritesRitem->BaseVertexLocation = treeSpritesRitem->Geo->DrawArgs["points"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTestedTreeSprites].push_back(treeSpritesRitem.get());

	mAllRitems.push_back(std::move(treeSpritesRitem));

	//wall 1

	auto boxRitem2 = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&boxRitem2->World, XMMatrixScaling(34.0f, 6.0f, 0.5f) * XMMatrixTranslation(0.0f, 5.0f, 18.0f));

	boxRitem2->ObjCBIndex = 3;

	boxRitem2->Mat = mMaterials["darkBrick"].get();

	boxRitem2->Geo = mGeometries["shapeGeo"].get();

	boxRitem2->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	boxRitem2->IndexCount = boxRitem2->Geo->DrawArgs["box"].IndexCount;

	boxRitem2->StartIndexLocation = boxRitem2->Geo->DrawArgs["box"].StartIndexLocation;

	boxRitem2->BaseVertexLocation = boxRitem2->Geo->DrawArgs["box"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(boxRitem2.get());

	mAllRitems.push_back(std::move(boxRitem2));

   
	// wall_2

	auto box2Ritem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&box2Ritem->World, XMMatrixScaling(12.0f, 6.0f, 0.5f) * XMMatrixTranslation(10.0f, 5.0f, -18.0f));

	box2Ritem->ObjCBIndex = 4;

	box2Ritem->Mat = mMaterials["darkBrick"].get();

	box2Ritem->Geo = mGeometries["shapeGeo"].get();

	box2Ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	box2Ritem->IndexCount = box2Ritem->Geo->DrawArgs["box"].IndexCount;

	box2Ritem->StartIndexLocation = box2Ritem->Geo->DrawArgs["box"].StartIndexLocation;

	box2Ritem->BaseVertexLocation = box2Ritem->Geo->DrawArgs["box"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(box2Ritem.get());

	mAllRitems.push_back(std::move(box2Ritem));


	// wall_3

	auto box3Ritem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&box3Ritem->World, XMMatrixScaling(12.0f, 6.0f, 0.5f) * XMMatrixTranslation(-10.0f, 5.0f, -18.0f));

	box3Ritem->ObjCBIndex = 5;

	box3Ritem->Mat = mMaterials["darkBrick"].get();

	box3Ritem->Geo = mGeometries["shapeGeo"].get();

	box3Ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	box3Ritem->IndexCount = box3Ritem->Geo->DrawArgs["box"].IndexCount;

	box3Ritem->StartIndexLocation = box3Ritem->Geo->DrawArgs["box"].StartIndexLocation;

	box3Ritem->BaseVertexLocation = box3Ritem->Geo->DrawArgs["box"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(box3Ritem.get());

	mAllRitems.push_back(std::move(box3Ritem));


	// wall_4

	auto box4Ritem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&box4Ritem->World, XMMatrixScaling(0.5f, 6.0f, 32.0f) * XMMatrixTranslation(18.0f, 5.0f, 0.0f));

	box4Ritem->ObjCBIndex = 6;

	box4Ritem->Mat = mMaterials["darkBrick"].get();

	box4Ritem->Geo = mGeometries["shapeGeo"].get();

	box4Ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	box4Ritem->IndexCount = box4Ritem->Geo->DrawArgs["box"].IndexCount;

	box4Ritem->StartIndexLocation = box4Ritem->Geo->DrawArgs["box"].StartIndexLocation;

	box4Ritem->BaseVertexLocation = box4Ritem->Geo->DrawArgs["box"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(box4Ritem.get());

	mAllRitems.push_back(std::move(box4Ritem));


	// wall_5

	auto box5Ritem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&box5Ritem->World, XMMatrixScaling(0.5f, 6.0f, 32.0f) * XMMatrixTranslation(-18.0f, 5.0f, 0.0f));

	box5Ritem->ObjCBIndex = 7;

	box5Ritem->Mat = mMaterials["darkBrick"].get();

	box5Ritem->Geo = mGeometries["shapeGeo"].get();

	box5Ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	box5Ritem->IndexCount = box5Ritem->Geo->DrawArgs["box"].IndexCount;

	box5Ritem->StartIndexLocation = box5Ritem->Geo->DrawArgs["box"].StartIndexLocation;

	box5Ritem->BaseVertexLocation = box5Ritem->Geo->DrawArgs["box"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(box5Ritem.get());

	mAllRitems.push_back(std::move(box5Ritem));



	// column

	auto cylinderRitem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&cylinderRitem->World, XMMatrixScaling(4.5f, 3.0f, 4.5f) * XMMatrixTranslation(-18.0f, 6.0f, 18.0f));

	cylinderRitem->ObjCBIndex = 8;

	cylinderRitem->Geo = mGeometries["shapeGeo"].get();

	cylinderRitem->Mat = mMaterials["darkBrick"].get();


	cylinderRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	cylinderRitem->IndexCount = cylinderRitem->Geo->DrawArgs["cylinder"].IndexCount;

	cylinderRitem->StartIndexLocation = cylinderRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;

	cylinderRitem->BaseVertexLocation = cylinderRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(cylinderRitem.get());


	mAllRitems.push_back(std::move(cylinderRitem));



	// column_2

	auto cylinder2Ritem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&cylinder2Ritem->World, XMMatrixScaling(4.5f, 3.0f, 4.5f) * XMMatrixTranslation(18.0f, 6.0f, -18.0f));

	cylinder2Ritem->ObjCBIndex = 9;

	cylinder2Ritem->Geo = mGeometries["shapeGeo"].get();

	cylinder2Ritem->Mat = mMaterials["darkBrick"].get();


	cylinder2Ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	cylinder2Ritem->IndexCount = cylinder2Ritem->Geo->DrawArgs["cylinder"].IndexCount;

	cylinder2Ritem->StartIndexLocation = cylinder2Ritem->Geo->DrawArgs["cylinder"].StartIndexLocation;

	cylinder2Ritem->BaseVertexLocation = cylinder2Ritem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(cylinder2Ritem.get());


	mAllRitems.push_back(std::move(cylinder2Ritem));



	// column_3

	auto cylinder3Ritem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&cylinder3Ritem->World, XMMatrixScaling(4.5f, 3.0f, 4.5f) * XMMatrixTranslation(-18.0f, 6.0f, -18.0f));

	cylinder3Ritem->ObjCBIndex = 10;

	cylinder3Ritem->Mat = mMaterials["darkBrick"].get();


	cylinder3Ritem->Geo = mGeometries["shapeGeo"].get();

	cylinder3Ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	cylinder3Ritem->IndexCount = cylinder3Ritem->Geo->DrawArgs["cylinder"].IndexCount;

	cylinder3Ritem->StartIndexLocation = cylinder3Ritem->Geo->DrawArgs["cylinder"].StartIndexLocation;

	cylinder3Ritem->BaseVertexLocation = cylinder3Ritem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(cylinder3Ritem.get());


	mAllRitems.push_back(std::move(cylinder3Ritem));



	// column_4

	auto cylinder4Ritem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&cylinder4Ritem->World, XMMatrixScaling(4.5f, 3.0f, 4.5f) * XMMatrixTranslation(18.0f, 6.0f, 18.0f));

	cylinder4Ritem->ObjCBIndex = 11;

	cylinder4Ritem->Mat = mMaterials["darkBrick"].get();


	cylinder4Ritem->Geo = mGeometries["shapeGeo"].get();

	cylinder4Ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	cylinder4Ritem->IndexCount = cylinder4Ritem->Geo->DrawArgs["cylinder"].IndexCount;

	cylinder4Ritem->StartIndexLocation = cylinder4Ritem->Geo->DrawArgs["cylinder"].StartIndexLocation;

	cylinder4Ritem->BaseVertexLocation = cylinder4Ritem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(cylinder4Ritem.get());


	mAllRitems.push_back(std::move(cylinder4Ritem));



	// cone

	auto coneRitem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&coneRitem->World, XMMatrixScaling(4.5f, 3.0f, 4.5f) * XMMatrixTranslation(18.0f, 14.0f, 18.0f));

	coneRitem->ObjCBIndex = 12;

	coneRitem->Mat = mMaterials["redTile"].get();


	coneRitem->Geo = mGeometries["shapeGeo"].get();

	coneRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	coneRitem->IndexCount = coneRitem->Geo->DrawArgs["cone"].IndexCount;

	coneRitem->StartIndexLocation = coneRitem->Geo->DrawArgs["cone"].StartIndexLocation;

	coneRitem->BaseVertexLocation = coneRitem->Geo->DrawArgs["cone"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(coneRitem.get());


	mAllRitems.push_back(std::move(coneRitem));



	// cone_2

	auto cone2Ritem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&cone2Ritem->World, XMMatrixScaling(4.5f, 3.0f, 4.5f) * XMMatrixTranslation(-18.0f, 14.0f, -18.0f));

	cone2Ritem->ObjCBIndex = 13;

	cone2Ritem->Mat = mMaterials["redTile"].get();


	cone2Ritem->Geo = mGeometries["shapeGeo"].get();

	cone2Ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	cone2Ritem->IndexCount = cone2Ritem->Geo->DrawArgs["cone"].IndexCount;

	cone2Ritem->StartIndexLocation = cone2Ritem->Geo->DrawArgs["cone"].StartIndexLocation;

	cone2Ritem->BaseVertexLocation = cone2Ritem->Geo->DrawArgs["cone"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(cone2Ritem.get());


	mAllRitems.push_back(std::move(cone2Ritem));



	// cone_3

	auto cone3Ritem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&cone3Ritem->World, XMMatrixScaling(4.5f, 3.0f, 4.5f) * XMMatrixTranslation(-18.0f, 14.0f, 18.0f));

	cone3Ritem->ObjCBIndex = 14;

	cone3Ritem->Mat = mMaterials["redTile"].get();


	cone3Ritem->Geo = mGeometries["shapeGeo"].get();

	cone3Ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	cone3Ritem->IndexCount = cone3Ritem->Geo->DrawArgs["cone"].IndexCount;

	cone3Ritem->StartIndexLocation = cone3Ritem->Geo->DrawArgs["cone"].StartIndexLocation;

	cone3Ritem->BaseVertexLocation = cone3Ritem->Geo->DrawArgs["cone"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(cone3Ritem.get());


	mAllRitems.push_back(std::move(cone3Ritem));



	// cone_4

	auto cone4Ritem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&cone4Ritem->World, XMMatrixScaling(4.5f, 3.0f, 4.5f) * XMMatrixTranslation(18.0f, 14.0f, -18.0f));

	cone4Ritem->ObjCBIndex = 15;

	cone4Ritem->Mat = mMaterials["redTile"].get();


	cone4Ritem->Geo = mGeometries["shapeGeo"].get();

	cone4Ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	cone4Ritem->IndexCount = cone4Ritem->Geo->DrawArgs["cone"].IndexCount;

	cone4Ritem->StartIndexLocation = cone4Ritem->Geo->DrawArgs["cone"].StartIndexLocation;

	cone4Ritem->BaseVertexLocation = cone4Ritem->Geo->DrawArgs["cone"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(cone4Ritem.get());


	mAllRitems.push_back(std::move(cone4Ritem));


	// inner wall - box

	auto box6Ritem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&box6Ritem->World, XMMatrixScaling(24.0f, 10.0f, 1.0f) * XMMatrixTranslation(0.0f, 7.0f, 14.0f));

	box6Ritem->ObjCBIndex = 16;

	box6Ritem->Mat = mMaterials["darkLightBrick"].get();


	box6Ritem->Geo = mGeometries["shapeGeo"].get();

	box6Ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	box6Ritem->IndexCount = box6Ritem->Geo->DrawArgs["box"].IndexCount;

	box6Ritem->StartIndexLocation = box6Ritem->Geo->DrawArgs["box"].StartIndexLocation;

	box6Ritem->BaseVertexLocation = box6Ritem->Geo->DrawArgs["box"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(box6Ritem.get());


	mAllRitems.push_back(std::move(box6Ritem));



	// inner wall 2 - box

	auto box7Ritem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&box7Ritem->World, XMMatrixScaling(1.0f, 10.0f, 24.0f) * XMMatrixTranslation(14.0f, 7.0f, 0.0f));

	box7Ritem->ObjCBIndex = 17;

	box7Ritem->Mat = mMaterials["darkLightBrick"].get();


	box7Ritem->Geo = mGeometries["shapeGeo"].get();

	box7Ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	box7Ritem->IndexCount = box7Ritem->Geo->DrawArgs["box"].IndexCount;

	box7Ritem->StartIndexLocation = box7Ritem->Geo->DrawArgs["box"].StartIndexLocation;

	box7Ritem->BaseVertexLocation = box7Ritem->Geo->DrawArgs["box"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(box7Ritem.get());


	mAllRitems.push_back(std::move(box7Ritem));



	// inner wall 3 - box

	auto box8Ritem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&box8Ritem->World, XMMatrixScaling(1.0f, 10.0f, 24.0f) * XMMatrixTranslation(-14.0f, 7.0f, 0.0f));

	box8Ritem->ObjCBIndex = 18;

	box8Ritem->Mat = mMaterials["darkLightBrick"].get();


	box8Ritem->Geo = mGeometries["shapeGeo"].get();

	box8Ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	box8Ritem->IndexCount = box8Ritem->Geo->DrawArgs["box"].IndexCount;

	box8Ritem->StartIndexLocation = box8Ritem->Geo->DrawArgs["box"].StartIndexLocation;

	box8Ritem->BaseVertexLocation = box8Ritem->Geo->DrawArgs["box"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(box8Ritem.get());


	mAllRitems.push_back(std::move(box8Ritem));


	// inner cylinder 

	auto cylinder5Ritem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&cylinder5Ritem->World, XMMatrixScaling(3.5f, 4.0f, 3.5f) * XMMatrixTranslation(-13.0f, 7.0f, 13.0f));

	cylinder5Ritem->ObjCBIndex = 19;

	cylinder5Ritem->Mat = mMaterials["darkLightBrick"].get();


	cylinder5Ritem->Geo = mGeometries["shapeGeo"].get();

	cylinder5Ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	cylinder5Ritem->IndexCount = cylinder5Ritem->Geo->DrawArgs["cylinder"].IndexCount;

	cylinder5Ritem->StartIndexLocation = cylinder5Ritem->Geo->DrawArgs["cylinder"].StartIndexLocation;

	cylinder5Ritem->BaseVertexLocation = cylinder5Ritem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(cylinder5Ritem.get());


	mAllRitems.push_back(std::move(cylinder5Ritem));



	// inner cylinder_2

	auto cylinder6Ritem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&cylinder6Ritem->World, XMMatrixScaling(3.5f, 4.0f, 3.5f) * XMMatrixTranslation(13.0f, 7.0f, 13.0f));

	cylinder6Ritem->ObjCBIndex = 20;

	cylinder6Ritem->Mat = mMaterials["darkLightBrick"].get();


	cylinder6Ritem->Geo = mGeometries["shapeGeo"].get();

	cylinder6Ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	cylinder6Ritem->IndexCount = cylinder6Ritem->Geo->DrawArgs["cylinder"].IndexCount;

	cylinder6Ritem->StartIndexLocation = cylinder6Ritem->Geo->DrawArgs["cylinder"].StartIndexLocation;

	cylinder6Ritem->BaseVertexLocation = cylinder6Ritem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(cylinder6Ritem.get());


	mAllRitems.push_back(std::move(cylinder6Ritem));



	// diamond

	auto diamondRitem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&diamondRitem->World, XMMatrixScaling(3.0f, 4.0f, 3.0f) * XMMatrixTranslation(13.0f, 16.0f, 13.0f));

	diamondRitem->ObjCBIndex = 21;

	diamondRitem->Mat = mMaterials["glass"].get();


	diamondRitem->Geo = mGeometries["shapeGeo"].get();

	diamondRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	diamondRitem->IndexCount = diamondRitem->Geo->DrawArgs["diamond"].IndexCount;

	diamondRitem->StartIndexLocation = diamondRitem->Geo->DrawArgs["diamond"].StartIndexLocation;

	diamondRitem->BaseVertexLocation = diamondRitem->Geo->DrawArgs["diamond"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(diamondRitem.get());


	mAllRitems.push_back(std::move(diamondRitem));



	// diamond_2

	auto diamond2Ritem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&diamond2Ritem->World, XMMatrixScaling(3.0f, 4.0f, 3.0f) * XMMatrixTranslation(-13.0f, 16.0f, 13.0f));

	diamond2Ritem->ObjCBIndex = 22;

	diamond2Ritem->Mat = mMaterials["glass"].get();


	diamond2Ritem->Geo = mGeometries["shapeGeo"].get();

	diamond2Ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	diamond2Ritem->IndexCount = diamond2Ritem->Geo->DrawArgs["diamond"].IndexCount;

	diamond2Ritem->StartIndexLocation = diamond2Ritem->Geo->DrawArgs["diamond"].StartIndexLocation;

	diamond2Ritem->BaseVertexLocation = diamond2Ritem->Geo->DrawArgs["diamond"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(diamond2Ritem.get());


	mAllRitems.push_back(std::move(diamond2Ritem));



	// sphere

	auto sphereRitem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&sphereRitem->World, XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(-13.0f, 20.5f, 13.0f));

	sphereRitem->ObjCBIndex = 23;

	sphereRitem->Mat = mMaterials["ball"].get();


	sphereRitem->Geo = mGeometries["shapeGeo"].get();

	sphereRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	sphereRitem->IndexCount = sphereRitem->Geo->DrawArgs["sphere"].IndexCount;

	sphereRitem->StartIndexLocation = sphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;

	sphereRitem->BaseVertexLocation = sphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(sphereRitem.get());


	mAllRitems.push_back(std::move(sphereRitem));



	// sphere_2

	auto sphere2Ritem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&sphere2Ritem->World, XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(13.0f, 20.5f, 13.0f));

	sphere2Ritem->ObjCBIndex = 24;

	sphere2Ritem->Mat = mMaterials["ball"].get();


	sphere2Ritem->Geo = mGeometries["shapeGeo"].get();

	sphere2Ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	sphere2Ritem->IndexCount = sphere2Ritem->Geo->DrawArgs["sphere"].IndexCount;

	sphere2Ritem->StartIndexLocation = sphere2Ritem->Geo->DrawArgs["sphere"].StartIndexLocation;

	sphere2Ritem->BaseVertexLocation = sphere2Ritem->Geo->DrawArgs["sphere"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(sphere2Ritem.get());


	mAllRitems.push_back(std::move(sphere2Ritem));



	//  triangular prism

	auto triangularPrismRitem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&triangularPrismRitem->World, XMMatrixScaling(2.0f, 4.0f, 24.0f) * XMMatrixTranslation(-14.0f, 12.0f, 0.0f));

	triangularPrismRitem->ObjCBIndex = 25;

	triangularPrismRitem->Mat = mMaterials["sand"].get();


	triangularPrismRitem->Geo = mGeometries["shapeGeo"].get();

	triangularPrismRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	triangularPrismRitem->IndexCount = triangularPrismRitem->Geo->DrawArgs["triangularPrism"].IndexCount;

	triangularPrismRitem->StartIndexLocation = triangularPrismRitem->Geo->DrawArgs["triangularPrism"].StartIndexLocation;

	triangularPrismRitem->BaseVertexLocation = triangularPrismRitem->Geo->DrawArgs["triangularPrism"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(triangularPrismRitem.get());


	mAllRitems.push_back(std::move(triangularPrismRitem));


	//  triangular prism_2

	auto triangularPrism2Ritem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&triangularPrism2Ritem->World, XMMatrixScaling(2.0f, 4.0f, 24.0f) * XMMatrixTranslation(14.0f, 12.0f, 0.0f));

	triangularPrism2Ritem->ObjCBIndex = 26;

	triangularPrism2Ritem->Mat = mMaterials["sand"].get();


	triangularPrism2Ritem->Geo = mGeometries["shapeGeo"].get();

	triangularPrism2Ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	triangularPrism2Ritem->IndexCount = triangularPrism2Ritem->Geo->DrawArgs["triangularPrism"].IndexCount;

	triangularPrism2Ritem->StartIndexLocation = triangularPrism2Ritem->Geo->DrawArgs["triangularPrism"].StartIndexLocation;

	triangularPrism2Ritem->BaseVertexLocation = triangularPrism2Ritem->Geo->DrawArgs["triangularPrism"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(triangularPrism2Ritem.get());


	mAllRitems.push_back(std::move(triangularPrism2Ritem));



	//  triangular prism_3

	auto triangularPrism3Ritem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&triangularPrism3Ritem->World, XMMatrixScaling(24.0f, 4.0f, 2.0f) * XMMatrixTranslation(0.0f, 12.0f, 14.0f));

	triangularPrism3Ritem->ObjCBIndex = 27;

	triangularPrism3Ritem->Mat = mMaterials["sand"].get();


	triangularPrism3Ritem->Geo = mGeometries["shapeGeo"].get();

	triangularPrism3Ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	triangularPrism3Ritem->IndexCount = triangularPrism3Ritem->Geo->DrawArgs["triangularPrism"].IndexCount;

	triangularPrism3Ritem->StartIndexLocation = triangularPrism3Ritem->Geo->DrawArgs["triangularPrism"].StartIndexLocation;

	triangularPrism3Ritem->BaseVertexLocation = triangularPrism3Ritem->Geo->DrawArgs["triangularPrism"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(triangularPrism3Ritem.get());


	mAllRitems.push_back(std::move(triangularPrism3Ritem));



	// wedge

	auto wedgeRitem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&wedgeRitem->World, XMMatrixScaling(2.0f, 2.0f, 34.0f) * XMMatrixTranslation(-18.0f, 8.0f, 0.0f));

	wedgeRitem->ObjCBIndex = 28;

	wedgeRitem->Mat = mMaterials["sand"].get();


	wedgeRitem->Geo = mGeometries["shapeGeo"].get();

	wedgeRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	wedgeRitem->IndexCount = wedgeRitem->Geo->DrawArgs["wedge"].IndexCount;

	wedgeRitem->StartIndexLocation = wedgeRitem->Geo->DrawArgs["wedge"].StartIndexLocation;

	wedgeRitem->BaseVertexLocation = wedgeRitem->Geo->DrawArgs["wedge"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(wedgeRitem.get());


	mAllRitems.push_back(std::move(wedgeRitem));



	// wedge_2

	auto wedge2Ritem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&wedge2Ritem->World, XMMatrixScaling(-2.0f, 2.0f, -34.0f) * XMMatrixTranslation(18.0f, 8.0f, 0.0f));

	wedge2Ritem->ObjCBIndex = 29;

	wedge2Ritem->Mat = mMaterials["sand"].get();


	wedge2Ritem->Geo = mGeometries["shapeGeo"].get();

	wedge2Ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	wedge2Ritem->IndexCount = wedge2Ritem->Geo->DrawArgs["wedge"].IndexCount;

	wedge2Ritem->StartIndexLocation = wedge2Ritem->Geo->DrawArgs["wedge"].StartIndexLocation;

	wedge2Ritem->BaseVertexLocation = wedge2Ritem->Geo->DrawArgs["wedge"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(wedge2Ritem.get());


	mAllRitems.push_back(std::move(wedge2Ritem));



	//  triangular prism_4

	auto triangularPrism4Ritem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&triangularPrism4Ritem->World, XMMatrixScaling(34.0f, 4.0f, 2.0f) * XMMatrixTranslation(0.0f, 8.0f, 18.0f));

	triangularPrism4Ritem->ObjCBIndex = 30;

	triangularPrism4Ritem->Mat = mMaterials["sand"].get();


	triangularPrism4Ritem->Geo = mGeometries["shapeGeo"].get();

	triangularPrism4Ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	triangularPrism4Ritem->IndexCount = triangularPrism4Ritem->Geo->DrawArgs["triangularPrism"].IndexCount;

	triangularPrism4Ritem->StartIndexLocation = triangularPrism4Ritem->Geo->DrawArgs["triangularPrism"].StartIndexLocation;

	triangularPrism4Ritem->BaseVertexLocation = triangularPrism4Ritem->Geo->DrawArgs["triangularPrism"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(triangularPrism4Ritem.get());


	mAllRitems.push_back(std::move(triangularPrism4Ritem));



	// center wall 1 - box

	auto box9Ritem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&box9Ritem->World, XMMatrixScaling(17.0f, 14.0f, 1.0f) * XMMatrixTranslation(0.0f, 9.0f, 8.0f));

	box9Ritem->ObjCBIndex = 31;

	box9Ritem->Mat = mMaterials["lightBrick"].get();


	box9Ritem->Geo = mGeometries["shapeGeo"].get();

	box9Ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	box9Ritem->IndexCount = box9Ritem->Geo->DrawArgs["box"].IndexCount;

	box9Ritem->StartIndexLocation = box9Ritem->Geo->DrawArgs["box"].StartIndexLocation;

	box9Ritem->BaseVertexLocation = box9Ritem->Geo->DrawArgs["box"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(box9Ritem.get());


	mAllRitems.push_back(std::move(box9Ritem));



	// center wall 2 - box

	auto box10Ritem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&box10Ritem->World, XMMatrixScaling(1.0f, 14.0f, 17.0f) * XMMatrixTranslation(8.0f, 9.0f, 0.0f));

	box10Ritem->ObjCBIndex = 32;

	box10Ritem->Mat = mMaterials["lightBrick"].get();


	box10Ritem->Geo = mGeometries["shapeGeo"].get();

	box10Ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	box10Ritem->IndexCount = box10Ritem->Geo->DrawArgs["box"].IndexCount;

	box10Ritem->StartIndexLocation = box10Ritem->Geo->DrawArgs["box"].StartIndexLocation;

	box10Ritem->BaseVertexLocation = box10Ritem->Geo->DrawArgs["box"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(box10Ritem.get());


	mAllRitems.push_back(std::move(box10Ritem));



	// center wall 3 - box

	auto box11Ritem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&box11Ritem->World, XMMatrixScaling(1.0f, 14.0f, 17.0f) * XMMatrixTranslation(-8.0f, 9.0f, 0.0f));

	box11Ritem->ObjCBIndex = 33;

	box11Ritem->Mat = mMaterials["lightBrick"].get();


	box11Ritem->Geo = mGeometries["shapeGeo"].get();

	box11Ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	box11Ritem->IndexCount = box11Ritem->Geo->DrawArgs["box"].IndexCount;

	box11Ritem->StartIndexLocation = box11Ritem->Geo->DrawArgs["box"].StartIndexLocation;

	box11Ritem->BaseVertexLocation = box11Ritem->Geo->DrawArgs["box"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(box11Ritem.get());


	mAllRitems.push_back(std::move(box11Ritem));



	// center wall 4 - box

	auto box12Ritem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&box12Ritem->World, XMMatrixScaling(17.0f, 8.0f, 1.0f) * XMMatrixTranslation(0.0f, 12.0f, -8.0f));

	box12Ritem->ObjCBIndex = 34;

	box12Ritem->Mat = mMaterials["lightBrick"].get();


	box12Ritem->Geo = mGeometries["shapeGeo"].get();

	box12Ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	box12Ritem->IndexCount = box12Ritem->Geo->DrawArgs["box"].IndexCount;

	box12Ritem->StartIndexLocation = box12Ritem->Geo->DrawArgs["box"].StartIndexLocation;

	box12Ritem->BaseVertexLocation = box12Ritem->Geo->DrawArgs["box"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(box12Ritem.get());


	mAllRitems.push_back(std::move(box12Ritem));



	// center roof  - box

	auto box13Ritem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&box13Ritem->World, XMMatrixScaling(17.0f, 1.0f, 17.0f) * XMMatrixTranslation(0.0f, 16.0f, 0.0f));

	box13Ritem->ObjCBIndex = 35;

	box13Ritem->Mat = mMaterials["lightBrick"].get();


	box13Ritem->Geo = mGeometries["shapeGeo"].get();

	box13Ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	box13Ritem->IndexCount = box13Ritem->Geo->DrawArgs["box"].IndexCount;

	box13Ritem->StartIndexLocation = box13Ritem->Geo->DrawArgs["box"].StartIndexLocation;

	box13Ritem->BaseVertexLocation = box13Ritem->Geo->DrawArgs["box"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(box13Ritem.get());


	mAllRitems.push_back(std::move(box13Ritem));



	// center wall 5 - box

	auto box14Ritem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&box14Ritem->World, XMMatrixScaling(3.0f, 6.0f, 1.0f) * XMMatrixTranslation(-6.0f, 5.0f, -8.0f));

	box14Ritem->ObjCBIndex = 36;

	box14Ritem->Mat = mMaterials["lightBrick"].get();


	box14Ritem->Geo = mGeometries["shapeGeo"].get();

	box14Ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	box14Ritem->IndexCount = box14Ritem->Geo->DrawArgs["box"].IndexCount;

	box14Ritem->StartIndexLocation = box14Ritem->Geo->DrawArgs["box"].StartIndexLocation;

	box14Ritem->BaseVertexLocation = box14Ritem->Geo->DrawArgs["box"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(box14Ritem.get());


	mAllRitems.push_back(std::move(box14Ritem));



	// center wall 5 - box

	auto box15Ritem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&box15Ritem->World, XMMatrixScaling(3.0f, 6.0f, 1.0f) * XMMatrixTranslation(6.0f, 5.0f, -8.0f));

	box15Ritem->ObjCBIndex = 37;

	box15Ritem->Mat = mMaterials["lightBrick"].get();


	box15Ritem->Geo = mGeometries["shapeGeo"].get();

	box15Ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	box15Ritem->IndexCount = box15Ritem->Geo->DrawArgs["box"].IndexCount;

	box15Ritem->StartIndexLocation = box15Ritem->Geo->DrawArgs["box"].StartIndexLocation;

	box15Ritem->BaseVertexLocation = box15Ritem->Geo->DrawArgs["box"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(box15Ritem.get());


	mAllRitems.push_back(std::move(box15Ritem));



	// bridge - box
	

	auto box16Ritem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&box16Ritem->World, XMMatrixScaling(8.0f, 0.2f, 10.0f) * XMMatrixRotationX(-38.0f) * XMMatrixTranslation(0.0f, 1.0f, -28.0f));

	box16Ritem->ObjCBIndex = 38;

	box16Ritem->Mat = mMaterials["brick"].get();


	box16Ritem->Geo = mGeometries["shapeGeo"].get();

	box16Ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	box16Ritem->IndexCount = box16Ritem->Geo->DrawArgs["box"].IndexCount;

	box16Ritem->StartIndexLocation = box16Ritem->Geo->DrawArgs["box"].StartIndexLocation;

	box16Ritem->BaseVertexLocation = box16Ritem->Geo->DrawArgs["box"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(box16Ritem.get());


	mAllRitems.push_back(std::move(box16Ritem));


	// entrance - quad

	auto quadRitem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&quadRitem->World, XMMatrixScaling(10.0f, 8.0f, 8.0f) *  XMMatrixTranslation(-15.0f, 2.0f, -16.0f));

	quadRitem->ObjCBIndex = 39;

	quadRitem->Mat = mMaterials["wirefence"].get();


	quadRitem->Geo = mGeometries["shapeGeo"].get();

	quadRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	quadRitem->IndexCount = quadRitem->Geo->DrawArgs["quad"].IndexCount;

	quadRitem->StartIndexLocation = quadRitem->Geo->DrawArgs["quad"].StartIndexLocation;

	quadRitem->BaseVertexLocation = quadRitem->Geo->DrawArgs["quad"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(quadRitem.get());


	mAllRitems.push_back(std::move(quadRitem));



	// tetrahedron

	auto tetrahedronRitem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&tetrahedronRitem->World, XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(-7.0f, 18.0f, -7.0f));

	tetrahedronRitem->ObjCBIndex = 40;

	tetrahedronRitem->Mat = mMaterials["glass"].get();


	tetrahedronRitem->Geo = mGeometries["shapeGeo"].get();

	tetrahedronRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	tetrahedronRitem->IndexCount = tetrahedronRitem->Geo->DrawArgs["tetrahedron"].IndexCount;

	tetrahedronRitem->StartIndexLocation = tetrahedronRitem->Geo->DrawArgs["tetrahedron"].StartIndexLocation;

	tetrahedronRitem->BaseVertexLocation = tetrahedronRitem->Geo->DrawArgs["tetrahedron"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(tetrahedronRitem.get());


	mAllRitems.push_back(std::move(tetrahedronRitem));



	// tetrahedron_2

	auto tetrahedron2Ritem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&tetrahedron2Ritem->World, XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(7.0f, 18.0f, -7.0f));

	tetrahedron2Ritem->ObjCBIndex = 41;

	tetrahedron2Ritem->Mat = mMaterials["glass"].get();


	tetrahedron2Ritem->Geo = mGeometries["shapeGeo"].get();

	tetrahedron2Ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	tetrahedron2Ritem->IndexCount = tetrahedron2Ritem->Geo->DrawArgs["tetrahedron"].IndexCount;

	tetrahedron2Ritem->StartIndexLocation = tetrahedron2Ritem->Geo->DrawArgs["tetrahedron"].StartIndexLocation;

	tetrahedron2Ritem->BaseVertexLocation = tetrahedron2Ritem->Geo->DrawArgs["tetrahedron"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(tetrahedron2Ritem.get());


	mAllRitems.push_back(std::move(tetrahedron2Ritem));



	// pyramid

	auto pyramidRitem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&pyramidRitem->World, XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(-7.0f, 17.0f, 7.0f));

	pyramidRitem->ObjCBIndex = 42;

	pyramidRitem->Mat = mMaterials["glass"].get();


	pyramidRitem->Geo = mGeometries["shapeGeo"].get();

	pyramidRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	pyramidRitem->IndexCount = pyramidRitem->Geo->DrawArgs["pyramid"].IndexCount;

	pyramidRitem->StartIndexLocation = pyramidRitem->Geo->DrawArgs["pyramid"].StartIndexLocation;

	pyramidRitem->BaseVertexLocation = pyramidRitem->Geo->DrawArgs["pyramid"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(pyramidRitem.get());


	mAllRitems.push_back(std::move(pyramidRitem));



	// pyramid_2

	auto pyramid2Ritem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&pyramid2Ritem->World, XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(7.0f, 17.0f, 7.0f));

	pyramid2Ritem->ObjCBIndex = 43;

	pyramid2Ritem->Mat = mMaterials["glass"].get();


	pyramid2Ritem->Geo = mGeometries["shapeGeo"].get();

	pyramid2Ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	pyramid2Ritem->IndexCount = pyramid2Ritem->Geo->DrawArgs["pyramid"].IndexCount;

	pyramid2Ritem->StartIndexLocation = pyramid2Ritem->Geo->DrawArgs["pyramid"].StartIndexLocation;

	pyramid2Ritem->BaseVertexLocation = pyramid2Ritem->Geo->DrawArgs["pyramid"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(pyramid2Ritem.get());


	mAllRitems.push_back(std::move(pyramid2Ritem));



	// sphere_3

	auto sphere3Ritem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&sphere3Ritem->World, XMMatrixScaling(12.0f, 12.0f, 12.0f) * XMMatrixTranslation(0.0f, 16.0f, 0.0f));

	sphere3Ritem->ObjCBIndex = 44;

	sphere3Ritem->Mat = mMaterials["ball"].get();


	sphere3Ritem->Geo = mGeometries["shapeGeo"].get();

	sphere3Ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	sphere3Ritem->IndexCount = sphere3Ritem->Geo->DrawArgs["sphere"].IndexCount;

	sphere3Ritem->StartIndexLocation = sphere3Ritem->Geo->DrawArgs["sphere"].StartIndexLocation;

	sphere3Ritem->BaseVertexLocation = sphere3Ritem->Geo->DrawArgs["sphere"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(sphere3Ritem.get());


	mAllRitems.push_back(std::move(sphere3Ritem));

	//verticals
	
	BuildBox(45, XMMatrixTranslation(-39.667f, 5.0f, 45.334f), XMMatrixScaling(0.5f, 6.0f, 11.334f));
	BuildBox(46, XMMatrixTranslation(-39.667f, 5.0f, 22.667f), XMMatrixScaling(0.5f, 6.0f, 11.334f));
	BuildBox(47, XMMatrixTranslation(-39.667f, 5.0f, 0.0f), XMMatrixScaling(0.5f, 6.0f, 11.334f));
	BuildBox(48, XMMatrixTranslation(-39.667f, 5.0f, -22.667f), XMMatrixScaling(0.5f, 6.0f, 11.334f));

	BuildBox(49, XMMatrixTranslation(-28.334f, 5.0f, 45.334f), XMMatrixScaling(0.5f, 6.0f, 11.334f));
	BuildBox(50, XMMatrixTranslation(-28.334f, 5.0f, 34.0f), XMMatrixScaling(0.5f, 6.0f, 11.334f));
	BuildBox(51, XMMatrixTranslation(-28.334f, 5.0f, 11.334f), XMMatrixScaling(0.5f, 6.0f, 11.334f));
	BuildBox(52, XMMatrixTranslation(-28.334f, 5.0f, -45.334f), XMMatrixScaling(0.5f, 6.0f, 11.334f));

	BuildBox(53, XMMatrixTranslation(-17, 5.0f, 45.334f), XMMatrixScaling(0.5f, 6.0f, 11.334f));
	BuildBox(54, XMMatrixTranslation(-17, 5.0f, -22.667f), XMMatrixScaling(0.5f, 6.0f, 11.334f));
	BuildBox(55, XMMatrixTranslation(-17, 5.0f, -34), XMMatrixScaling(0.5f, 6.0f, 11.334f));

	BuildBox(56, XMMatrixTranslation(-5.66f, 5.0f, 34), XMMatrixScaling(0.5f, 6.0f, 11.334f));
	BuildBox(57, XMMatrixTranslation(-5.66f, 5.0f, -22.667f), XMMatrixScaling(0.5f, 6.0f, 11.334f));
	BuildBox(58, XMMatrixTranslation(-5.66f, 5.0f, -45.334f), XMMatrixScaling(0.5f, 6.0f, 11.334f));

	BuildBox(59, XMMatrixTranslation(5.66f, 5.0f, 34), XMMatrixScaling(0.5f, 6.0f, 11.334f));
	BuildBox(60, XMMatrixTranslation(5.66f, 5.0f, -34), XMMatrixScaling(0.5f, 6.0f, 11.334f));

	BuildBox(61, XMMatrixTranslation(17, 5.0f, 45.334f), XMMatrixScaling(0.5f, 6.0f, 11.334f));
	BuildBox(62, XMMatrixTranslation(17, 5.0f, 22.667f), XMMatrixScaling(0.5f, 6.0f, 11.334f));
	BuildBox(63, XMMatrixTranslation(17, 5.0f, -22.667f), XMMatrixScaling(0.5f, 6.0f, 11.334f));
	BuildBox(64, XMMatrixTranslation(17, 5.0f, -34), XMMatrixScaling(0.5f, 6.0f, 11.334f));
	BuildBox(65, XMMatrixTranslation(17, 5.0f, -45.334f), XMMatrixScaling(0.5f, 6.0f, 11.334f));

	BuildBox(66, XMMatrixTranslation(28.334f, 5.0f, 45.334f), XMMatrixScaling(0.5f, 6.0f, 11.334f));
	BuildBox(67, XMMatrixTranslation(28.334f, 5.0f, 22.667f), XMMatrixScaling(0.5f, 6.0f, 11.334f));
	BuildBox(68, XMMatrixTranslation(28.334f, 5.0f, 11.334f), XMMatrixScaling(0.5f, 6.0f, 11.334f));
	BuildBox(69, XMMatrixTranslation(28.334f, 5.0f, 0), XMMatrixScaling(0.5f, 6.0f, 11.334f));
	BuildBox(70, XMMatrixTranslation(28.334f, 5.0f, -22.667f), XMMatrixScaling(0.5f, 6.0f, 11.334f));
	BuildBox(71, XMMatrixTranslation(28.334f, 5.0f, -45.334f), XMMatrixScaling(0.5f, 6.0f, 11.334f));

	BuildBox(72, XMMatrixTranslation(39.667f, 5.0f, 45.334f), XMMatrixScaling(0.5f, 6.0f, 11.334f));
	BuildBox(73, XMMatrixTranslation(39.667f, 5.0f, 22.667f), XMMatrixScaling(0.5f, 6.0f, 11.334f));
	BuildBox(74, XMMatrixTranslation(39.667f, 5.0f, 0), XMMatrixScaling(0.5f, 6.0f, 11.334f));
	BuildBox(75, XMMatrixTranslation(39.667f, 5.0f, -34), XMMatrixScaling(0.5f, 6.0f, 11.334f));


	//horizontals
	BuildBox(76, XMMatrixTranslation(-45.334f, 5.0f, 5.66f), XMMatrixScaling(11.334f, 6.0f, 0.5f));
	BuildBox(77, XMMatrixTranslation(-45.334f, 5.0f, -5.66f), XMMatrixScaling(11.334f, 6.0f, 0.5f));

	BuildBox(78, XMMatrixTranslation(-34, 5.0f, 17), XMMatrixScaling(11.334f, 6.0f, 0.5f));
	BuildBox(79, XMMatrixTranslation(-34, 5.0f, 5.66f), XMMatrixScaling(11.334f, 6.0f, 0.5f));
	BuildBox(80, XMMatrixTranslation(-34, 5.0f, -17), XMMatrixScaling(11.334f, 6.0f, 0.5f));
	BuildBox(81, XMMatrixTranslation(-34, 5.0f, -28.33f), XMMatrixScaling(11.334f, 6.0f, 0.5f));
	BuildBox(82, XMMatrixTranslation(-34, 5.0f, -39.66f), XMMatrixScaling(11.334f, 6.0f, 0.5f));

	BuildBox(83, XMMatrixTranslation(-22.667f, 5.0f, 39.66f), XMMatrixScaling(11.334f, 6.0f, 0.5f));
	BuildBox(84, XMMatrixTranslation(-22.667f, 5.0f, 17), XMMatrixScaling(11.334f, 6.0f, 0.5f));

	BuildBox(85, XMMatrixTranslation(-11.334f, 5.0f, 28.334f), XMMatrixScaling(11.334f, 6.0f, 0.5f));

	BuildBox(86, XMMatrixTranslation(0, 5.0f, 39.667f), XMMatrixScaling(11.334f, 6.0f, 0.5f));
	BuildBox(87, XMMatrixTranslation(0, 5.0f, -28.334f), XMMatrixScaling(11.334f, 6.0f, 0.5f));

	BuildBox(88, XMMatrixTranslation(11.334f, 5.0f, 28.334f), XMMatrixScaling(11.334f, 6.0f, 0.5f));

	BuildBox(89, XMMatrixTranslation(22.667f, 5.0f, 39.667f), XMMatrixScaling(11.334f, 6.0f, 0.5f));
	BuildBox(90, XMMatrixTranslation(22.667f, 5.0f, -5.667f), XMMatrixScaling(11.334f, 6.0f, 0.5f));
	BuildBox(91, XMMatrixTranslation(22.667f, 5.0f, -28.334f), XMMatrixScaling(11.334f, 6.0f, 0.5f));
	BuildBox(92, XMMatrixTranslation(22.667f, 5.0f, -39.667f), XMMatrixScaling(11.334f, 6.0f, 0.5f));

	BuildBox(93, XMMatrixTranslation(34, 5.0f, 28.334f), XMMatrixScaling(11.334f, 6.0f, 0.5f));
	BuildBox(94, XMMatrixTranslation(34, 5.0f, 17), XMMatrixScaling(11.334f, 6.0f, 0.5f));
	BuildBox(95, XMMatrixTranslation(34, 5.0f, -17), XMMatrixScaling(11.334f, 6.0f, 0.5f));

	BuildBox(96, XMMatrixTranslation(45.334f, 5.0f, 5.66f), XMMatrixScaling(11.334f, 6.0f, 0.5f));
	BuildBox(97, XMMatrixTranslation(45.334f, 5.0f, -5.66f), XMMatrixScaling(11.334f, 6.0f, 0.5f));
	BuildBox(98, XMMatrixTranslation(45.334f, 5.0f, -28.334f), XMMatrixScaling(11.334f, 6.0f, 0.5f));
	BuildBox(99, XMMatrixTranslation(45.334f, 5.0f, -39.667f), XMMatrixScaling(11.334f, 6.0f, 0.5f));


	//perimeter
	BuildBox(100, XMMatrixTranslation(-51, 5.0f, 0), XMMatrixScaling(0.5f, 6.0f, 102));
	BuildBox(101, XMMatrixTranslation(51, 5.0f, 0), XMMatrixScaling(0.5f, 6.0f, 102));
	BuildBox(102, XMMatrixTranslation(0, 5.0f, 51), XMMatrixScaling(102, 6.0f, 0.5f));
	BuildBox(103, XMMatrixTranslation(-5.667f, 5.0f, -51), XMMatrixScaling(90.667f, 6.0f, 0.5f));


}

void TreeBillboardsApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	auto matCB = mCurrFrameResource->MaterialCB->Resource();

    // For each render item...
    for(size_t i = 0; i < ritems.size(); ++i)
    {
        auto ri = ritems[i];

        cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
        cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		tex.Offset(ri->Mat->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);

        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex*objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex*matCBByteSize;

		cmdList->SetGraphicsRootDescriptorTable(0, tex);
        cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
        cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);

        cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> TreeBillboardsApp::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return { 
		pointWrap, pointClamp,
		linearWrap, linearClamp, 
		anisotropicWrap, anisotropicClamp };
}

float TreeBillboardsApp::GetHillsHeight(float x, float z)const
{
    return 0.3f*(z*sinf(0.1f*x) + x*cosf(0.1f*z));
}

XMFLOAT3 TreeBillboardsApp::GetHillsNormal(float x, float z)const
{
    // n = (-df/dx, 1, -df/dz)
    XMFLOAT3 n(
        -0.03f*z*cosf(0.1f*x) - 0.3f*cosf(0.1f*z),
        1.0f,
        -0.3f*sinf(0.1f*x) + 0.03f*x*sinf(0.1f*z));

    XMVECTOR unitNormal = XMVector3Normalize(XMLoadFloat3(&n));
    XMStoreFloat3(&n, unitNormal);

    return n;
}

void TreeBillboardsApp::BuildBox(int cbIndex, const XMMATRIX& tranlate, const XMMATRIX& scale)
{
	auto box15Ritem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&box15Ritem->World, scale * tranlate);

	box15Ritem->ObjCBIndex = cbIndex;

	box15Ritem->Mat = mMaterials["brick"].get();


	box15Ritem->Geo = mGeometries["shapeGeo"].get();

	box15Ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	box15Ritem->IndexCount = box15Ritem->Geo->DrawArgs["box"].IndexCount;

	box15Ritem->StartIndexLocation = box15Ritem->Geo->DrawArgs["box"].StartIndexLocation;

	box15Ritem->BaseVertexLocation = box15Ritem->Geo->DrawArgs["box"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(box15Ritem.get());


	mAllRitems.push_back(std::move(box15Ritem));
}

/*
0 0 0
1 1
__________
_######_#_
_#____#_#_
_######_#_
________#_
_########_

1 1 1
2 2 2
1 2 1


*/