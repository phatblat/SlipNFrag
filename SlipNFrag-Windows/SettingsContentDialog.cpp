#include "pch.h"
#include "SettingsContentDialog.h"
#include "SettingsContentDialog.g.cpp"
#include "quakedef.h"
#include "in_uwp.h"

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Gaming::Input;
using namespace Windows::Storage;
using namespace Windows::Storage::AccessCache;
using namespace Windows::Storage::Pickers;
using namespace Windows::System::Threading;
using namespace Windows::UI::Core;
using namespace Windows::UI::Popups;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Controls;

namespace winrt::SlipNFrag_Windows::implementation
{
	SettingsContentDialog::SettingsContentDialog()
	{
		InitializeComponent();
		is_loading = true;
		stickProgressBars.push_back(joy_axis_x_progress());
		stickProgressBars.push_back(joy_axis_y_progress());
		stickProgressBars.push_back(joy_axis_z_progress());
		stickProgressBars.push_back(joy_axis_r_progress());
		stickProgressBars.push_back(joy_axis_u_progress());
		stickProgressBars.push_back(joy_axis_v_progress());
		buttonProgressBars.push_back(k_joy1_progress());
		buttonProgressBars.push_back(k_joy2_progress());
		buttonProgressBars.push_back(k_joy3_progress());
		buttonProgressBars.push_back(k_joy4_progress());
		buttonProgressBars.push_back(k_aux1_progress());
		buttonProgressBars.push_back(k_aux2_progress());
		buttonProgressBars.push_back(k_aux3_progress());
		buttonProgressBars.push_back(k_aux4_progress());
		buttonProgressBars.push_back(k_aux5_progress());
		buttonProgressBars.push_back(k_aux6_progress());
		buttonProgressBars.push_back(k_aux7_progress());
		buttonProgressBars.push_back(k_aux8_progress());
		buttonProgressBars.push_back(k_aux9_progress());
		buttonProgressBars.push_back(k_aux10_progress());
		buttonProgressBars.push_back(k_aux11_progress());
		buttonProgressBars.push_back(k_aux12_progress());
		buttonProgressBars.push_back(k_aux13_progress());
		buttonProgressBars.push_back(k_aux14_progress());
		buttonProgressBars.push_back(k_aux15_progress());
		buttonProgressBars.push_back(k_aux16_progress());
		buttonProgressBars.push_back(k_aux17_progress());
		buttonProgressBars.push_back(k_aux18_progress());
		buttonProgressBars.push_back(k_aux19_progress());
		buttonProgressBars.push_back(k_aux20_progress());
		buttonProgressBars.push_back(k_aux21_progress());
		buttonProgressBars.push_back(k_aux22_progress());
		buttonProgressBars.push_back(k_aux23_progress());
		buttonProgressBars.push_back(k_aux24_progress());
		buttonProgressBars.push_back(k_aux25_progress());
		buttonProgressBars.push_back(k_aux26_progress());
		buttonProgressBars.push_back(k_aux27_progress());
		buttonProgressBars.push_back(k_aux28_progress());
		buttonProgressBars.push_back(k_aux29_progress());
		buttonProgressBars.push_back(k_aux30_progress());
		buttonProgressBars.push_back(k_aux31_progress());
		buttonProgressBars.push_back(k_aux32_progress());
		buttonTextBlocks.push_back(k_joy1_text());
		buttonTextBlocks.push_back(k_joy2_text());
		buttonTextBlocks.push_back(k_joy3_text());
		buttonTextBlocks.push_back(k_joy4_text());
		buttonTextBlocks.push_back(k_aux1_text());
		buttonTextBlocks.push_back(k_aux2_text());
		buttonTextBlocks.push_back(k_aux3_text());
		buttonTextBlocks.push_back(k_aux4_text());
		buttonTextBlocks.push_back(k_aux5_text());
		buttonTextBlocks.push_back(k_aux6_text());
		buttonTextBlocks.push_back(k_aux7_text());
		buttonTextBlocks.push_back(k_aux8_text());
		buttonTextBlocks.push_back(k_aux9_text());
		buttonTextBlocks.push_back(k_aux10_text());
		buttonTextBlocks.push_back(k_aux11_text());
		buttonTextBlocks.push_back(k_aux12_text());
		buttonTextBlocks.push_back(k_aux13_text());
		buttonTextBlocks.push_back(k_aux14_text());
		buttonTextBlocks.push_back(k_aux15_text());
		buttonTextBlocks.push_back(k_aux16_text());
		buttonTextBlocks.push_back(k_aux17_text());
		buttonTextBlocks.push_back(k_aux18_text());
		buttonTextBlocks.push_back(k_aux19_text());
		buttonTextBlocks.push_back(k_aux20_text());
		buttonTextBlocks.push_back(k_aux21_text());
		buttonTextBlocks.push_back(k_aux22_text());
		buttonTextBlocks.push_back(k_aux23_text());
		buttonTextBlocks.push_back(k_aux24_text());
		buttonTextBlocks.push_back(k_aux25_text());
		buttonTextBlocks.push_back(k_aux26_text());
		buttonTextBlocks.push_back(k_aux27_text());
		buttonTextBlocks.push_back(k_aux28_text());
		buttonTextBlocks.push_back(k_aux29_text());
		buttonTextBlocks.push_back(k_aux30_text());
		buttonTextBlocks.push_back(k_aux31_text());
		buttonTextBlocks.push_back(k_aux32_text());
		stickComboBoxes.push_back(joy_axis_x_combo());
		stickComboBoxes.push_back(joy_axis_y_combo());
		stickComboBoxes.push_back(joy_axis_z_combo());
		stickComboBoxes.push_back(joy_axis_r_combo());
		stickComboBoxes.push_back(joy_axis_u_combo());
		stickComboBoxes.push_back(joy_axis_v_combo());
		buttonComboBoxes.push_back(k_joy1_combo());
		buttonComboBoxes.push_back(k_joy2_combo());
		buttonComboBoxes.push_back(k_joy3_combo());
		buttonComboBoxes.push_back(k_joy4_combo());
		buttonComboBoxes.push_back(k_aux1_combo());
		buttonComboBoxes.push_back(k_aux2_combo());
		buttonComboBoxes.push_back(k_aux3_combo());
		buttonComboBoxes.push_back(k_aux4_combo());
		buttonComboBoxes.push_back(k_aux5_combo());
		buttonComboBoxes.push_back(k_aux6_combo());
		buttonComboBoxes.push_back(k_aux7_combo());
		buttonComboBoxes.push_back(k_aux8_combo());
		buttonComboBoxes.push_back(k_aux9_combo());
		buttonComboBoxes.push_back(k_aux10_combo());
		buttonComboBoxes.push_back(k_aux11_combo());
		buttonComboBoxes.push_back(k_aux12_combo());
		buttonComboBoxes.push_back(k_aux13_combo());
		buttonComboBoxes.push_back(k_aux14_combo());
		buttonComboBoxes.push_back(k_aux15_combo());
		buttonComboBoxes.push_back(k_aux16_combo());
		buttonComboBoxes.push_back(k_aux17_combo());
		buttonComboBoxes.push_back(k_aux18_combo());
		buttonComboBoxes.push_back(k_aux19_combo());
		buttonComboBoxes.push_back(k_aux20_combo());
		buttonComboBoxes.push_back(k_aux21_combo());
		buttonComboBoxes.push_back(k_aux22_combo());
		buttonComboBoxes.push_back(k_aux23_combo());
		buttonComboBoxes.push_back(k_aux24_combo());
		buttonComboBoxes.push_back(k_aux25_combo());
		buttonComboBoxes.push_back(k_aux26_combo());
		buttonComboBoxes.push_back(k_aux27_combo());
		buttonComboBoxes.push_back(k_aux28_combo());
		buttonComboBoxes.push_back(k_aux29_combo());
		buttonComboBoxes.push_back(k_aux30_combo());
		buttonComboBoxes.push_back(k_aux31_combo());
		buttonComboBoxes.push_back(k_aux32_combo());
		stickCheckBoxes.push_back(joy_axis_x_check());
		stickCheckBoxes.push_back(joy_axis_y_check());
		stickCheckBoxes.push_back(joy_axis_z_check());
		stickCheckBoxes.push_back(joy_axis_r_check());
		stickCheckBoxes.push_back(joy_axis_u_check());
		stickCheckBoxes.push_back(joy_axis_v_check());
		stickComboBoxItems.push_back(L"None");
		stickComboBoxItems.push_back(L"Forward / Backward");
		stickComboBoxItems.push_back(L"Look Up / Down");
		stickComboBoxItems.push_back(L"Step Left / Right");
		stickComboBoxItems.push_back(L"Turn Left / Right");
		buttonComboBoxItems.push_back(L"None");
		buttonComboBoxItems.push_back(L"Attack");
		buttonComboBoxItems.push_back(L"Change weapon");
		buttonComboBoxItems.push_back(L"Jump / Swim up");
		buttonComboBoxItems.push_back(L"Walk forward");
		buttonComboBoxItems.push_back(L"Backpedal");
		buttonComboBoxItems.push_back(L"Turn left");
		buttonComboBoxItems.push_back(L"Turn right");
		buttonComboBoxItems.push_back(L"Run");
		buttonComboBoxItems.push_back(L"Step left");
		buttonComboBoxItems.push_back(L"Step right");
		buttonComboBoxItems.push_back(L"Sidestep");
		buttonComboBoxItems.push_back(L"Look up");
		buttonComboBoxItems.push_back(L"Look down");
		buttonComboBoxItems.push_back(L"Center view");
		buttonComboBoxItems.push_back(L"Mouse look");
		buttonComboBoxItems.push_back(L"Keyboard look");
		buttonComboBoxItems.push_back(L"Swim up");
		buttonComboBoxItems.push_back(L"Swim down");
		buttonComboBoxItems.push_back(L"ENTER Key");
		buttonComboBoxItems.push_back(L"ESC Key");
		buttonComboBoxItems.push_back(L"Up Arrow");
		buttonComboBoxItems.push_back(L"Left Arrow");
		buttonComboBoxItems.push_back(L"Right Arrow");
		buttonComboBoxItems.push_back(L"Down Arrow");
		buttonComboBoxItems.push_back(L"impulse 1");
		buttonComboBoxItems.push_back(L"impulse 2");
		buttonComboBoxItems.push_back(L"impulse 3");
		buttonComboBoxItems.push_back(L"impulse 4");
		buttonComboBoxItems.push_back(L"impulse 5");
		buttonComboBoxItems.push_back(L"impulse 6");
		buttonComboBoxItems.push_back(L"impulse 7");
		buttonComboBoxItems.push_back(L"impulse 8");
		for (auto i = 1; i < (int)buttonComboBoxItems.size(); i++)
		{
			stickComboBoxItems.push_back(buttonComboBoxItems[i]);
		}
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
		if (values.HasKey(L"joystick_check"))
		{
			auto value = values.Lookup(L"joystick_check");
			joystick_check().IsChecked(unbox_value<bool>(value));
		}
		else
		{
			joystick_check().IsChecked(false);
		}
		oneIsChecked = false;
		if (values.HasKey(L"joy_standard_radio"))
		{
			auto value = values.Lookup(L"joy_standard_radio");
			joy_standard_radio().IsChecked(unbox_value<bool>(value));
			oneIsChecked = oneIsChecked || unbox_value<bool>(value);
		}
		if (values.HasKey(L"joy_advanced_radio"))
		{
			auto value = values.Lookup(L"joy_advanced_radio");
			joy_advanced_radio().IsChecked(unbox_value<bool>(value));
			oneIsChecked = oneIsChecked || unbox_value<bool>(value);
		}
		if (!oneIsChecked)
		{
			joy_standard_radio().IsChecked(true);
		}
		if (Gamepad::Gamepads().Size() > 0)
		{
			gamepad = Gamepad::Gamepads().GetAt(0);
			connected_controller_text().Text(L"Xbox One / 360 gamepad or compatible");
		}
		if (gamepad.get() == nullptr)
		{
			for (auto const& controller : RawGameController::RawGameControllers())
			{
				if (controller.AxisCount() >= 2 && controller.ButtonCount() >= 4)
				{
					joystick = controller;
					connected_controller_text().Text(joystick.get().DisplayName());
					break;
				}
			}
		}
		LoadTextBlocks();
		Gamepad::GamepadAdded([this](IInspectable const&, Gamepad const& e)
			{
				if (gamepad.get() == nullptr && joystick.get() == nullptr)
				{
					gamepad = e;
					connected_controller_text().Dispatcher().RunAsync(CoreDispatcherPriority::High, [this]()
						{
							connected_controller_text().Text(L"Xbox One / 360 gamepad or compatible");
							LoadTextBlocks();
						});
				}
			});
		Gamepad::GamepadRemoved([this](IInspectable const&, Gamepad const& e)
			{
				if (gamepad.get() != nullptr && gamepad.get() == e)
				{
					gamepad = nullptr;
					connected_controller_text().Dispatcher().RunAsync(CoreDispatcherPriority::High, [this]()
						{
							connected_controller_text().Text(L"(none)");
							LoadTextBlocks();
						});
				}
			});
		RawGameController::RawGameControllerAdded([this](IInspectable const&, RawGameController const& e)
			{
				if (gamepad.get() == nullptr && joystick.get() == nullptr && e.AxisCount() >= 2 && e.ButtonCount() >= 4)
				{
					joystick = e;
					connected_controller_text().Dispatcher().RunAsync(CoreDispatcherPriority::High, [this]()
						{
							connected_controller_text().Text(joystick.get().DisplayName());
							LoadTextBlocks();
						});
				}
			});
		RawGameController::RawGameControllerRemoved([this](IInspectable const&, RawGameController const& e)
			{
				if (joystick.get() != nullptr && joystick.get().NonRoamableId() == e.NonRoamableId())
				{
					joystick = nullptr;
					connected_controller_text().Dispatcher().RunAsync(CoreDispatcherPriority::High, [this]()
						{
							connected_controller_text().Text(L"(none)");
							LoadTextBlocks();
						});
				}
			});
		LoadStickSettings(values);
		for (auto i = 0; i < (int)buttonComboBoxes.size(); i++)
		{
			buttonComboBoxes[i].Items().Clear();
			for (auto j = 0; j < (int)buttonComboBoxItems.size(); j++)
			{
				buttonComboBoxes[i].Items().Append(box_value(buttonComboBoxItems[j]));
			}
			auto found = false;
			if (values.HasKey(buttonComboBoxes[i].Name()))
			{
				auto value = values.Lookup(buttonComboBoxes[i].Name());
				std::wstring text(unbox_value<hstring>(value));
				if (text.size() > 0)
				{
					try
					{
						auto index = std::stoi(text);
						if (index >= 0 && index < (int)buttonComboBoxItems.size())
						{
							buttonComboBoxes[i].SelectedIndex(index);
							found = true;
						}
					}
					catch (...)
					{
						// Do nothing. Value will be set to none.
					}
				}
			}
			if (!found)
			{
				buttonComboBoxes[i].SelectedIndex(0);
			}
		}
		for (auto i = 0; i < (int)stickComboBoxes.size(); i++)
		{
			stickComboBoxes[i].SelectionChanged([this](IInspectable const& source, SelectionChangedEventArgs const&) 
				{
					if (is_loading)
					{
						return;
					}
					auto values = ApplicationData::Current().LocalSettings().Values();
					auto comboBox = source.as<ComboBox>();
					auto index = comboBox.SelectedIndex();
					values.Insert(comboBox.Name(), box_value(to_hstring(index)));
				});
		}
		for (auto i = 0; i < (int)stickCheckBoxes.size(); i++)
		{
			stickCheckBoxes[i].Checked([this](IInspectable const& source, RoutedEventArgs const&)
				{
					SaveStickCheck(source);
				});
			stickCheckBoxes[i].Unchecked([this](IInspectable const& source, RoutedEventArgs const&)
				{
					SaveStickCheck(source);
				});
		}
		for (auto i = 0; i < (int)buttonComboBoxes.size(); i++)
		{
			buttonComboBoxes[i].SelectionChanged([this](IInspectable const& source, SelectionChangedEventArgs const&)
				{
					if (is_loading)
					{
						return;
					}
					auto values = ApplicationData::Current().LocalSettings().Values();
					auto comboBox = source.as<ComboBox>();
					auto index = comboBox.SelectedIndex();
					values.Insert(comboBox.Name(), box_value(to_hstring(index)));
				});
		}
		if (values.HasKey(L"mlook_check"))
		{
			auto value = values.Lookup(L"mlook_check");
			mlook_check().IsChecked(unbox_value<bool>(value));
		}
		else
		{
			mlook_check().IsChecked(false);
		}
		timer.Interval(std::chrono::milliseconds(100));
		timer.Tick([this](IInspectable const&, IInspectable const&) 
			{
				if (gamepad.get() != nullptr)
				{
					auto reading = gamepad.get().GetCurrentReading();
					joy_axis_x_progress().Value(reading.LeftThumbstickX * 50 + 50);
					joy_axis_y_progress().Value(reading.LeftThumbstickY * 50 + 50);
					joy_axis_z_progress().Value(reading.RightThumbstickX * 50 + 50);
					joy_axis_r_progress().Value(reading.RightThumbstickY * 50 + 50);
					joy_axis_u_progress().Value(reading.LeftTrigger * 50 + 50);
					joy_axis_v_progress().Value(reading.RightTrigger * 50 + 50);
					auto buttons = (unsigned int)reading.Buttons;
					unsigned int mask = 1;
					for (auto i = 0; i < 31; i++)
					{
						auto value = 0.0;
						if ((buttons & mask) == mask)
						{
							value = 100;
						}
						buttonProgressBars[i].Value(value);
						mask <<= 1;
					}
				}
				if (joystick.get() != nullptr)
				{
					auto buttonCount = joystick.get().ButtonCount();
					auto buttons = new bool[buttonCount];
					std::vector<GameControllerSwitchPosition> switches(joystick.get().SwitchCount());
					std::vector<double> axes(joystick.get().AxisCount());
					joystick.get().GetCurrentReading(array_view(buttons, buttons + joystick.get().ButtonCount()), switches, axes);
					for (auto i = 0; i < (int)stickProgressBars.size(); i++)
					{
						if (axes.size() > i)
						{
							stickProgressBars[i].Value(axes[i] * 100);
						}
						else
						{
							stickProgressBars[i].Value(0);
						}
					}
					for (auto i = 0; i < (int)buttonProgressBars.size(); i++)
					{
						if (buttonCount > i && buttons[i])
						{
							buttonProgressBars[i].Value(100);
						}
						else
						{
							buttonProgressBars[i].Value(0);
						}
					}
				}
				if (gamepad.get() == nullptr && joystick.get() == nullptr)
				{
					for (auto& progressBar : stickProgressBars)
					{
						progressBar.Value(0);
					}
					for (auto& progressBar : buttonProgressBars)
					{
						progressBar.Value(0);
					}
				}
			});
		timer.Start();
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
					task.Completed([=](IAsyncOperation<StorageFile> const&, AsyncStatus const& status)
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
		SaveGeneralRadioButtons();
	}

	void SettingsContentDialog::Hipnotic_radio_Checked(IInspectable const&, RoutedEventArgs const&)
	{
		SaveGeneralRadioButtons();
	}

	void SettingsContentDialog::Rogue_radio_Checked(IInspectable const&, RoutedEventArgs const&)
	{
		SaveGeneralRadioButtons();
	}

	void SettingsContentDialog::Game_radio_Checked(IInspectable const&, RoutedEventArgs const&)
	{
		SaveGeneralRadioButtons();
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

	void SettingsContentDialog::SaveGeneralRadioButtons()
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
		SaveFullScreenCheck();
	}

	void SettingsContentDialog::Fullscreen_check_Unchecked(IInspectable const&, RoutedEventArgs const&)
	{
		SaveFullScreenCheck();
	}

	void SettingsContentDialog::SaveFullScreenCheck()
	{
		if (is_loading)
		{
			return;
		}
		auto values = ApplicationData::Current().LocalSettings().Values();
		values.Insert(L"fullscreen_check", fullscreen_check().IsChecked());
	}

	void SettingsContentDialog::General_button_Click(IInspectable const&, RoutedEventArgs const&)
	{
		general_button().IsEnabled(false);
		general_tab().Visibility(Visibility::Visible);
		joystick_button().IsEnabled(true);
		joystick_tab().Visibility(Visibility::Collapsed);
	}

	void SettingsContentDialog::Joystick_button_Click(IInspectable const&, RoutedEventArgs const&)
	{
		general_button().IsEnabled(true);
		general_tab().Visibility(Visibility::Collapsed);
		joystick_button().IsEnabled(false);
		joystick_tab().Visibility(Visibility::Visible);
	}

	void SettingsContentDialog::ContentDialog_Closing(ContentDialog const&, ContentDialogClosingEventArgs const&)
	{
		timer.Stop();
	}

	void SettingsContentDialog::LoadStickSettings(IPropertySet& values)
	{
		for (auto i = 0; i < (int)stickComboBoxes.size(); i++)
		{
			stickComboBoxes[i].Items().Clear();
			for (auto j = 0; j < (int)stickComboBoxItems.size(); j++)
			{
				stickComboBoxes[i].Items().Append(box_value(stickComboBoxItems[j]));
			}
		}
		if (joy_standard_radio().IsChecked().Value())
		{
			for (auto i = 0; i < (int)stickComboBoxes.size(); i++)
			{
				if (i == 0)
				{
					stickComboBoxes[i].SelectedIndex(4); // Turn Left / Right
				}
				else if (i == 1)
				{
					stickComboBoxes[i].SelectedIndex(1); // Forward / Backward
				}
				else if (i == 4)
				{
					stickComboBoxes[i].SelectedIndex(5); // Attack
				}
				else if (i == 5)
				{
					stickComboBoxes[i].SelectedIndex(5); // Attack
				}
				else
				{
					stickComboBoxes[i].SelectedIndex(0);
				}
				stickComboBoxes[i].IsEnabled(false);
			}
			for (auto i = 0; i < (int)stickCheckBoxes.size(); i++)
			{
				stickCheckBoxes[i].IsChecked(false);
				stickCheckBoxes[i].IsEnabled(false);
			}
			load_default_axes_button().IsEnabled(false);
		}
		else
		{
			for (auto i = 0; i < (int)stickComboBoxes.size(); i++)
			{
				auto found = false;
				if (values.HasKey(stickComboBoxes[i].Name()))
				{
					auto value = values.Lookup(stickComboBoxes[i].Name());
					std::wstring text(unbox_value<hstring>(value));
					if (text.size() > 0)
					{
						try
						{
							auto index = std::stoi(text);
							if (index >= 0 && index < (int)stickComboBoxItems.size())
							{
								stickComboBoxes[i].SelectedIndex(index);
								found = true;
							}
						}
						catch (...)
						{
							// Do nothing. Value will be set to none.
						}
					}
				}
				if (!found)
				{
					stickComboBoxes[i].SelectedIndex(0);
				}
				stickComboBoxes[i].IsEnabled(true);
			}
			for (auto i = 0; i < (int)stickCheckBoxes.size(); i++)
			{
				if (values.HasKey(stickCheckBoxes[i].Name()))
				{
					auto value = values.Lookup(stickCheckBoxes[i].Name());
					stickCheckBoxes[i].IsChecked(unbox_value<bool>(value));
				}
				else
				{
					stickCheckBoxes[i].IsChecked(false);
				}
				stickCheckBoxes[i].IsEnabled(true);
			}
			load_default_axes_button().IsEnabled(true);
		}
	}

	void SettingsContentDialog::Joy_standard_radio_Checked(IInspectable const&, RoutedEventArgs const&)
	{
		auto values = ApplicationData::Current().LocalSettings().Values();
		SaveJoystickRadioButtons(values);
		is_loading = true;
		LoadStickSettings(values);
		is_loading = false;
	}

	void SettingsContentDialog::Joy_advanced_radio_Checked(IInspectable const&, RoutedEventArgs const&)
	{
		auto values = ApplicationData::Current().LocalSettings().Values();
		SaveJoystickRadioButtons(values);
		is_loading = true;
		LoadStickSettings(values);
		is_loading = false;
	}

	void SettingsContentDialog::SaveJoystickRadioButtons(IPropertySet& values)
	{
		if (is_loading)
		{
			return;
		}
		values.Insert(L"joy_standard_radio", joy_standard_radio().IsChecked());
		values.Insert(L"joy_advanced_radio", joy_advanced_radio().IsChecked());
	}

	void SettingsContentDialog::Joystick_check_Checked(IInspectable const&, RoutedEventArgs const&)
	{
		SaveJoystickCheck();
	}

	void SettingsContentDialog::Joystick_check_Unchecked(IInspectable const&, RoutedEventArgs const&)
	{
		SaveJoystickCheck();
	}

	void SettingsContentDialog::SaveJoystickCheck()
	{
		if (is_loading)
		{
			return;
		}
		auto values = ApplicationData::Current().LocalSettings().Values();
		values.Insert(L"joystick_check", joystick_check().IsChecked());
	}

	void SettingsContentDialog::SaveStickCheck(IInspectable const& source)
	{
		if (is_loading)
		{
			return;
		}
		auto values = ApplicationData::Current().LocalSettings().Values();
		auto checkbox = source.as<CheckBox>();
		values.Insert(checkbox.Name(), checkbox.IsChecked());
	}

	void SettingsContentDialog::LoadTextBlocks()
	{
		if (gamepad.get() != nullptr)
		{
			joy_axis_x_text().Text(L"Left Thumbstick X");
			joy_axis_y_text().Text(L"Left Thumbstick Y");
			joy_axis_z_text().Text(L"Right Thumbstick X");
			joy_axis_r_text().Text(L"Right Thumbstick Y");
			joy_axis_u_text().Text(L"Left Trigger");
			joy_axis_v_text().Text(L"Right Trigger");
			k_joy1_text().Text(L"Menu");
			k_joy2_text().Text(L"View");
			k_joy3_text().Text(L"A");
			k_joy4_text().Text(L"B");
			k_aux1_text().Text(L"X");
			k_aux2_text().Text(L"Y");
			k_aux3_text().Text(L"D-pad Up");
			k_aux4_text().Text(L"D-pad Down");
			k_aux5_text().Text(L"D-pad Left");
			k_aux6_text().Text(L"D-pad Right");
			k_aux7_text().Text(L"Left Shoulder");
			k_aux8_text().Text(L"Right Shoulder");
			k_aux9_text().Text(L"Left Thumbstick");
			k_aux10_text().Text(L"Right Thumbstick");
			k_aux11_text().Text(L"Paddle 1");
			k_aux12_text().Text(L"Paddle 2");
			k_aux13_text().Text(L"Paddle 3");
			k_aux14_text().Text(L"Paddle 4");
			for (auto i = 18; i < buttonTextBlocks.size(); i++)
			{
				buttonTextBlocks[i].Text(L"K_AUX" + to_hstring(i - 3));
			}
		}
		else
		{
			joy_axis_x_text().Text(L"JOY_AXIS_X");
			joy_axis_y_text().Text(L"JOY_AXIS_Y");
			joy_axis_z_text().Text(L"JOY_AXIS_Z");
			joy_axis_r_text().Text(L"JOY_AXIS_R");
			joy_axis_u_text().Text(L"JOY_AXIS_U");
			joy_axis_v_text().Text(L"JOY_AXIS_V");
			for (auto i = 0; i < buttonTextBlocks.size(); i++)
			{
				if (i < 4)
				{
					buttonTextBlocks[i].Text(L"K_JOY" + to_hstring(i + 1));
				}
				else
				{
					buttonTextBlocks[i].Text(L"K_AUX" + to_hstring(i - 3));
				}
			}
		}
	}

	void SettingsContentDialog::Load_default_axes_button_Click(IInspectable const&, RoutedEventArgs const&)
	{
		joy_axis_x_combo().SelectedIndex(3); // Step Left / Right
		joy_axis_x_check().IsChecked(true); // Inverted step
		joy_axis_y_combo().SelectedIndex(1); // Forward / Backward
		joy_axis_y_check().IsChecked(false);
		joy_axis_z_combo().SelectedIndex(4); // Turn Left / Right
		joy_axis_z_check().IsChecked(false); 
		joy_axis_r_combo().SelectedIndex(2); // Look Up / Down
		joy_axis_r_check().IsChecked(true); // Inverted look
		joy_axis_u_combo().SelectedIndex(5); // Attack
		joy_axis_u_check().IsChecked(false);
		joy_axis_v_combo().SelectedIndex(5); // Attack
		joy_axis_v_check().IsChecked(false);
	}

	void SettingsContentDialog::Load_default_buttons_button_Click(IInspectable const&, RoutedEventArgs const&)
	{
		k_joy1_combo().SelectedIndex(20); // ESC Key
		k_joy2_combo().SelectedIndex(0);
		k_joy3_combo().SelectedIndex(3); // Jump / Swim up
		k_joy4_combo().SelectedIndex(18); // Swim down
		k_aux1_combo().SelectedIndex(8); // Run
		k_aux2_combo().SelectedIndex(2); // Change weapon
		k_aux3_combo().SelectedIndex(26); // impulse 2
		k_aux4_combo().SelectedIndex(28); // impulse 4
		k_aux5_combo().SelectedIndex(25); // impulse 1 
		k_aux6_combo().SelectedIndex(27); // impulse 3
		k_aux7_combo().SelectedIndex(2); // Change weapon
		k_aux8_combo().SelectedIndex(3); // Jump / Swim down 
		k_aux9_combo().SelectedIndex(8); // Run
		k_aux10_combo().SelectedIndex(15); // Mouse look
		for (auto i = 14; i < (int)buttonComboBoxes.size(); i++)
		{
			buttonComboBoxes[i].SelectedIndex(0);
		}
		mlook_check().IsChecked(false);
	}

	void SettingsContentDialog::Mlook_check_Checked(IInspectable const&, RoutedEventArgs const&)
	{
		SaveMouseLookCheck();
	}

	void SettingsContentDialog::Mlook_check_Unchecked(IInspectable const&, RoutedEventArgs const&)
	{
		SaveMouseLookCheck();
	}

	void SettingsContentDialog::SaveMouseLookCheck()
	{
		if (is_loading)
		{
			return;
		}
		auto values = ApplicationData::Current().LocalSettings().Values();
		values.Insert(L"mlook_check", mlook_check().IsChecked());
	}
}
