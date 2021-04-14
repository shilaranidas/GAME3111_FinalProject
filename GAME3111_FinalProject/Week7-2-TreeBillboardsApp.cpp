//***************************************************************************************
// TreeBillboardsApp.cpp 
//***************************************************************************************

#include "../Common/d3dApp.h"
#include "../Common/MathHelper.h"
#include "../Common/UploadBuffer.h"
#include "../Common/GeometryGenerator.h"
#include "../Common/Camera.h"
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
	//RenderItem(const RenderItem& rhs) = delete;
	BoundingBox Bounds;
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
    void BuildPSOs();
    void BuildFrameResources();
    void BuildMaterials();
    void BuildRenderItems();
	bool CheckCameraCollision(FXMVECTOR predictPos);
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);
	void CreateItem(const char* item, XMMATRIX p, XMMATRIX q, XMMATRIX r, UINT ObjIndex, const char* material);
	void CreateItemT(const char* item, XMMATRIX p, XMMATRIX q, XMMATRIX r, UINT ObjIndex, const char* material);
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

    float GetHillsHeight(float x, float z)const;
    XMFLOAT3 GetHillsNormal(float x, float z)const;

private:

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
	Camera mCamera;
	float mCameraSpeed = 10.f;
	BoundingBox mCameraBoundbox;
	bool mIsWireframe = false;
	/*XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

    float mTheta = 1.5f*XM_PI;
    float mPhi = XM_PIDIV2 - 0.1f;
    float mRadius = 50.0f;*/

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
void TreeBillboardsApp::CreateItem(const char* item, XMMATRIX p, XMMATRIX q,XMMATRIX r, UINT ObjIndex, const char* material)
{
    auto RightWall = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&RightWall->World, p * q * r);
    RightWall->ObjCBIndex = ObjIndex;
    RightWall->Mat = mMaterials[material].get();//"wirefence"
    RightWall->Geo = mGeometries["boxGeo"].get();
    
    RightWall->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	RightWall->Bounds = RightWall->Geo->DrawArgs[item].Bounds;
    RightWall->IndexCount = RightWall->Geo->DrawArgs[item].IndexCount;
    RightWall->StartIndexLocation = RightWall->Geo->DrawArgs[item].StartIndexLocation;
    RightWall->BaseVertexLocation = RightWall->Geo->DrawArgs[item].BaseVertexLocation;
    //mAllRitems.push_back(std::move(RightWall));
	mRitemLayer[(int)RenderLayer::Opaque].push_back(RightWall.get());
	mAllRitems.push_back(std::move(RightWall));
}
void TreeBillboardsApp::CreateItemT(const char* item, XMMATRIX p, XMMATRIX q, XMMATRIX r, UINT ObjIndex, const char* material)
{
	auto RightWall = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&RightWall->World, p * q * r);
	RightWall->ObjCBIndex = ObjIndex;
	RightWall->Mat = mMaterials[material].get();//"wirefence"
	RightWall->Geo = mGeometries["boxGeo"].get();

	RightWall->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	RightWall->Bounds = RightWall->Geo->DrawArgs[item].Bounds;
	RightWall->IndexCount = RightWall->Geo->DrawArgs[item].IndexCount;
	RightWall->StartIndexLocation = RightWall->Geo->DrawArgs[item].StartIndexLocation;
	RightWall->BaseVertexLocation = RightWall->Geo->DrawArgs[item].BaseVertexLocation;
	//mAllRitems.push_back(std::move(RightWall));
	mRitemLayer[(int)RenderLayer::Transparent].push_back(RightWall.get());
	mAllRitems.push_back(std::move(RightWall));
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

	//mCamera.SetPosition(-5.f, 50.0f, -100.0f);
	mCamera.SetPosition(0.f, 10.0f, -65.0f);
	mCameraBoundbox.Center = mCamera.GetPosition3f();
	mCameraBoundbox.Extents = XMFLOAT3(1.1f, 1.1f, 1.1f);

    mWaves = std::make_unique<Waves>(128, 128, 1.0f, 0.03f, 4.0f, 0.2f);
 
	LoadTextures();
    BuildRootSignature();
	BuildDescriptorHeaps();
    BuildShadersAndInputLayouts();
    BuildLandGeometry();
    BuildWavesGeometry();
	BuildBoxGeometry();
	BuildTreeSpritesGeometry();
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
   // XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    //XMStoreFloat4x4(&mProj, P);
	//step2: When the window is resized, we no longer rebuild the projection matrix explicitly, 
	//and instead delegate the work to the Camera class with SetLens:
	mCamera.SetLens(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
}

void TreeBillboardsApp::Update(const GameTimer& gt)
{
    OnKeyboardInput(gt);
//	UpdateCamera(gt);

    // Cycle through the circular frame resource array.
    mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
    mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

    // Has the GPU finished processing the commands of the current frame resource?
    // If not, wait until the GPU has completed commands up to this fence point.
    if(mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
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
		//step4: Instead of updating the angles based on input to orbit camera around scene, 
		//we rotate the camera�s look direction:
		mCamera.Pitch(dy);
		mCamera.RotateY(dx);
    }
    //else if((btnState & MK_RBUTTON) != 0)
    //{
    //    // Make each pixel correspond to 0.2 unit in the scene.
    //    float dx = 0.2f*static_cast<float>(x - mLastMousePos.x);
    //    float dy = 0.2f*static_cast<float>(y - mLastMousePos.y);

    //    // Update the camera radius based on input.
    //    mRadius += dx - dy;

    //    // Restrict the radius.
    //    mRadius = MathHelper::Clamp(mRadius, 5.0f, 150.0f);
    //}

    mLastMousePos.x = x;
    mLastMousePos.y = y;
}
 
