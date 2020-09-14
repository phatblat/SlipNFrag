//
//  GameView.h
//  SlipNFrag-iOS
//
//  Created by Heriberto Delgado on 9/13/20.
//  Copyright © 2020 Heriberto Delgado. All rights reserved.
//

#import <MetalKit/MetalKit.h>

@interface GameView : MTKView

@property (nonatomic, readonly) NSMutableArray<UIButton*>* buttons;

@property (nonatomic, weak) UIViewController* parentViewController;

@property (nonatomic, readonly) NSMutableArray<NSValue*>* downActions;

@property (nonatomic, readonly) NSMutableArray<NSValue*>* upActions;

@end
