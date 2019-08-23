﻿#include "pch.h"
#include "SettingsContentDialog.h"
#include "SettingsContentDialog.g.cpp"

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Storage;
using namespace Windows::Storage::AccessCache;
using namespace Windows::Storage::Pickers;
using namespace Windows::UI::Core;
using namespace Windows::UI::Popups;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;

namespace winrt::SlipNFrag_Windows::implementation
{
	SettingsContentDialog::SettingsContentDialog()
	{
		InitializeComponent();
		is_loading = true;
		if (StorageApplicationPermissions::FutureAccessList().ContainsItem(L"basedir_text"))
		{
			auto task = StorageApplicationPermissions::FutureAccessList().GetFolderAsync(L"basedir_text");
			task.Completed([this](IAsyncOperation<StorageFolder> const& operation, AsyncStatus const status)
				{
					if (status == AsyncStatus::Error)
					{
						return;
					}
					auto folder = operation.GetResults();
					auto path = folder.Path();
					basedir_text().Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [=]()
						{
							basedir_text().Text(path);
						});
				});
		}
		auto oneIsChecked = false;
		auto values = ApplicationData::Current().LocalSettings().Values();
		if (values.HasKey(L"standard_quake_radio"))
		{
			auto value = values.Lookup(L"standard_quake_radio");
			standard_quake_radio().IsChecked(unbox_value<bool>(value));
			oneIsChecked = oneIsChecked || unbox_value<bool>(value);
		}
		if (values.HasKey(L"hipnotic_radio"))
		{
			auto value = values.Lookup(L"hipnotic_radio");
			hipnotic_radio().IsChecked(unbox_value<bool>(value));
			oneIsChecked = oneIsChecked || unbox_value<bool>(value);
		}
		if (values.HasKey(L"rogue_radio"))
		{
			auto value = values.Lookup(L"rogue_radio");
			rogue_radio().IsChecked(unbox_value<bool>(value));
			oneIsChecked = oneIsChecked || unbox_value<bool>(value);
		}
		if (values.HasKey(L"game_radio"))
		{
			auto value = values.Lookup(L"game_radio");
			game_radio().IsChecked(unbox_value<bool>(value));
			oneIsChecked = oneIsChecked || unbox_value<bool>(value);
		}
		if (values.HasKey(L"game_text"))
		{
			auto value = values.Lookup(L"game_text");
			game_text().Text(unbox_value<hstring>(value));
		}
		if (values.HasKey(L"command_line_text"))
		{
			auto value = values.Lookup(L"command_line_text");
			command_line_text().Text(unbox_value<hstring>(value));
		}
		if (!oneIsChecked)
		{
			standard_quake_radio().IsChecked(true);
		}
		if (values.HasKey(L"fullscreen_check"))
		{
			auto value = values.Lookup(L"fullscreen_check");
			fullscreen_check().IsChecked(unbox_value<bool>(value));
		}
		else
		{
			fullscreen_check().IsChecked(true);
		}
		is_loading = false;
	}

	void SettingsContentDialog::Basedir_choose_Click(IInspectable const&, RoutedEventArgs const&)
	{
		FolderPicker picker;
		picker.SuggestedStartLocation(Windows::Storage::Pickers::PickerLocationId::ComputerFolder);
		picker.FileTypeFilter().Append(L"*");
		auto task = picker.PickSingleFolderAsync();
		task.Completed([=](IAsyncOperation<StorageFolder> const& operation, AsyncStatus const)
			{
				auto folder = operation.GetResults();
				if (folder != nullptr)
				{
					StorageApplicationPermissions::FutureAccessList().AddOrReplace(L"basedir_text", folder);
					auto path = folder.Path();
					basedir_text().Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [=]()
						{
							basedir_text().Text(path);
						});
					auto task = folder.GetFileAsync(L"ID1\\PAK0.PAK");
					task.Completed([=](IAsyncOperation<StorageFile> const& operation, AsyncStatus const& status)
						{
							if (status == AsyncStatus::Error)
							{
								basedir_text().Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [=]()
									{
										MessageDialog msg(L"The folder " + path + L" does not contain a folder ID1 with a file PAK0.PAK - the game might not start.\n\nEnsure that all required files are present before starting the game.", L"Core game data not found");
										msg.ShowAsync();
									});
							}
						});
				}
			});
	}

	void SettingsContentDialog::Standard_quake_radio_Checked(IInspectable const&, RoutedEventArgs const&)
	{
		SaveRadioButtons();
	}

	void SettingsContentDialog::Hipnotic_radio_Checked(IInspectable const&, RoutedEventArgs const&)
	{
		SaveRadioButtons();
	}

	void SettingsContentDialog::Rogue_radio_Checked(IInspectable const&, RoutedEventArgs const&)
	{
		SaveRadioButtons();
	}

	void SettingsContentDialog::Game_radio_Checked(IInspectable const&, RoutedEventArgs const&)
	{
		SaveRadioButtons();
	}

	void SettingsContentDialog::Game_text_TextChanged(IInspectable const&, TextChangedEventArgs const&)
	{
		if (is_loading)
		{
			return;
		}
		auto values = ApplicationData::Current().LocalSettings().Values();
		values.Insert(L"game_text", box_value(game_text().Text()));
	}

	void SettingsContentDialog::Command_line_text_TextChanged(IInspectable const&, TextChangedEventArgs const&)
	{
		if (is_loading)
		{
			return;
		}
		auto values = ApplicationData::Current().LocalSettings().Values();
		values.Insert(L"command_line_text", box_value(command_line_text().Text()));
	}

	void SettingsContentDialog::SaveRadioButtons()
	{
		if (is_loading)
		{
			return;
		}
		auto values = ApplicationData::Current().LocalSettings().Values();
		values.Insert(L"standard_quake_radio", standard_quake_radio().IsChecked());
		values.Insert(L"hipnotic_radio", hipnotic_radio().IsChecked());
		values.Insert(L"rogue_radio", rogue_radio().IsChecked());
		values.Insert(L"game_radio", game_radio().IsChecked());
	}

	void SettingsContentDialog::Fullscreen_check_Checked(IInspectable const&, RoutedEventArgs const&)
	{
		SaveFullScreen();
	}

	void SettingsContentDialog::Fullscreen_check_Unchecked(IInspectable const&, RoutedEventArgs const&)
	{
		SaveFullScreen();
	}

	void SettingsContentDialog::SaveFullScreen()
	{
		if (is_loading)
		{
			return;
		}
		auto values = ApplicationData::Current().LocalSettings().Values();
		values.Insert(L"fullscreen_check", fullscreen_check().IsChecked());
	}
}