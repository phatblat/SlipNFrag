//
//  AppDelegate.h
//  SlipNFrag-MacOS
//
//  Created by Heriberto Delgado on 5/29/19.
//  Copyright © 2019 Heriberto Delgado. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface AppDelegate : NSObject <NSApplicationDelegate>

@property (strong, nonatomic) NSWindowController* preferencesController;

-(IBAction)preferences:(NSMenuItem *)sender;

@end
