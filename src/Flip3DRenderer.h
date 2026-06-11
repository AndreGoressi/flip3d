#pragma once

#include "Config.h"
#include "WindowCapture.h"

// ============================================================================
// Main Flip3D D3D11 prototype application class
// ============================================================================
class Flip3DRenderer
{
public:
    bool Initialize(HINSTANCE instance);
    HWND RenderHandle() const { return m_hwnd; }
    int Run();

    // Exposed for WndProc forwarding
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    // Card / window model — uses std::list matching uDWM's linked-list architecture
    void BuildCardModels();
    void CreateWindowCaptures();
    int FrontCardIndex() const;                        // returns 0 (front = head of list)
    int ResolveOriginalFrontIndex() const;
    int DistanceBetween(size_t sourcePos, size_t targetPos, bool forward) const;

    // creation
    bool ApplyAcrylic(HWND hwnd);
    bool Render3Dstack();

    // D3D initialization
    HRESULT InitializeD3D();
    HRESULT CreateDeviceResources();
    HRESULT CreateWindowSizeResources(bool resizeBuffers);

    // Per-frame update
    void Update(float deltaSeconds);
    void OnGlobalTimeUpdated();
    void TickRepeatedRotate();

    // View state management
    void ReplayEnterAnimation();
    void BeginExitView();
    bool IsInteractiveKeyboardState() const;
    bool IsSelectionKeyboardState() const;
    bool IsSelectionInputState() const;
    void BeginExitAnimation();

    // Rotation — physically rearranges the list matching uDWM RotateList
    // backward: back card → front   forward: front card → back
    void RotateListPhysically(bool backward);
    void StartRotationStep(bool backward, float duration);

    void BeginRotateToThumbnail(HWND targetHwnd);
    void SelectThumbnail(HWND targetHwnd);
    void SelectFrontThumbnail();




    void RotateBy(int delta);
    void RotateTo(int targetIndex);
    void RotateListByMouseWheelAmount(int mouseWheelAmount);
    void ContinueMouseWheelIfNeeded();
    void ContinueKeyboardRepeatIfNeeded();
    float RotationDurationForRotateList() const;
    bool ShouldReverseHorizontalWheel() const;

    // Input processing
    bool ProcessKeyboardInput(bool isKeyDown, UINT vkCode, bool isRepeat);
    bool ProcessMouseInput(UINT message, WPARAM wParam, LPARAM lParam);
    bool ProcessMouseWheelInput(int mouseWheelAmount, bool horizontalWheel);
    bool ProcessMouseButtonInput(LONG x, LONG y, bool pressed);

    // Hit testing (uDWM-style 3D ray-triangle intersection)
    bool IntersectRayTriangle(const XMFLOAT3 &origin, const XMFLOAT3 &dir,
        const XMFLOAT3 &v0, const XMFLOAT3 &v1, const XMFLOAT3 &v2,
        float &t, float &u) const;
    int HitTest3DScene(LONG x, LONG y) const;



    bool SelectThumbnailAtPoint(LONG x, LONG y);

    // Drawing / rendering
    float EnterProgress() const;
    XMMATRIX BuildViewMatrix(float enterProgress) const;
    XMMATRIX BuildProjectionMatrix(float enterProgress) const;
    DrawBuildContext CreateDrawBuildContext() const;
    float SteadyOpacityForRelative(const DrawBuildContext &context, float relative) const;
    std::vector<VisibleCardStructure> BuildVisibleCardStructure(const DrawBuildContext &context) const;
    CardAnimationState CreateBaseCardAnimationState(size_t position, const DrawBuildContext &context) const;
    CardAnimationState ResolveOutgoingCardAnimationState(const DrawBuildContext &context, CardAnimationState state) const;
    CardAnimationState ResolveIncomingCardAnimationState(const VisibleCardStructure &entry, const DrawBuildContext &context, CardAnimationState state) const;
    CardAnimationState ResolveSteadyCardAnimationState(const VisibleCardStructure &entry, const DrawBuildContext &context, CardAnimationState state) const;
    CardAnimationState ResolveCycleCardAnimationState(const DrawBuildContext &context, CardAnimationState state) const;
    CardAnimationState ResolveCardAnimationState(const VisibleCardStructure &entry, const DrawBuildContext &context) const;
    CardWorldState GetWorldFromParametric(const DrawBuildContext &context, const CardModel &card, size_t position, const CardAnimationState &animationState, float enterProgress) const;
    float ResolveDrawItemOpacity(const VisibleCardStructure &entry, const DrawBuildContext &context, const CardAnimationState &animationState) const;
    float ApplyEnterProgressToOpacity(float opacity, float enterProgress) const;
    float ResolveDrawItemStateOpacity(const CardModel &card, float enterProgress) const;
    float ResolveOutgoingTransitionOpacity(const DrawBuildContext &context) const;
    float ResolveIncomingTransitionOpacity(const DrawBuildContext &context) const;
    float ResolveCycleTransitionOpacity(const DrawBuildContext &context) const;
    float ResolveDrawItemTransitionOpacity(const VisibleCardStructure &entry, const DrawBuildContext &context) const;
    float UpdateDrawItemAlpha(const VisibleCardStructure &entry, const DrawBuildContext &context, const CardModel &card, const CardAnimationState &animationState, float enterProgress) const;
    bool TryBuildDrawItemForStructure(const VisibleCardStructure &entry, const DrawBuildContext &context, float enterProgress, DrawItem &item) const;
    std::vector<DrawItem> BuildDrawItems(float enterProgress) const;

