//
//  PreferencesController.m
//  SlipNFrag-MacOS
//
//  Created by Heriberto Delgado on 6/29/19.
//  Copyright © 2019 Heriberto Delgado. All rights reserved.
//

#import "PreferencesController.h"
#import "AppDelegate.h"

@interface PreferencesController () <NSTextFieldDelegate>

@property (weak) IBOutlet NSTextField *basedir_label;
@property (weak) IBOutlet NSTextField *basedir_text;
@property (weak) IBOutlet NSButton *basedir_choose;
@property (weak) IBOutlet NSButton *standard_quake_radio;
@property (weak) IBOutlet NSButton *hipnotic_radio;
@property (weak) IBOutlet NSButton *rogue_radio;
@property (weak) IBOutlet NSButton *game_radio;
@property (weak) IBOutlet NSTextField *game_text;
@property (weak) IBOutlet NSTextField *command_line_text;

@end

@implementation PreferencesController

-(void)viewDidLoad
{
    [super viewDidLoad];
    self.basedir_text.delegate = self;
    self.game_text.delegate = self;
    self.command_line_text.delegate = self;
    [self loadStringUserDefault:@"basedir_text" intoTextField:self.basedir_text];
    [self loadBooleanUserDefault:@"standard_quake_radio" intoButton:self.standard_quake_radio];
    [self loadBooleanUserDefault:@"hipnotic_radio" intoButton:self.hipnotic_radio];
    [self loadBooleanUserDefault:@"rogue_radio" intoButton:self.rogue_radio];
    [self loadBooleanUserDefault:@"game_radio" intoButton:self.game_radio];
    [self loadStringUserDefault:@"game_text" intoTextField:self.game_text];
    [self loadStringUserDefault:@"command_line_text" intoTextField:self.command_line_text];
    [self checkCommandLine];
    if (self.standard_quake_radio.state == NSControlStateValueOff && self.hipnotic_radio.state == NSControlStateValueOff && self.rogue_radio.state == NSControlStateValueOff && self.game_radio.state == NSControlStateValueOff)
    {
        self.standard_quake_radio.state = NSControlStateValueOn;
    }
}

-(void)viewWillDisappear
{
    [super viewWillDisappear];
    AppDelegate* appDelegate = NSApplication.sharedApplication.delegate;
    appDelegate.preferencesController = nil;
}

-(void)controlTextDidChange:(NSNotification *)notification
{
    NSTextField* textField = notification.object;
    if (textField == self.basedir_text)
    {
        [self storeStringUserDefault:@"basedir_text" fromTextField:self.basedir_text];
    }
    else if (textField == self.game_text)
    {
        [self storeStringUserDefault:@"game_text" fromTextField:self.game_text];
    }
    else if (textField == self.command_line_text)
    {
        [self storeStringUserDefault:@"command_line_text" fromTextField:self.command_line_text];
    }
    [self checkCommandLine];
}

-(IBAction)chooseMissionPack:(NSButton *)sender
{
    [self storeBooleanUserDefault:@"standard_quake_radio" fromButton:sender ifMatchingButton:self.standard_quake_radio];
    [self storeBooleanUserDefault:@"hipnotic_radio" fromButton:sender ifMatchingButton:self.hipnotic_radio];
    [self storeBooleanUserDefault:@"rogue_radio" fromButton:sender ifMatchingButton:self.rogue_radio];
    [self storeBooleanUserDefault:@"game_radio" fromButton:sender ifMatchingButton:self.game_radio];
}

- (IBAction)chooseBaseDir:(NSButton *)sender
{
    NSOpenPanel* openPanel = [NSOpenPanel new];
    openPanel.canChooseFiles = NO;
    openPanel.canChooseDirectories = YES;
    if ([openPanel runModal] == NSModalResponseOK)
    {
        self.basedir_text.stringValue = openPanel.directoryURL.path;
        [self storeStringUserDefault:@"basedir_text" fromTextField:self.basedir_text];
    }
}

