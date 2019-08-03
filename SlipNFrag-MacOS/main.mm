//
//  main.m
//  SlipNFrag-MacOS
//
//  Created by Heriberto Delgado on 5/29/19.
//  Copyright © 2019 Heriberto Delgado. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#include "sys_macos.h"

int main(int argc, const char * argv[]) {
    sys_argc = argc;
    sys_argv = (char**)argv;
    return NSApplicationMain(argc, argv);
}
