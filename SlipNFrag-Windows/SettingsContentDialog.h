﻿#pragma once

#include "SettingsContentDialog.g.h"

namespace winrt::SlipNFrag_Windows::implementation
{
	struct SettingsContentDialog : SettingsContentDialogT<SettingsContentDialog>
	{
		bool is_loading = false;
		winrt::Windows::Gaming::Input::RawGameController joystick = nullptr;
		winrt::Windows::Gaming::Input::Gamepad gamepad = nullptr;
		winrt::Windows::UI::Xaml::DispatcherTimer timer;
		std::vector<winrt::Windows::UI::Xaml::Controls::ProgressBar> stickProgressBars;
		std::vector<winrt::Windows::UI::Xaml::Controls::ProgressBar> buttonProgressBars;
		std::vector<winrt::Windows::UI::Xaml::Controls::ComboBox> stickComboBoxes;
		std::vector<winrt::Windows::UI::Xaml::Controls::ComboBox> buttonComboBoxes;
		std::vector<winrt::hstring> stickComboBoxItems;

		SettingsContentDialog();
		void Basedir_choose_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Windows::UI::Xaml::RoutedEventArgs const&);
		void Standard_quake_radio_Checked(winrt::Windows::Foundation::IInspectable const&, winrt::Windows::UI::Xaml::RoutedEventArgs const&);
		void Hipnotic_radio_Checked(winrt::Windows::Foundation::IInspectable const&, winrt::Windows::UI::Xaml::RoutedEventArgs const&);
		void Rogue_radio_Checked(winrt::Windows::Foundation::IInspectable const&, winrt::Windows::UI::Xaml::RoutedEventArgs const&);
		void Game_radio_Checked(winrt::Windows::Foundation::IInspectable const&, winrt::Windows::UI::Xaml::RoutedEventArgs const&);
		void Game_text_TextChanged(winrt::Windows::Foundation::IInspectable const&, winrt::Windows::UI::Xaml::Controls::TextChangedEventArgs const&);
		void Command_line_text_TextChanged(winrt::Windows::Foundation::IInspectable const&, winrt::Windows::UI::Xaml::Controls::TextChangedEventArgs const&);
		void SaveGeneralRadioButtons();
		void Fullscreen_check_Checked(winrt::Windows::Foundation::IInspectable const&, winrt::Windows::UI::Xaml::RoutedEventArgs const&);
		void Fullscreen_check_Unchecked(winrt::Windows::Foundation::IInspectable const&, winrt::Windows::UI::Xaml::RoutedEventArgs const&);
		void SaveFullScreenCheck();
		void General_button_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Windows::UI::Xaml::RoutedEventArgs const&);
		void Joystick_button_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Windows::UI::Xaml::RoutedEventArgs const&);
		void ContentDialog_Closing(winrt::Windows::UI::Xaml::Controls::ContentDialog const&, winrt::Windows::UI::Xaml::Controls::ContentDialogClosingEventArgs const&);
		void LoadButtonComboBoxes(winrt::Windows::Foundation::Collections::IPropertySet& values);
		void Joy_standard_radio_Checked(winrt::Windows::Foundation::IInspectable const&, winrt::Windows::UI::Xaml::RoutedEventArgs const&);
		void Joy_advanced_radio_Checked(winrt::Windows::Foundation::IInspectable const&, winrt::Windows::UI::Xaml::RoutedEventArgs const&);
		void SaveJoystickRadioButtons(winrt::Windows::Foundation::Collections::IPropertySet& values);
		void Joystick_check_Checked(winrt::Windows::Foundation::IInspectable const&, winrt::Windows::UI::Xaml::RoutedEventArgs const&);
		void Joystick_check_Unchecked(winrt::Windows::Foundation::IInspectable const&, winrt::Windows::UI::Xaml::RoutedEventArgs const&);
		void SaveJoystickCheck();
	};
}

namespace winrt::SlipNFrag_Windows::factory_implementation
{
	struct SettingsContentDialog : SettingsContentDialogT<SettingsContentDialog, implementation::SettingsContentDialog>
	{
	};
}