void TreeBillboardsApp::OnKeyboardInput(const GameTimer& gt)
{
	
	
	if (GetAsyncKeyState('1') & 0x8000)
	{
		mCamera.SetPosition(-5.f, 50.0f, -100.0f);
	}
	else if (GetAsyncKeyState('2') & 0x8000)
	{
		mCamera.SetPosition(0.f, 10.0f, -65.0f);
	}
	/*if (GetAsyncKeyState('1') & 0x8000)
		mIsWireframe = true;
	else
		mIsWireframe = false;*/

	const float dt = gt.DeltaTime();

	////GetAsyncKeyState returns a short (2 bytes)
	XMVECTOR predictPos = XMVectorSet(0.f, 0.f, 0.f, 0.f);
	if (GetAsyncKeyState('W') & 0x8000) //most significant bit (MSB) is 1 when key is pressed (1000 000 000 000)
	{
		XMVECTOR s = XMVectorReplicate(mCameraSpeed * dt);
		predictPos = XMVectorMultiplyAdd(s, mCamera.GetLook(), mCamera.GetPosition());

		if (CheckCameraCollision(predictPos) == false)
			mCamera.Walk(mCameraSpeed * dt);
	}

	if (GetAsyncKeyState('S') & 0x8000)
	{
		XMVECTOR s = XMVectorReplicate(-mCameraSpeed * dt);
		predictPos = XMVectorMultiplyAdd(s, mCamera.GetLook(), mCamera.GetPosition());

		if (CheckCameraCollision(predictPos) == false)
			mCamera.Walk(-mCameraSpeed * dt);
	}


	if (GetAsyncKeyState('A') & 0x8000)
	{
		XMVECTOR s = XMVectorReplicate(-mCameraSpeed * dt);
		predictPos = XMVectorMultiplyAdd(s, mCamera.GetRight(), mCamera.GetPosition());

		if (CheckCameraCollision(predictPos) == false)
			mCamera.Strafe(-mCameraSpeed * dt);

	}

	if (GetAsyncKeyState('D') & 0x8000)
	{
		XMVECTOR s = XMVectorReplicate(mCameraSpeed * dt);
		predictPos = XMVectorMultiplyAdd(s, mCamera.GetRight(), mCamera.GetPosition());


		if (CheckCameraCollision(predictPos) == false)
			mCamera.Strafe(mCameraSpeed * dt);
	}

	//step1
	if (GetAsyncKeyState(VK_UP) & 0x8000)
	{
		XMVECTOR s = XMVectorReplicate(mCameraSpeed * dt);
		predictPos = XMVectorMultiplyAdd(s, mCamera.GetUp(), mCamera.GetPosition());

		if (CheckCameraCollision(predictPos) == false)
			mCamera.Pedestal(mCameraSpeed * dt);
	}

	if (GetAsyncKeyState(VK_DOWN) & 0x8000)
	{
		XMVECTOR s = XMVectorReplicate(-mCameraSpeed * dt);
		predictPos = XMVectorMultiplyAdd(s, mCamera.GetUp(), mCamera.GetPosition());

		if (CheckCameraCollision(predictPos) == false)
			mCamera.Pedestal(-mCameraSpeed * dt);
	}

	if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
		mCamera.Roll(10.0f * dt);

	if (GetAsyncKeyState(VK_LEFT) & 0x8000)
		mCamera.Roll(-10.0f * dt);


	mCamera.UpdateViewMatrix();
	mCameraBoundbox.Center = mCamera.GetPosition3f();
}
 
