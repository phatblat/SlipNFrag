#include "pch.h"
#include "App.h"
#include "MainPage.h"

using namespace winrt;
using namespace SlipNFrag_Windows::implementation;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::Foundation;
using namespace Windows::UI::ViewManagement;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Navigation;

App::App()
{
    InitializeComponent();

#if defined _DEBUG && !defined DISABLE_XAML_GENERATED_BREAK_ON_UNHANDLED_EXCEPTION
    UnhandledException([this](IInspectable const&, UnhandledExceptionEventArgs const& e)
    {
        if (IsDebuggerPresent())
        {
            auto errorMessage = e.Message();
            __debugbreak();
        }
    });
#endif
}

void App::OnLaunched(LaunchActivatedEventArgs const& e)
{
    Frame rootFrame{ nullptr };
    auto content = Window::Current().Content();
    if (content)
    {
        rootFrame = content.try_as<Frame>();
    }
    if (rootFrame == nullptr)
    {
        rootFrame = Frame();
        rootFrame.NavigationFailed({ this, &App::OnNavigationFailed });

        if (e.PrelaunchActivated() == false)
        {
            if (rootFrame.Content() == nullptr)
            {
                rootFrame.Navigate(xaml_typename<SlipNFrag_Windows::MainPage>(), box_value(e.Arguments()));
            }
            Window::Current().Content(rootFrame);
            Window::Current().Activate();
			ApplicationView::GetForCurrentView().SetPreferredMinSize(Size(320, 200));
        }
    }
    else
    {
        if (e.PrelaunchActivated() == false)
        {
            if (rootFrame.Content() == nullptr)
            {
                rootFrame.Navigate(xaml_typename<SlipNFrag_Windows::MainPage>(), box_value(e.Arguments()));
            }
            Window::Current().Activate();
        }
    }
}

void App::OnNavigationFailed(IInspectable const&, NavigationFailedEventArgs const& e)
{
    throw hresult_error(E_FAIL, hstring(L"Slip & Frag was unable to navigate to the page ") + e.SourcePageType().Name);
}