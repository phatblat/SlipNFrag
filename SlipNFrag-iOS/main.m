//
//  main.m
//  SlipNFrag-iOS
//
//  Created by Heriberto Delgado on 9/13/20.
//  Copyright © 2020 Heriberto Delgado. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "AppDelegate.h"

int main(int argc, char * argv[])
{
    NSString * appDelegateClassName;
    @autoreleasepool
    {
        appDelegateClassName = NSStringFromClass([AppDelegate class]);
    }
    return UIApplicationMain(argc, argv, nil, appDelegateClassName);
}