void TreeBillboardsApp::UpdateCamera(const GameTimer& gt)
{
	// Convert Spherical to Cartesian coordinates.
	//mEyePos.x = mRadius*sinf(mPhi)*cosf(mTheta);
	//mEyePos.z = mRadius*sinf(mPhi)*sinf(mTheta);
	//mEyePos.y = mRadius*cosf(mPhi);

	//// Build the view matrix.
	//XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
	//XMVECTOR target = XMVectorZero();
	//XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	//XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	//XMStoreFloat4x4(&mView, view);
}

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
	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	mMainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
	mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
	mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };
	mMainPassCB.Lights[3].Position = { 0.0f, 10.0f, 12.0f };
	mMainPassCB.Lights[3].Direction = { 5.0f, 0.0f, 0.0f };
	mMainPassCB.Lights[3].Strength = { 0.35f, 0.0f, 100.05f };
	mMainPassCB.Lights[3].SpotPower = 2.0;

	mMainPassCB.Lights[4].Position = { 25.0f, 10.0f, -8.0f };
	mMainPassCB.Lights[4].Strength = { 1000.0f, 1.0f, 0.05f };

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
	grassTex->Filename = L"../Texture/grass.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), grassTex->Filename.c_str(),
		grassTex->Resource, grassTex->UploadHeap));

	auto waterTex = std::make_unique<Texture>();
	waterTex->Name = "waterTex";
	waterTex->Filename = L"../Texture/water1.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), waterTex->Filename.c_str(),
		waterTex->Resource, waterTex->UploadHeap));

	auto fenceTex = std::make_unique<Texture>();
	fenceTex->Name = "fenceTex";
	fenceTex->Filename = L"../Texture/brick.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), fenceTex->Filename.c_str(),
		fenceTex->Resource, fenceTex->UploadHeap));

	auto stoneTex = std::make_unique<Texture>();
	stoneTex->Name = "stoneTex";
	stoneTex->Filename = L"../Texture/stone.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), stoneTex->Filename.c_str(),
		stoneTex->Resource, stoneTex->UploadHeap));

	auto sandTex = std::make_unique<Texture>();
	sandTex->Name = "sandTex";
	sandTex->Filename = L"../Texture/sand.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), sandTex->Filename.c_str(),
		sandTex->Resource, sandTex->UploadHeap));

	auto diamondTex = std::make_unique<Texture>();
	diamondTex->Name = "diamondTex";
	diamondTex->Filename = L"../Texture/diamond.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), diamondTex->Filename.c_str(),
		diamondTex->Resource, diamondTex->UploadHeap));

	auto torusTex = std::make_unique<Texture>();
	torusTex->Name = "torusTex";
	torusTex->Filename = L"../Texture/torus.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), torusTex->Filename.c_str(),
		torusTex->Resource, torusTex->UploadHeap));

	auto triprisTex = std::make_unique<Texture>();
	triprisTex->Name = "triprisTex";
	triprisTex->Filename = L"../Texture/WireFence.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), triprisTex->Filename.c_str(),
		triprisTex->Resource, triprisTex->UploadHeap));

	auto pyramidTex = std::make_unique<Texture>();
	pyramidTex->Name = "pyramidTex";
	pyramidTex->Filename = L"../Texture/pyramid.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), pyramidTex->Filename.c_str(),
		pyramidTex->Resource, pyramidTex->UploadHeap));

	auto ballTex = std::make_unique<Texture>();
	ballTex->Name = "ballTex";
	ballTex->Filename = L"../Texture/ball.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), ballTex->Filename.c_str(),
		ballTex->Resource, ballTex->UploadHeap));

	auto stairTex = std::make_unique<Texture>();
	stairTex->Name = "stairTex";
	stairTex->Filename = L"../Texture/stair.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), stairTex->Filename.c_str(),
		stairTex->Resource, stairTex->UploadHeap));
	auto mazeTex = std::make_unique<Texture>();
	mazeTex->Name = "mazeTex";
	mazeTex->Filename = L"../Texture/maze.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), mazeTex->Filename.c_str(),
		mazeTex->Resource, mazeTex->UploadHeap));

	auto treeArrayTex = std::make_unique<Texture>();
	treeArrayTex->Name = "treeArrayTex";
	treeArrayTex->Filename = L"../Texture/treeArray.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), treeArrayTex->Filename.c_str(),
		treeArrayTex->Resource, treeArrayTex->UploadHeap));

	

	mTextures[grassTex->Name] = std::move(grassTex);
	mTextures[waterTex->Name] = std::move(waterTex);
	mTextures[fenceTex->Name] = std::move(fenceTex);
	mTextures[stoneTex->Name] = std::move(stoneTex);
	mTextures[sandTex->Name] = std::move(sandTex);
	mTextures[diamondTex->Name] = std::move(diamondTex);
	mTextures[torusTex->Name] = std::move(torusTex);
	mTextures[triprisTex->Name] = std::move(triprisTex);
	mTextures[pyramidTex->Name] = std::move(pyramidTex);
	mTextures[ballTex->Name] = std::move(ballTex);
	mTextures[stairTex->Name] = std::move(stairTex);
	mTextures[mazeTex->Name] = std::move(mazeTex);
	mTextures[treeArrayTex->Name] = std::move(treeArrayTex);
	
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
	srvHeapDesc.NumDescriptors = 13;
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
	auto stoneTex = mTextures["stoneTex"]->Resource;
	auto sandTex = mTextures["sandTex"]->Resource;
	auto diamondTex = mTextures["diamondTex"]->Resource;
	auto torusTex = mTextures["torusTex"]->Resource;
	auto triprisTex = mTextures["triprisTex"]->Resource;
	auto pyramidTex = mTextures["pyramidTex"]->Resource;
	auto ballTex = mTextures["ballTex"]->Resource;
	auto stairTex = mTextures["stairTex"]->Resource;
	auto mazeTex = mTextures["mazeTex"]->Resource;
	auto treeArrayTex = mTextures["treeArrayTex"]->Resource;
	
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

	srvDesc.Format = stoneTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(stoneTex.Get(), &srvDesc, hDescriptor);
	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = sandTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(sandTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = diamondTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(diamondTex.Get(), &srvDesc, hDescriptor);
	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = torusTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(torusTex.Get(), &srvDesc, hDescriptor);
	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = triprisTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(triprisTex.Get(), &srvDesc, hDescriptor);
	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = pyramidTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(pyramidTex.Get(), &srvDesc, hDescriptor);
	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = ballTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(ballTex.Get(), &srvDesc, hDescriptor);
	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = stairTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(stairTex.Get(), &srvDesc, hDescriptor);
	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = mazeTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(mazeTex.Get(), &srvDesc, hDescriptor);
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

	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", defines, "PS", "ps_5_1");
	mShaders["alphaTestedPS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", alphaTestDefines, "PS", "ps_5_1");
	
	mShaders["treeSpriteVS"] = d3dUtil::CompileShader(L"Shaders\\TreeSprite.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["treeSpriteGS"] = d3dUtil::CompileShader(L"Shaders\\TreeSprite.hlsl", nullptr, "GS", "gs_5_1");
	mShaders["treeSpritePS"] = d3dUtil::CompileShader(L"Shaders\\TreeSprite.hlsl", alphaTestDefines, "PS", "ps_5_1");

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
    GeometryGenerator::MeshData grid = geoGen.CreateGrid(80.0f, 120.0f, 10, 10);

    //
    // Extract the vertex elements we are interested and apply the height function to
    // each vertex.  In addition, color the vertices based on their height so we have
    // sandy looking beaches, grassy low hills, and snow mountain peaks.
    //

    std::vector<Vertex> vertices(grid.Vertices.size());

	//Calculate Bound Box//step1 
	XMFLOAT3 vMinf3(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
	XMFLOAT3 vMaxf3(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);

	XMVECTOR vMin = XMLoadFloat3(&vMinf3);
	XMVECTOR vMax = XMLoadFloat3(&vMaxf3);

    for(size_t i = 0; i < grid.Vertices.size(); ++i)
    {
        auto& p = grid.Vertices[i].Position;
        vertices[i].Pos = p;
		vertices[i].Pos.y = 5;// GetHillsHeight(p.x, p.z);
        vertices[i].Normal = GetHillsNormal(p.x, p.z);
		vertices[i].TexC = grid.Vertices[i].TexC;

		//Calculate Bound Box
		//step 2
		XMVECTOR P = XMLoadFloat3(&vertices[i].Pos);
		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
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
	//Calculate Bound Box
	//step 3
	BoundingBox bounds;
	XMStoreFloat3(&bounds.Center, 0.5f * (vMin + vMax));
	XMStoreFloat3(&bounds.Extents, 0.5f * (vMax - vMin));
	//step 4
	submesh.Bounds = bounds;

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
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.5f, 15.0f, 1.5f, 3);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.5f, 3.0f, 20, 20);
	GeometryGenerator::MeshData cone = geoGen.CreateCone(1.f, 1.f, 40, 6);
	GeometryGenerator::MeshData pyramid = geoGen.CreatePyramid(1, 1, 1, 0);
	GeometryGenerator::MeshData wedge = geoGen.CreateWedge(1, 1, 1, 0);
	GeometryGenerator::MeshData diamond = geoGen.CreateDiamond(1, 2, 1, 0);
	GeometryGenerator::MeshData triangularPrism = geoGen.CreateTriangularPrism(1.0f, 1.0f, 1.0f, 2);
	GeometryGenerator::MeshData torus = geoGen.CreateTorus(1.0f, 0.2f, 16, 16);

	// Vertex Cache
	UINT boxVertexOffset = 0;
	UINT sphereVertexOffset = boxVertexOffset + (UINT)box.Vertices.size();
	UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();
	UINT coneVertexOffset = cylinderVertexOffset + (UINT)cylinder.Vertices.size();
	UINT pyramidVertexOffset = coneVertexOffset + (UINT)cone.Vertices.size();
	UINT wedgeVertexOffset = pyramidVertexOffset + (UINT)pyramid.Vertices.size();
	UINT diamondVertexOffset = wedgeVertexOffset + (UINT)wedge.Vertices.size();
	UINT triangularPrismVertexOffset = diamondVertexOffset + (UINT)diamond.Vertices.size();
	UINT torusVertexOffset = triangularPrismVertexOffset + (UINT)triangularPrism.Vertices.size();

	//Index Cache
	UINT boxIndexOffset = 0;
	UINT sphereIndexOffset = boxIndexOffset + (UINT)box.Indices32.size();
	UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();
	UINT coneIndexOffset = cylinderIndexOffset + (UINT)cylinder.Indices32.size();
	UINT pyramidIndexOffset = coneIndexOffset + (UINT)cone.Indices32.size();
	UINT wedgeIndexOffset = pyramidIndexOffset + (UINT)pyramid.Indices32.size();
	UINT diamondIndexOffset = wedgeIndexOffset + (UINT)wedge.Indices32.size();
	UINT triangularPrismIndexOffset = diamondIndexOffset + (UINT)diamond.Indices32.size();
	UINT torusIndexOffset = triangularPrismIndexOffset + (UINT)triangularPrism.Indices32.size();

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.BaseVertexLocation = boxVertexOffset;

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
	sphereSubmesh.StartIndexLocation = sphereIndexOffset;
	sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
	cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
	cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

	SubmeshGeometry coneSubmesh;
	coneSubmesh.IndexCount = (UINT)cone.Indices32.size();
	coneSubmesh.StartIndexLocation = coneIndexOffset;
	coneSubmesh.BaseVertexLocation = coneVertexOffset;


	SubmeshGeometry pyramidSubmesh;
	pyramidSubmesh.IndexCount = (UINT)pyramid.Indices32.size();
	pyramidSubmesh.StartIndexLocation = pyramidIndexOffset;
	pyramidSubmesh.BaseVertexLocation = pyramidVertexOffset;

	SubmeshGeometry wedgeSubmesh;
	wedgeSubmesh.IndexCount = (UINT)wedge.Indices32.size();
	wedgeSubmesh.StartIndexLocation = wedgeIndexOffset;
	wedgeSubmesh.BaseVertexLocation = wedgeVertexOffset;

	SubmeshGeometry diamondSubmesh;
	diamondSubmesh.IndexCount = (UINT)diamond.Indices32.size();
	diamondSubmesh.StartIndexLocation = diamondIndexOffset;
	diamondSubmesh.BaseVertexLocation = diamondVertexOffset;

	SubmeshGeometry triangularPrismSubmesh;
	triangularPrismSubmesh.IndexCount = (UINT)triangularPrism.Indices32.size();
	triangularPrismSubmesh.StartIndexLocation = triangularPrismIndexOffset;
	triangularPrismSubmesh.BaseVertexLocation = triangularPrismVertexOffset;

	SubmeshGeometry torusSubmesh;
	torusSubmesh.IndexCount = (UINT)torus.Indices32.size();
	torusSubmesh.StartIndexLocation = torusIndexOffset;
	torusSubmesh.BaseVertexLocation = torusVertexOffset;
	auto totalVertexCount =
		box.Vertices.size() +

		sphere.Vertices.size() +
		cylinder.Vertices.size() +
		cone.Vertices.size() +
		pyramid.Vertices.size() +
		wedge.Vertices.size() +
		diamond.Vertices.size() +
		triangularPrism.Vertices.size() +
		torus.Vertices.size();

	std::vector<Vertex> vertices(totalVertexCount);

	//Calculate Bound Box//step1 
	XMFLOAT3 vMinf3(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
	XMFLOAT3 vMaxf3(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);

	XMVECTOR vMin = XMLoadFloat3(&vMinf3);
	XMVECTOR vMax = XMLoadFloat3(&vMaxf3);


	UINT k = 0;
	for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		auto& p = box.Vertices[i].Position;
		vertices[k].Pos = p;
		vertices[k].Normal = box.Vertices[i].Normal;
		vertices[k].TexC = box.Vertices[i].TexC;

		// Calculate Bound Box
			//step 2
			XMVECTOR P = XMLoadFloat3(&box.Vertices[i].Position);
		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}
	//step 3
	BoundingBox bounds;
	XMStoreFloat3(&bounds.Center, 0.5f * (vMin + vMax));
	XMStoreFloat3(&bounds.Extents, 0.5f * (vMax - vMin));
	//step 4
	boxSubmesh.Bounds = bounds;
	vMin = XMLoadFloat3(&vMinf3);
	vMax = XMLoadFloat3(&vMaxf3);
	for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Normal = sphere.Vertices[i].Normal;
		vertices[k].TexC = sphere.Vertices[i].TexC;
		//vertices[k].Color = XMFLOAT4(DirectX::Colors::Crimson);

		// Calculate Bound Box
			//step 2
			XMVECTOR P = XMLoadFloat3(&sphere.Vertices[i].Position);
		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}
	XMStoreFloat3(&bounds.Center, 0.5f * (vMin + vMax));
	XMStoreFloat3(&bounds.Extents, 0.5f * (vMax - vMin));
	//step 4
	sphereSubmesh.Bounds = bounds;
	vMin = XMLoadFloat3(&vMinf3);
	vMax = XMLoadFloat3(&vMaxf3);
	for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cylinder.Vertices[i].Position;
		vertices[k].Normal = cylinder.Vertices[i].Normal;
		vertices[k].TexC = cylinder.Vertices[i].TexC;
		//vertices[k].Color = XMFLOAT4(DirectX::Colors::SteelBlue);
		// Calculate Bound Box
			//step 2
		XMVECTOR P = XMLoadFloat3(&cylinder.Vertices[i].Position);
		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}
	XMStoreFloat3(&bounds.Center, 0.5f * (vMin + vMax));
	XMStoreFloat3(&bounds.Extents, 0.5f * (vMax - vMin));
	//step 4
	cylinderSubmesh.Bounds = bounds;
	vMin = XMLoadFloat3(&vMinf3);
	vMax = XMLoadFloat3(&vMaxf3);
	for (size_t i = 0; i < cone.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cone.Vertices[i].Position;
		vertices[k].Normal = cone.Vertices[i].Normal;
		vertices[k].TexC = cone.Vertices[i].TexC;
		// vertices[k].Color = XMFLOAT4(DirectX::Colors::Blue);
		// Calculate Bound Box
			//step 2
		XMVECTOR P = XMLoadFloat3(&cone.Vertices[i].Position);
		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}
	XMStoreFloat3(&bounds.Center, 0.5f * (vMin + vMax));
	XMStoreFloat3(&bounds.Extents, 0.5f * (vMax - vMin));
	//step 4
	coneSubmesh.Bounds = bounds;
	vMin = XMLoadFloat3(&vMinf3);
	vMax = XMLoadFloat3(&vMaxf3);
	for (size_t i = 0; i < pyramid.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = pyramid.Vertices[i].Position;
		vertices[k].Normal = pyramid.Vertices[i].Normal;
		vertices[k].TexC = pyramid.Vertices[i].TexC;
		// vertices[k].Color = XMFLOAT4(DirectX::Colors::Blue);
		// Calculate Bound Box
			//step 2
		XMVECTOR P = XMLoadFloat3(&pyramid.Vertices[i].Position);
		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}
	XMStoreFloat3(&bounds.Center, 0.5f * (vMin + vMax));
	XMStoreFloat3(&bounds.Extents, 0.5f * (vMax - vMin));
	//step 4
	pyramidSubmesh.Bounds = bounds;
	vMin = XMLoadFloat3(&vMinf3);
	vMax = XMLoadFloat3(&vMaxf3);
	for (size_t i = 0; i < wedge.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = wedge.Vertices[i].Position;
		vertices[k].Normal = wedge.Vertices[i].Normal;
		vertices[k].TexC = wedge.Vertices[i].TexC;
		// vertices[k].Color = XMFLOAT4(DirectX::Colors::Blue);
		// Calculate Bound Box
			//step 2
		XMVECTOR P = XMLoadFloat3(&wedge.Vertices[i].Position);
		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}
	XMStoreFloat3(&bounds.Center, 0.5f * (vMin + vMax));
	XMStoreFloat3(&bounds.Extents, 0.5f * (vMax - vMin));
	//step 4
	wedgeSubmesh.Bounds = bounds;
	vMin = XMLoadFloat3(&vMinf3);
	vMax = XMLoadFloat3(&vMaxf3);
	for (size_t i = 0; i < diamond.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = diamond.Vertices[i].Position;
		vertices[k].Normal = diamond.Vertices[i].Normal;
		vertices[k].TexC = diamond.Vertices[i].TexC;
		// vertices[k].Color = XMFLOAT4(DirectX::Colors::Blue);
		// Calculate Bound Box
			//step 2
		XMVECTOR P = XMLoadFloat3(&diamond.Vertices[i].Position);
		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}
	XMStoreFloat3(&bounds.Center, 0.5f * (vMin + vMax));
	XMStoreFloat3(&bounds.Extents, 0.5f * (vMax - vMin));
	//step 4
	diamondSubmesh.Bounds = bounds;
	vMin = XMLoadFloat3(&vMinf3);
	vMax = XMLoadFloat3(&vMaxf3);
	for (size_t i = 0; i < triangularPrism.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = triangularPrism.Vertices[i].Position;
		vertices[k].Normal = triangularPrism.Vertices[i].Normal;
		vertices[k].TexC = triangularPrism.Vertices[i].TexC;
		// vertices[k].Color = XMFLOAT4(DirectX::Colors::Blue);
		// Calculate Bound Box
			//step 2
		XMVECTOR P = XMLoadFloat3(&triangularPrism.Vertices[i].Position);
		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}
	XMStoreFloat3(&bounds.Center, 0.5f * (vMin + vMax));
	XMStoreFloat3(&bounds.Extents, 0.5f * (vMax - vMin));
	//step 4
	triangularPrismSubmesh.Bounds = bounds;
	vMin = XMLoadFloat3(&vMinf3);
	vMax = XMLoadFloat3(&vMaxf3);
	for (size_t i = 0; i < torus.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = torus.Vertices[i].Position;
		vertices[k].Normal = torus.Vertices[i].Normal;
		vertices[k].TexC = torus.Vertices[i].TexC;
		// vertices[k].Color = XMFLOAT4(DirectX::Colors::Blue);
		// Calculate Bound Box
			//step 2
		XMVECTOR P = XMLoadFloat3(&torus.Vertices[i].Position);
		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}
	XMStoreFloat3(&bounds.Center, 0.5f * (vMin + vMax));
	XMStoreFloat3(&bounds.Extents, 0.5f * (vMax - vMin));
	//step 4
	torusSubmesh.Bounds = bounds;
	vMin = XMLoadFloat3(&vMinf3);
	vMax = XMLoadFloat3(&vMaxf3);
	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));

	indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
	indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));
	indices.insert(indices.end(), std::begin(cone.GetIndices16()), std::end(cone.GetIndices16()));
	indices.insert(indices.end(), std::begin(pyramid.GetIndices16()), std::end(pyramid.GetIndices16()));
	indices.insert(indices.end(), std::begin(wedge.GetIndices16()), std::end(wedge.GetIndices16()));
	indices.insert(indices.end(), std::begin(diamond.GetIndices16()), std::end(diamond.GetIndices16()));
	indices.insert(indices.end(), std::begin(triangularPrism.GetIndices16()), std::end(triangularPrism.GetIndices16()));
	indices.insert(indices.end(), std::begin(torus.GetIndices16()), std::end(torus.GetIndices16()));
	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);


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

	//SubmeshGeometry submesh;
	//submesh.IndexCount = (UINT)indices.size();
	//submesh.StartIndexLocation = 0;
	//submesh.BaseVertexLocation = 0; 
	

		geo->DrawArgs["box"] = boxSubmesh;

	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;
	geo->DrawArgs["cone"] = coneSubmesh;
	geo->DrawArgs["pyramid"] = pyramidSubmesh;
	geo->DrawArgs["wedge"] = wedgeSubmesh;
	geo->DrawArgs["diamond"] = diamondSubmesh;
	geo->DrawArgs["triangularPrism"] = triangularPrismSubmesh;
	geo->DrawArgs["torus"] = torusSubmesh;

	mGeometries[geo->Name] = std::move(geo);
}