- (IBAction)resetAllSettings:(NSButton *)sender
{
    NSAlert* alert = [NSAlert new];
    [alert setMessageText:@"Reset All Preferences"];
    [alert setInformativeText:@"Are you sure you want to reset all preferences? This will delete all your configuration for Slip & Frag from your system."];
    [alert addButtonWithTitle:@"Yes"];
    [alert addButtonWithTitle:@"No"];
    [alert setAlertStyle:NSAlertStyleWarning];
    [alert beginSheetModalForWindow:self.view.window completionHandler:^(NSModalResponse returnCode)
    {
        if (returnCode == NSAlertSecondButtonReturn)
        {
            return;
        }
        [NSUserDefaults.standardUserDefaults removeObjectForKey:@"basedir_text"];
        [NSUserDefaults.standardUserDefaults removeObjectForKey:@"standard_quake_radio"];
        [NSUserDefaults.standardUserDefaults removeObjectForKey:@"hipnotic_radio"];
        [NSUserDefaults.standardUserDefaults removeObjectForKey:@"rogue_radio"];
        [NSUserDefaults.standardUserDefaults removeObjectForKey:@"game_radio"];
        [NSUserDefaults.standardUserDefaults removeObjectForKey:@"game_text"];
        [NSUserDefaults.standardUserDefaults removeObjectForKey:@"command_line_text"];
        [self.view.window close];
    }];
}

-(void)checkCommandLine
{
    NSString* command_line = self.command_line_text.stringValue;
    if (command_line == nil || [command_line isEqualToString:@""])
    {
        if (!self.basedir_text.isEnabled)
        {
            self.basedir_label.textColor = NSColor.textColor;
            self.basedir_text.enabled = YES;
            self.basedir_choose.enabled = YES;
            self.standard_quake_radio.enabled = YES;
            self.hipnotic_radio.enabled = YES;
            self.rogue_radio.enabled = YES;
            self.game_radio.enabled = YES;
            self.game_text.enabled = YES;
        }
    }
    else if (self.basedir_text.isEnabled)
    {
        self.basedir_label.textColor = NSColor.disabledControlTextColor;
        self.basedir_text.enabled = NO;
        self.basedir_choose.enabled = NO;
        self.standard_quake_radio.enabled = NO;
        self.hipnotic_radio.enabled = NO;
        self.rogue_radio.enabled = NO;
        self.game_radio.enabled = NO;
        self.game_text.enabled = NO;
    }
}

-(void)loadStringUserDefault:(NSString*)key intoTextField:(NSTextField*)textField
{
    NSString* value = [NSUserDefaults.standardUserDefaults stringForKey:key];
    if (value == nil)
    {
        value = @"";
    }
    textField.stringValue = value;
}

-(void)loadBooleanUserDefault:(NSString*)key intoButton:(NSButton*)button
{
    BOOL value = [NSUserDefaults.standardUserDefaults boolForKey:key];
    if (value)
    {
        button.state = NSControlStateValueOn;
    }
    else
    {
        button.state = NSControlStateValueOff;
    }
}

-(void)storeStringUserDefault:(NSString*)key fromTextField:(NSTextField*)textField
{
    NSString* value = textField.stringValue;
    if (value == nil || [value isEqualToString:@""])
    {
        [NSUserDefaults.standardUserDefaults removeObjectForKey:key];
    }
    else
    {
        [NSUserDefaults.standardUserDefaults setObject:value forKey:key];
    }
}

-(void)storeBooleanUserDefault:(NSString*)key fromButton:(NSButton*)button ifMatchingButton:(NSButton*)matchingButton
{
    if (button == matchingButton && button.state == NSControlStateValueOn)
    {
        [NSUserDefaults.standardUserDefaults setObject:@YES forKey:key];
    }
    else
    {
        [NSUserDefaults.standardUserDefaults removeObjectForKey:key];
    }
}

@end
