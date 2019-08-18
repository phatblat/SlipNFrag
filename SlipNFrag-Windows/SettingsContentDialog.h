#pragma once

#include "SettingsContentDialog.g.h"

namespace winrt::SlipNFrag_Windows::implementation
{
	struct SettingsContentDialog : SettingsContentDialogT<SettingsContentDialog>
	{
		bool is_loading = false;

		SettingsContentDialog();
		void Basedir_choose_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Windows::UI::Xaml::RoutedEventArgs const&);
		void Standard_quake_radio_Checked(winrt::Windows::Foundation::IInspectable const&, winrt::Windows::UI::Xaml::RoutedEventArgs const&);
		void Hipnotic_radio_Checked(winrt::Windows::Foundation::IInspectable const&, winrt::Windows::UI::Xaml::RoutedEventArgs const&);
		void Rogue_radio_Checked(winrt::Windows::Foundation::IInspectable const&, winrt::Windows::UI::Xaml::RoutedEventArgs const&);
		void Game_radio_Checked(winrt::Windows::Foundation::IInspectable const&, winrt::Windows::UI::Xaml::RoutedEventArgs const&);
		void Game_text_TextChanged(winrt::Windows::Foundation::IInspectable const&, winrt::Windows::UI::Xaml::Controls::TextChangedEventArgs const&);
		void Command_line_text_TextChanged(winrt::Windows::Foundation::IInspectable const&, winrt::Windows::UI::Xaml::Controls::TextChangedEventArgs const&);
		void SaveRadioButtons();
		void Fullscreen_check_Checked(winrt::Windows::Foundation::IInspectable const&, winrt::Windows::UI::Xaml::RoutedEventArgs const&);
		void Fullscreen_check_Unchecked(winrt::Windows::Foundation::IInspectable const&, winrt::Windows::UI::Xaml::RoutedEventArgs const&);
		void SaveFullScreen();
	};
}

namespace winrt::SlipNFrag_Windows::factory_implementation
{
	struct SettingsContentDialog : SettingsContentDialogT<SettingsContentDialog, implementation::SettingsContentDialog>
	{
	};
}