void TreeBillboardsApp::BuildTreeSpritesGeometry()
{
	//step5
	struct TreeSpriteVertex
	{
		XMFLOAT3 Pos;
		XMFLOAT2 Size;
	};

	static const int treeCount = 4;
	std::array<TreeSpriteVertex, 16> vertices;
	//for(UINT i = 0; i < treeCount; ++i)
	//{
	//	float x = MathHelper::RandF(-30.0f, 40.0f);
	//	float z = MathHelper::RandF(-45.0f, -25.0f);
	//	float y = GetHillsHeight(x, z);

	//	// Move tree slightly above land height.
	float	y = 14.0f;

	//	vertices[i].Pos = XMFLOAT3(x, y, z);
	//	vertices[i].Size = XMFLOAT2(20.0f, 20.0f);
	//}
	vertices[0].Pos = XMFLOAT3(-35.0f, y, -3.0f);
	vertices[0].Size = XMFLOAT2(20.0f, 20.0f);
	vertices[1].Pos = XMFLOAT3(-32.0f, y,-20.0f);
	vertices[1].Size = XMFLOAT2(20.0f, 20.0f);
	vertices[2].Pos = XMFLOAT3(33.0f, y, -25.0f);
	vertices[2].Size = XMFLOAT2(20.0f, 20.0f);
	vertices[3].Pos = XMFLOAT3(33.0f, y, -5.0f);
	vertices[3].Size = XMFLOAT2(20.0f, 20.0f);
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

	//there is abug with F2 key that is supposed to turn on the multisampling!
//Set4xMsaaState(true);
	//m4xMsaaState = true;

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

	//transparentPsoDesc.BlendState.AlphaToCoverageEnable = true;

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
	//step1
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
	int i = 0;
	auto grass = std::make_unique<Material>();
	grass->Name = "grass";
	grass->MatCBIndex = i;
	grass->DiffuseSrvHeapIndex = i;
	grass->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	grass->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	grass->Roughness = 0.125f;
	i++;
	// This is not a good water material definition, but we do not have all the rendering
	// tools we need (transparency, environment reflection), so we fake it for now.
	auto water = std::make_unique<Material>();
	water->Name = "water";
	water->MatCBIndex = i;
	water->DiffuseSrvHeapIndex = i;
	water->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.5f);
	water->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	water->Roughness = 0.0f;
	i++;
	auto wirefence = std::make_unique<Material>();
	wirefence->Name = "wirefence";
	wirefence->MatCBIndex = i;
	wirefence->DiffuseSrvHeapIndex = i;
	wirefence->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	wirefence->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	wirefence->Roughness = 0.25f;
	i++;
	auto stone = std::make_unique<Material>();
	stone->Name = "stone";
	stone->MatCBIndex =i;
	stone->DiffuseSrvHeapIndex = i;
	stone->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	stone->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	stone->Roughness = 0.2f;
	i++;
	auto sand = std::make_unique<Material>();
	sand->Name = "sand";
	sand->MatCBIndex = i;
	sand->DiffuseSrvHeapIndex = i;
	sand->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	sand->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	sand->Roughness = 0.9f;
	i++;
	auto diamond = std::make_unique<Material>();
	diamond->Name = "diamond";
	diamond->MatCBIndex = i;
	diamond->DiffuseSrvHeapIndex = i;
	diamond->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	diamond->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	diamond->Roughness = 0.9f;
	i++;
	auto torus = std::make_unique<Material>();
	torus->Name = "torus";
	torus->MatCBIndex = i;
	torus->DiffuseSrvHeapIndex = i;
	torus->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	torus->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	torus->Roughness = 0.9f;
	i++;
	auto tripris = std::make_unique<Material>();
	tripris->Name = "tripris";
	tripris->MatCBIndex = i;
	tripris->DiffuseSrvHeapIndex = i;
	tripris->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	tripris->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	tripris->Roughness = 0.9f;
	i++;
	auto pyramid = std::make_unique<Material>();
	pyramid->Name = "pyramid";
	pyramid->MatCBIndex = i;
	pyramid->DiffuseSrvHeapIndex = i;
	pyramid->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	pyramid->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	pyramid->Roughness = 0.9f;
	i++;
	auto ball = std::make_unique<Material>();
	ball->Name = "ball";
	ball->MatCBIndex = i;
	ball->DiffuseSrvHeapIndex = i;
	ball->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	ball->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	ball->Roughness = 0.9f;
	i++;
	auto stair = std::make_unique<Material>();
	stair->Name = "stair";
	stair->MatCBIndex = i;
	stair->DiffuseSrvHeapIndex = i;
	stair->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	stair->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	stair->Roughness = 0.9f;
	i++;
	auto maze = std::make_unique<Material>();
	maze->Name = "maze";
	maze->MatCBIndex = i;
	maze->DiffuseSrvHeapIndex = i;
	maze->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	maze->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	maze->Roughness = 0.9f;
	i++;
	auto treeSprites = std::make_unique<Material>();
	treeSprites->Name = "treeSprites";
	treeSprites->MatCBIndex =i;
	treeSprites->DiffuseSrvHeapIndex = i;
	treeSprites->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	treeSprites->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	treeSprites->Roughness = 0.125f;

	

	mMaterials["grass"] = std::move(grass);
	mMaterials["water"] = std::move(water);
	mMaterials["wirefence"] = std::move(wirefence);
	mMaterials["stone"] = std::move(stone);
	mMaterials["sand"] = std::move(sand);
	mMaterials["diamond"] = std::move(diamond);
	mMaterials["torus"] = std::move(torus);
	mMaterials["tripris"] = std::move(tripris);
	mMaterials["pyramid"] = std::move(pyramid);
	mMaterials["ball"] = std::move(ball);
	mMaterials["stair"] = std::move(stair);
	mMaterials["maze"] = std::move(maze);
	mMaterials["treeSprites"] = std::move(treeSprites);

	
}