    void Render();
    bool IsVisibleInView(size_t position, bool isVisibleInFinalView, size_t rotatingInPos, size_t rotatingOutPos) const;

    // Member variables
    HINSTANCE m_instance = nullptr;
    HWND m_hwnd = nullptr;
    UINT m_width = kInitialWidth;
    UINT m_height = kInitialHeight;
    bool m_minimized = false;

    // uDWM m_leWindows — doubly-linked list, head = frontmost card
    std::list<CardModel> m_cards;

    ViewState m_state = ViewState::Inactive;
    Timeline m_enterTimeline;
    Timeline m_rotateTimeline;
    bool m_rotateBackward = false;    // current rotation direction (back→front or front→back)
    HWND m_originalFrontHWND = nullptr;
    int m_rotationTargetIndex = -1;
    float m_rRepeatedRotateRate = 0.0f;
    int m_bufferedRotateDelta = 0;
    int m_cMouseWheelLeftOver = 0;
    int m_mousePressedCardIndex = -1;
    UINT m_vkLastRepeatingKey = 0;
    bool m_pendingWinKeyAction = false;
    bool m_showOutgoingDuringRotation = false;
    HWND m_selectedHWND = nullptr;
    bool m_selectedWindowWasMinimized = false;
    bool m_selectedWindowWasShellDesktop = false;
    bool m_selectedWindowActivationDispatched = false;
    bool m_fRTLMirror = false;
    float m_totalTime = 0.0f;
    std::chrono::steady_clock::time_point m_previousFrameTime = {};

    // D3D11 resources
    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;
    ComPtr<IDXGISwapChain1> m_swapChain;

    // DirectComposition resources (for transparency support)
    ComPtr<IDCompositionDevice> m_dcompDevice;
    ComPtr<IDCompositionTarget> m_dcompTarget;
    ComPtr<IDCompositionVisual> m_dcompVisual;

    ComPtr<ID3D11RenderTargetView> m_renderTargetView;
    ComPtr<ID3D11Texture2D> m_msaaRenderTarget;     // MSAA color buffer
    ComPtr<ID3D11RenderTargetView> m_msaaRTV;       // persistent MSAA RTV
    ComPtr<ID3D11Texture2D> m_depthStencilTexture;
    ComPtr<ID3D11DepthStencilView> m_depthStencilView;

    ComPtr<ID3D11VertexShader> m_backgroundVertexShader;
    ComPtr<ID3D11PixelShader> m_backgroundPixelShader;
    ComPtr<ID3D11VertexShader> m_cardVertexShader;
    ComPtr<ID3D11PixelShader> m_cardPixelShader;
    ComPtr<ID3D11InputLayout> m_inputLayout;
    ComPtr<ID3D11Buffer> m_vertexBuffer;
    ComPtr<ID3D11Buffer> m_indexBuffer;
    ComPtr<ID3D11Buffer> m_frameConstantsBuffer;
    ComPtr<ID3D11Buffer> m_objectConstantsBuffer;
    ComPtr<ID3D11RasterizerState> m_rasterizerState;
    ComPtr<ID3D11DepthStencilState> m_depthStencilState;
    ComPtr<ID3D11BlendState> m_blendState;

    // Window captures embedded in each CardModel (uDWM per-window representation)
    ComPtr<ID3D11SamplerState> m_cardSampler;

    // Cached inverse view matrix for 3D hit testing (uDWM m_matHitTestInverse)
    XMMATRIX m_matHitTestInverse = XMMatrixIdentity();
    int m_monitorWidth = 0;
    int m_monitorHeight = 0;

    D3D11_VIEWPORT m_viewport = {};
};
