#include <windows.h>

#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <chrono>
#include <cstdint>

namespace {

LRESULT CALLBACK WindowProcedure(const HWND window, const UINT message, const WPARAM wparam, const LPARAM lparam) noexcept {
    if (message == WM_CLOSE || message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    WNDCLASSW window_class{};
    window_class.lpfnWndProc = &WindowProcedure;
    window_class.hInstance = instance;
    window_class.lpszClassName = L"HardwareScope.FpsGameFixture";
    if (RegisterClassW(&window_class) == 0U && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return 1;
    const auto window = CreateWindowExW(
        0U,
        window_class.lpszClassName,
        L"HardwareScope FPS DirectX validation fixture",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        960,
        540,
        nullptr,
        nullptr,
        instance,
        nullptr);
    if (window == nullptr) return 2;
    DXGI_SWAP_CHAIN_DESC description{};
    description.BufferDesc.Width = 960U;
    description.BufferDesc.Height = 540U;
    description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.SampleDesc.Count = 1U;
    description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    description.BufferCount = 2U;
    description.OutputWindow = window;
    description.Windowed = TRUE;
    description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swap_chain;
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL feature_level{};
    if (FAILED(D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            0U,
            nullptr,
            0U,
            D3D11_SDK_VERSION,
            &description,
            swap_chain.ReleaseAndGetAddressOf(),
            device.ReleaseAndGetAddressOf(),
            &feature_level,
            context.ReleaseAndGetAddressOf()))) return 3;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer;
    if (FAILED(swap_chain->GetBuffer(0U, IID_PPV_ARGS(back_buffer.ReleaseAndGetAddressOf())))) return 4;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_target;
    if (FAILED(device->CreateRenderTargetView(back_buffer.Get(), nullptr, render_target.ReleaseAndGetAddressOf()))) return 5;

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
    MSG message{};
    std::uint32_t frame{};
    while (std::chrono::steady_clock::now() < deadline) {
        while (PeekMessageW(&message, nullptr, 0U, 0U, PM_REMOVE)) {
            if (message.message == WM_QUIT) return 0;
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        const auto phase = static_cast<float>(frame++ % 240U) / 240.0F;
        const float color[]{0.04F + phase * 0.10F, 0.18F, 0.22F + phase * 0.10F, 1.0F};
        context->ClearRenderTargetView(render_target.Get(), color);
        if (FAILED(swap_chain->Present(1U, 0U))) return 6;
    }
    DestroyWindow(window);
    return 0;
}