void TreeBillboardsApp::BuildRenderItems()
{
	UINT objCBIndex = 0;
    auto wavesRitem = std::make_unique<RenderItem>();
    wavesRitem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&wavesRitem->TexTransform, XMMatrixScaling(5.0f, 5.0f, 1.0f));
	wavesRitem->ObjCBIndex = objCBIndex;
	wavesRitem->Mat = mMaterials["water"].get();
	wavesRitem->Geo = mGeometries["waterGeo"].get();
	wavesRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	wavesRitem->IndexCount = wavesRitem->Geo->DrawArgs["grid"].IndexCount;
	wavesRitem->StartIndexLocation = wavesRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	wavesRitem->BaseVertexLocation = wavesRitem->Geo->DrawArgs["grid"].BaseVertexLocation;

    mWavesRitem = wavesRitem.get();

	mRitemLayer[(int)RenderLayer::Transparent].push_back(wavesRitem.get());

    auto gridRitem = std::make_unique<RenderItem>();
    gridRitem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&gridRitem->TexTransform, XMMatrixScaling(5.0f, 5.0f, 1.0f) );
	objCBIndex++;
	gridRitem->ObjCBIndex = objCBIndex;
	gridRitem->Mat = mMaterials["grass"].get();
	gridRitem->Geo = mGeometries["landGeo"].get();
	gridRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->Bounds = gridRitem->Geo->DrawArgs["grid"].Bounds;
	
    gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
    gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
    gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::Opaque].push_back(gridRitem.get());
	//UINT objCBIndex = 0;
	objCBIndex++;
	CreateItem("box", XMMatrixScaling(30.0f, 1.0f, 1.0f), XMMatrixTranslation(0.0f, 10.0f, 25.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "wirefence");//back wall
	objCBIndex++;
	CreateItem("box", XMMatrixScaling(14.0f, 1.0f, 1.0f), XMMatrixTranslation(-16.0f, 10.0f, -1.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "wirefence");//front left wall
	objCBIndex++;
	CreateItem("box", XMMatrixScaling(14.0f, 1.0f, 1.0f), XMMatrixTranslation(16.0f, 10.0f, -1.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "wirefence");//front right wall
	objCBIndex++;
	CreateItem("box", XMMatrixScaling(1.0f, 1.0f, 14.0f), XMMatrixTranslation(25.0f, 10.0f, 12.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "wirefence");//left wall
	objCBIndex++;
	CreateItem("box", XMMatrixScaling(1.0f, 1.0f, 14.0f), XMMatrixTranslation(-25.0f, 10.0f, 12.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "wirefence");//right wall
	objCBIndex++;
	CreateItem("cylinder", XMMatrixScaling(5.0f, 5.5f, 5.0f), XMMatrixTranslation(25.0f, 10.0f, 25.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "stone");// back right 
	objCBIndex++;
	CreateItem("cylinder", XMMatrixScaling(5.0f, 5.5f, 5.0f), XMMatrixTranslation(-25.0f, 10.0f, 25.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "stone");// back left 
	objCBIndex++;
	CreateItem("cylinder", XMMatrixScaling(5.0f, 5.5f, 5.0f), XMMatrixTranslation(25.0f, 10.0f, -1.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "stone");// back right 
	objCBIndex++;
	CreateItem("cylinder", XMMatrixScaling(5.0f, 5.5f, 5.0f), XMMatrixTranslation(-25.0f, 10.0f, -1.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "stone");// back left 
	objCBIndex++;
	CreateItem("cone", XMMatrixScaling(4.0f, 5.5f, 4.0f), XMMatrixTranslation(25.0f, 20.0f, 25.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "sand");// back right 
	objCBIndex++;
	CreateItem("cone", XMMatrixScaling(4.0f, 5.5f, 4.0f), XMMatrixTranslation(-25.0f, 20.0f, 25.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "sand");// back left 
	objCBIndex++;
	CreateItem("cone", XMMatrixScaling(4.0f, 5.5f, 4.0f), XMMatrixTranslation(25.0f, 20.0f, -1.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "sand");// back right 
	objCBIndex++;
	CreateItem("cone", XMMatrixScaling(4.0f, 5.5f, 4.0f), XMMatrixTranslation(-25.0f, 20.0f, -1.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "sand");// back right 
	objCBIndex++;
	CreateItem("diamond", XMMatrixScaling(2.0f, 4.0f, 2.0f), XMMatrixTranslation(25.0f, 25.0f, 25.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "diamond");// back right 
	objCBIndex++;
	CreateItem("diamond", XMMatrixScaling(2.0f, 4.0f, 2.0f), XMMatrixTranslation(-25.0f, 25.0f, 25.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "diamond");// back left 
	objCBIndex++;
	CreateItem("diamond", XMMatrixScaling(2.0f, 4.0f, 2.0f), XMMatrixTranslation(25.0f, 25.0f, -1.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "diamond");// back right 
	objCBIndex++;
	CreateItem("diamond", XMMatrixScaling(2.0f, 4.0f, 2.0f), XMMatrixTranslation(-25.0f, 25.0f, -1.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "diamond");// back right 
	objCBIndex++;
	CreateItem("sphere", XMMatrixScaling(5.0f, 5.0f, 5.0f), XMMatrixTranslation(0.0f, 17.0f, 13.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "ball");// back left 
	objCBIndex++;
	CreateItem("pyramid", XMMatrixScaling(10.0f, 10.0f, 10.0f), XMMatrixTranslation(0.0f, 10.0f, 13.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "pyramid");// back left 
	objCBIndex++;
	CreateItem("wedge", XMMatrixScaling(11.0f, 5.0f, 10.0f), XMMatrixTranslation(0.0f, 7.0f, -5.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "stair");// back left 
	objCBIndex++;
	CreateItemT("triangularPrism", XMMatrixScaling(5.0f, 5.0f, 5.0f), XMMatrixTranslation(7.0f, -25.0f, -8.0f), XMMatrixRotationRollPitchYaw( 0.0f, 0.f, XM_PIDIV2), objCBIndex, "tripris");// back left 
	objCBIndex++;
	CreateItem("torus", XMMatrixScaling(3.0f, 3.0f, 3.0f), XMMatrixTranslation(24.8f, 12.0f, -8.0f), XMMatrixRotationRollPitchYaw(0.0f, 0.f, 0.0f), objCBIndex, "torus");// back left 
	objCBIndex++;
	////start maze
	CreateItem("box", XMMatrixScaling(12.0f, 1.0f, .5f), XMMatrixTranslation(12.0f, 10.0f, -14.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "maze");//back wall
	objCBIndex++;
	CreateItem("box", XMMatrixScaling(12.0f, 1.0f, .5f), XMMatrixTranslation(-12.0f, 10.0f, -14.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "maze");//front wall
	objCBIndex++;
	CreateItem("box", XMMatrixScaling(12.0f, 1.0f, .5f), XMMatrixTranslation(12.0f, 10.0f, -48.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "maze");//back wall
	objCBIndex++;
	CreateItem("box", XMMatrixScaling(12.0f, 1.0f, .5f), XMMatrixTranslation(-12.0f, 10.0f, -48.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "maze");//front wall
	objCBIndex++;
	CreateItem("box", XMMatrixScaling(0.5f, 1.0f, 23.0f), XMMatrixTranslation(20.6f, 10.0f, -31.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "maze");//Right wall
	objCBIndex++;
	CreateItem("box", XMMatrixScaling(0.5f, 1.0f, 23.0f), XMMatrixTranslation(-20.6f, 10.0f, -31.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "maze");//Left wall
	objCBIndex++;
	////inner maze
	CreateItem("box", XMMatrixScaling(0.5f, 1.0f, 19.5f), XMMatrixTranslation(-12.6f, 10.0f, -28.5f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "maze");
	objCBIndex++;
	CreateItem("box", XMMatrixScaling(0.5f, 1.0f, 5.0f), XMMatrixTranslation(3.4f, 10.0f, -18.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "maze");
	objCBIndex++;
	CreateItem("box", XMMatrixScaling(0.5f, 1.0f, 3.0f), XMMatrixTranslation(-5.6f, 10.0f, -24.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "maze");
	objCBIndex++;
	CreateItem("box", XMMatrixScaling(12.0f, 1.0f, .5f), XMMatrixTranslation(3.5f, 10.0f, -22.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "maze");
	objCBIndex++;
	CreateItem("box", XMMatrixScaling(12.0f, 1.0f, .5f), XMMatrixTranslation(-4.0f, 10.0f, -33.5f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "maze");
	objCBIndex++;
	CreateItem("box", XMMatrixScaling(12.0f, 1.0f, .5f), XMMatrixTranslation(6.0f, 10.0f, -38.8f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "maze");
	objCBIndex++;
	CreateItem("box", XMMatrixScaling(0.5f, 1.0f, 3.0f), XMMatrixTranslation(4.6f, 10.0f, -31.5f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "maze");
	objCBIndex++;
	CreateItem("box", XMMatrixScaling(0.5f, 1.0f, 6.5f), XMMatrixTranslation(14.4f, 10.0f, -34.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "maze");
	objCBIndex++;
	CreateItem("box", XMMatrixScaling(6.0f, 1.0f, .5f), XMMatrixTranslation(9.5f, 10.0f, -29.7f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "maze");
	objCBIndex++;
	
	/*auto boxRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem->World, XMMatrixTranslation(3.0f, 2.0f, -9.0f));
	boxRitem->ObjCBIndex = 2;
	boxRitem->Mat = mMaterials["wirefence"].get();
	boxRitem->Geo = mGeometries["boxGeo"].get();
	boxRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(boxRitem.get());*/

	auto treeSpritesRitem = std::make_unique<RenderItem>();
	treeSpritesRitem->World = MathHelper::Identity4x4();
	treeSpritesRitem->ObjCBIndex = objCBIndex;
	treeSpritesRitem->Mat = mMaterials["treeSprites"].get();
	treeSpritesRitem->Geo = mGeometries["treeSpritesGeo"].get();
	//step2
	treeSpritesRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
	treeSpritesRitem->IndexCount = treeSpritesRitem->Geo->DrawArgs["points"].IndexCount;
	treeSpritesRitem->StartIndexLocation = treeSpritesRitem->Geo->DrawArgs["points"].StartIndexLocation;
	treeSpritesRitem->BaseVertexLocation = treeSpritesRitem->Geo->DrawArgs["points"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTestedTreeSprites].push_back(treeSpritesRitem.get());

    mAllRitems.push_back(std::move(wavesRitem));
    mAllRitems.push_back(std::move(gridRitem));
	//mAllRitems.push_back(std::move(boxRitem));
	mAllRitems.push_back(std::move(treeSpritesRitem));
}
bool TreeBillboardsApp::CheckCameraCollision(FXMVECTOR predictPos)
{
	for (auto ri : mRitemLayer[(int)RenderLayer::Opaque])
	{
		BoundingBox tempCameraBound;
		XMStoreFloat3(&tempCameraBound.Center, predictPos);
		tempCameraBound.Extents = mCameraBoundbox.Extents;


		BoundingBox localCameraBound;

		XMMATRIX W = XMLoadFloat4x4(&ri->World);
		XMMATRIX invWorld = XMMatrixInverse(&XMMatrixDeterminant(W), W);

		tempCameraBound.Transform(localCameraBound, invWorld);

		if (ri->Bounds.Intersects(localCameraBound))
		{
			return true;
		}
	}

	return false;
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
		//step3
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
