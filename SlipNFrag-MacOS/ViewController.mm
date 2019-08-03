//
//  ViewController.m
//  SlipNFrag-MacOS
//
//  Created by Heriberto Delgado on 5/29/19.
//  Copyright © 2019 Heriberto Delgado. All rights reserved.
//

#import "ViewController.h"
#import <MetalKit/MetalKit.h>
#include "in_macos.h"
#include "scantokey.h"
#include "sys_macos.h"
#include "vid_macos.h"
#include "AppDelegate.h"

@interface ViewController () <MTKViewDelegate>

@end

@implementation ViewController
{
    NSButton* playButton;
    NSButton* preferencesButton;
    id<MTLCommandQueue> commandQueue;
    MTKView* mtkView;
    id<MTLRenderPipelineState> pipelineState;
    id<MTLBuffer> modelViewProjectionMatrixBuffer;
    id<MTLTexture> screen;
    id<MTLTexture> palette;
    id<MTLSamplerState> screenSamplerState;
    id<MTLSamplerState> paletteSamplerState;
    id<MTLBuffer> screenPlane;
    NSTimeInterval previousTime;
    NSTimeInterval currentTime;
    BOOL firstFrame;
    NSEventModifierFlags previousModifierFlags;
    NSTrackingArea* trackingArea;
    NSCursor* blankCursor;
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    playButton = [[NSButton alloc] initWithFrame:NSZeroRect];
    playButton.image = [NSImage imageNamed:@"play"];
    playButton.alternateImage = [NSImage imageNamed:@"play-pressed"];
    playButton.bordered = NO;
    playButton.translatesAutoresizingMaskIntoConstraints = NO;
    [playButton setButtonType:NSMomentaryChangeButton];
    [playButton setTarget:self];
    [playButton setAction:@selector(play:)];
    [self.view addSubview:playButton];
    [self.view addConstraint:[NSLayoutConstraint constraintWithItem:playButton attribute:NSLayoutAttributeCenterX relatedBy:NSLayoutRelationEqual toItem:self.view attribute:NSLayoutAttributeCenterX multiplier:1 constant:0]];
    [self.view addConstraint:[NSLayoutConstraint constraintWithItem:playButton attribute:NSLayoutAttributeCenterY relatedBy:NSLayoutRelationEqual toItem:self.view attribute:NSLayoutAttributeCenterY multiplier:1 constant:0]];
    [playButton addConstraint:[NSLayoutConstraint constraintWithItem:playButton attribute:NSLayoutAttributeWidth relatedBy:NSLayoutRelationEqual toItem:nil attribute:NSLayoutAttributeNotAnAttribute multiplier:1 constant:150]];
    [playButton addConstraint:[NSLayoutConstraint constraintWithItem:playButton attribute:NSLayoutAttributeHeight relatedBy:NSLayoutRelationEqual toItem:nil attribute:NSLayoutAttributeNotAnAttribute multiplier:1 constant:150]];
    preferencesButton = [[NSButton alloc] initWithFrame:NSZeroRect];
    preferencesButton.image = [NSImage imageNamed:NSImageNameAdvanced];
    preferencesButton.bordered = NO;
    preferencesButton.translatesAutoresizingMaskIntoConstraints = NO;
    preferencesButton.imageScaling = NSImageScaleProportionallyUpOrDown;
    [preferencesButton setButtonType:NSMomentaryChangeButton];
    [preferencesButton setTarget:self];
    [preferencesButton setAction:@selector(preferences:)];
    [self.view addSubview:preferencesButton];
    [self.view addConstraint:[NSLayoutConstraint constraintWithItem:preferencesButton attribute:NSLayoutAttributeRight relatedBy:NSLayoutRelationEqual toItem:self.view attribute:NSLayoutAttributeRight multiplier:1 constant:-10]];
    [self.view addConstraint:[NSLayoutConstraint constraintWithItem:preferencesButton attribute:NSLayoutAttributeBottom relatedBy:NSLayoutRelationEqual toItem:self.view attribute:NSLayoutAttributeBottom multiplier:1 constant:-10]];
    [preferencesButton addConstraint:[NSLayoutConstraint constraintWithItem:preferencesButton attribute:NSLayoutAttributeWidth relatedBy:NSLayoutRelationEqual toItem:nil attribute:NSLayoutAttributeNotAnAttribute multiplier:1 constant:24]];
    [preferencesButton addConstraint:[NSLayoutConstraint constraintWithItem:preferencesButton attribute:NSLayoutAttributeHeight relatedBy:NSLayoutRelationEqual toItem:nil attribute:NSLayoutAttributeNotAnAttribute multiplier:1 constant:24]];
}

-(void)viewWillAppear
{
    [super viewWillAppear];
    trackingArea = [[NSTrackingArea alloc] initWithRect:mtkView.frame options:NSTrackingCursorUpdate | NSTrackingActiveAlways owner:mtkView userInfo:nil];
    [mtkView addTrackingArea:trackingArea];
    [NSNotificationCenter.defaultCenter addObserverForName:NSViewFrameDidChangeNotification object:mtkView queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification * _Nonnull note)
    {
        [self->mtkView removeTrackingArea:self->trackingArea];
        self->trackingArea = [[NSTrackingArea alloc] initWithRect:self->mtkView.frame options:NSTrackingCursorUpdate | NSTrackingActiveAlways owner:self->mtkView userInfo:nil];
        [self->mtkView addTrackingArea:self->trackingArea];
    }];
}

-(void)viewWillDisappear
{
    [super viewWillDisappear];
    [mtkView removeTrackingArea:trackingArea];
}

-(void)cursorUpdate:(NSEvent *)event
{
    [blankCursor set];
}

-(void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size
{
}

- (void)drawInMTKView:(MTKView *)view
{
    if (sys_errormessage.length() > 0)
    {
        return;
    }
    if (previousTime < 0)
    {
        previousTime = CACurrentMediaTime();
    }
    else if (currentTime < 0)
    {
        currentTime = CACurrentMediaTime();
    }
    else
    {
        previousTime = currentTime;
        currentTime = CACurrentMediaTime();
        frame_lapse = currentTime - previousTime;
    }
    auto width = mtkView.bounds.size.width;
    auto height = mtkView.bounds.size.height;
    if (firstFrame || vid_width != (int)width || vid_height != (int)height)
    {
        vid_width = (int)width;
        vid_height = (int)height;
        VID_Resize();
        float matrix[16];
        bzero(matrix, sizeof(matrix));
        matrix[0] = 1;
        matrix[5] = 1;
        matrix[10] = -1;
        matrix[15] = 1;
        float* matrixData = (float*)modelViewProjectionMatrixBuffer.contents;
        memcpy(matrixData, matrix, sizeof(matrix));
        MTLTextureDescriptor* textureDescriptor = [MTLTextureDescriptor new];
        textureDescriptor.pixelFormat = MTLPixelFormatR8Unorm;
        textureDescriptor.width = width;
        textureDescriptor.height = height;
        screen = [mtkView.device newTextureWithDescriptor:textureDescriptor];
        firstFrame = NO;
    }
    Sys_Frame(frame_lapse);
    if ([self displaySysErrorIfNeeded])
    {
        return;
    }
    MTLRegion screenRegion = MTLRegionMake2D(0, 0, width, height);
    [screen replaceRegion:screenRegion mipmapLevel:0 withBytes:vid_buffer.data() bytesPerRow:width];
    MTLRegion paletteRegion = MTLRegionMake1D(0, 256);
    [palette replaceRegion:paletteRegion mipmapLevel:0 withBytes:d_8to24table bytesPerRow:0];
    id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
    MTLRenderPassDescriptor* renderPassDescriptor = mtkView.currentRenderPassDescriptor;
    id<CAMetalDrawable> currentDrawable = mtkView.currentDrawable;
    if (renderPassDescriptor != nil && currentDrawable != nil)
    {
        renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
        renderPassDescriptor.depthAttachment.loadAction = MTLLoadActionClear;
        id<MTLRenderCommandEncoder> commandEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
        [commandEncoder setRenderPipelineState:pipelineState];
        [commandEncoder setVertexBuffer:screenPlane offset:0 atIndex:0];
        [commandEncoder setVertexBuffer:modelViewProjectionMatrixBuffer offset:0 atIndex:1];
        [commandEncoder setFragmentTexture:screen atIndex:0];
        [commandEncoder setFragmentTexture:palette atIndex:1];
        [commandEncoder setFragmentSamplerState:screenSamplerState atIndex:0];
        [commandEncoder setFragmentSamplerState:paletteSamplerState atIndex:1];
        [commandEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
        [commandEncoder endEncoding];
        [commandBuffer presentDrawable:currentDrawable];
    }
    [commandBuffer commit];
}

-(void)play:(NSButton*)sender
{
    [playButton removeFromSuperview];
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    mtkView = [[MTKView alloc] initWithFrame:CGRectZero device:device];
    mtkView.delegate = self;
    mtkView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:mtkView];
    [self.view addConstraint:[NSLayoutConstraint constraintWithItem:mtkView attribute:NSLayoutAttributeLeft relatedBy:NSLayoutRelationEqual toItem:self.view attribute:NSLayoutAttributeLeft multiplier:1 constant:0]];
    [self.view addConstraint:[NSLayoutConstraint constraintWithItem:mtkView attribute:NSLayoutAttributeTop relatedBy:NSLayoutRelationEqual toItem:self.view attribute:NSLayoutAttributeTop multiplier:1 constant:0]];
    [self.view addConstraint:[NSLayoutConstraint constraintWithItem:mtkView attribute:NSLayoutAttributeRight relatedBy:NSLayoutRelationEqual toItem:self.view attribute:NSLayoutAttributeRight multiplier:1 constant:0]];
    [self.view addConstraint:[NSLayoutConstraint constraintWithItem:mtkView attribute:NSLayoutAttributeBottom relatedBy:NSLayoutRelationEqual toItem:self.view attribute:NSLayoutAttributeBottom multiplier:1 constant:0]];
    commandQueue = [device newCommandQueue];
    id<MTLLibrary> library = [device newDefaultLibrary];
    id<MTLFunction> vertexProgram = [library newFunctionWithName:@"vertexMain"];
    id<MTLFunction> fragmentProgram = [library newFunctionWithName:@"fragmentMain"];
    MTLVertexDescriptor* vertexDescriptor = [MTLVertexDescriptor new];
    vertexDescriptor.attributes[0].format = MTLVertexFormatFloat4;
    vertexDescriptor.attributes[0].offset = 0;
    vertexDescriptor.attributes[0].bufferIndex = 0;
    vertexDescriptor.attributes[1].format = MTLVertexFormatFloat2;
    vertexDescriptor.attributes[1].offset = 16;
    vertexDescriptor.attributes[1].bufferIndex = 0;
    vertexDescriptor.layouts[0].stride = 24;
    MTLRenderPipelineDescriptor* pipelineStateDescriptor = [MTLRenderPipelineDescriptor new];
    pipelineStateDescriptor.vertexDescriptor = vertexDescriptor;
    pipelineStateDescriptor.vertexFunction = vertexProgram;
    pipelineStateDescriptor.fragmentFunction = fragmentProgram;
    pipelineStateDescriptor.colorAttachments[0].pixelFormat = mtkView.colorPixelFormat;
    pipelineStateDescriptor.colorAttachments[0].blendingEnabled = YES;
    pipelineStateDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    pipelineStateDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
    pipelineStateDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    pipelineStateDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    pipelineStateDescriptor.sampleCount = mtkView.sampleCount;
    NSError* error = nil;
    pipelineState = [device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];
    if (error != nil)
    {
        NSLog(@"%@", error);
        [NSApplication.sharedApplication terminate:self];
    }
    modelViewProjectionMatrixBuffer = [device newBufferWithLength:16 * sizeof(float) options:0];
    MTLTextureDescriptor* paletteDescriptor = [MTLTextureDescriptor new];
    paletteDescriptor.textureType = MTLTextureType1D;
    paletteDescriptor.width = 256;
    palette = [device newTextureWithDescriptor:paletteDescriptor];
    MTLSamplerDescriptor* screenSamplerDescriptor = [MTLSamplerDescriptor new];
    screenSamplerState = [device newSamplerStateWithDescriptor:screenSamplerDescriptor];
    MTLSamplerDescriptor* paletteSamplerDescriptor = [MTLSamplerDescriptor new];
    paletteSamplerState = [device newSamplerStateWithDescriptor:paletteSamplerDescriptor];
    float screenPlaneVertices[] = {
        -1, 1, -1, 1,
        0, 0,
        1, 1, -1, 1,
        1, 0,
        -1, -1, -1, 1,
        0, 1,
        1, -1, -1, 1,
        1, 1};
    screenPlane = [device newBufferWithBytes:screenPlaneVertices length:sizeof(screenPlaneVertices) options:0];
    vid_width = (int)self.view.frame.size.width;
    vid_height = (int)self.view.frame.size.height;
    auto commandLine = [NSUserDefaults.standardUserDefaults stringForKey:@"command_line_text"];
    if (commandLine != nil && ![commandLine isEqualToString:@""])
    {
        std::string firstArgument = sys_argv[0];
        NSArray<NSString*>* arguments = [commandLine componentsSeparatedByString:@" "];
        sys_argc = (int)arguments.count + 1;
        sys_argv = new char*[sys_argc];
        auto i = 0;
        sys_argv[i] = new char[firstArgument.length() + 1];
        strcpy(sys_argv[i], firstArgument.c_str());
        for (NSString* argument : arguments)
        {
            i++;
            auto argumentAsString = [argument cStringUsingEncoding:NSString.defaultCStringEncoding];
            sys_argv[i] = new char[strlen(argumentAsString) + 1];
            strcpy(sys_argv[i], argumentAsString);
        }
    }
    else
    {
        auto basedir = [NSUserDefaults.standardUserDefaults stringForKey:@"basedir_text"];
        if (basedir != nil && ![basedir isEqualToString:@""])
        {
            std::vector<std::string> arguments;
            arguments.emplace_back(sys_argv[0]);
            arguments.emplace_back("-basedir");
            arguments.emplace_back([basedir cStringUsingEncoding:NSString.defaultCStringEncoding]);
            if ([NSUserDefaults.standardUserDefaults boolForKey:@"hipnotic_radio"])
            {
                arguments.emplace_back("-hipnotic");
            }
            else if ([NSUserDefaults.standardUserDefaults boolForKey:@"rogue_radio"])
            {
                arguments.emplace_back("-rogue");
            }
            else if ([NSUserDefaults.standardUserDefaults boolForKey:@"game_radio"])
            {
                arguments.emplace_back("-game");
                auto game = [NSUserDefaults.standardUserDefaults stringForKey:@"game_text"];
                if (game != nil && ![game isEqualToString:@""])
                {
                    arguments.emplace_back([game cStringUsingEncoding:NSString.defaultCStringEncoding]);
                }
            }
            sys_argc = (int)arguments.size();
            sys_argv = new char*[sys_argc];
            for (auto i = 0; i < arguments.size(); i++)
            {
                sys_argv[i] = new char[arguments[i].length() + 1];
                strcpy(sys_argv[i], arguments[i].c_str());
            }
        }
    }
    Sys_Init(sys_argc, sys_argv);
    if ([self displaySysErrorIfNeeded])
    {
        return;
    }
    previousTime = -1;
    currentTime = -1;
    previousModifierFlags = 0;
    blankCursor = [[NSCursor alloc] initWithImage:[NSImage imageNamed:@"blank"] hotSpot:NSZeroPoint];
    firstFrame = YES;
    [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown handler:^NSEvent * _Nullable(NSEvent* _Nonnull event)
     {
         if (!self.view.window.isKeyWindow)
         {
             return event;
         }
         unsigned short code = event.keyCode;
         int mapped = scantokey[code];
         Key_Event(mapped, YES);
         return nil;
     }];
    [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyUp handler:^NSEvent * _Nullable(NSEvent * _Nonnull event)
     {
         if (!self.view.window.isKeyWindow)
         {
             return event;
         }
         unsigned short code = event.keyCode;
         int mapped = scantokey[code];
         Key_Event(mapped, NO);
         return nil;
     }];
    [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskFlagsChanged handler:^NSEvent * _Nullable(NSEvent * _Nonnull event)
     {
         if (!self.view.window.isKeyWindow)
         {
             return event;
         }
         if ((event.modifierFlags & NSEventModifierFlagOption) != 0 && (self->previousModifierFlags & NSEventModifierFlagOption) == 0)
         {
             Key_Event(132, YES);
         }
         else if ((event.modifierFlags & NSEventModifierFlagOption) == 0 && (self->previousModifierFlags & NSEventModifierFlagOption) != 0)
         {
             Key_Event(132, NO);
         }
         if ((event.modifierFlags & NSEventModifierFlagControl) != 0 && (self->previousModifierFlags & NSEventModifierFlagControl) == 0)
         {
             Key_Event(133, YES);
         }
         else if ((event.modifierFlags & NSEventModifierFlagControl) == 0 && (self->previousModifierFlags & NSEventModifierFlagControl) != 0)
         {
             Key_Event(133, NO);
         }
         if ((event.modifierFlags & NSEventModifierFlagShift) != 0 && (self->previousModifierFlags & NSEventModifierFlagShift) == 0)
         {
             Key_Event(134, YES);
         }
         else if ((event.modifierFlags & NSEventModifierFlagShift) == 0 && (self->previousModifierFlags & NSEventModifierFlagShift) != 0)
         {
             Key_Event(134, NO);
         }
         self->previousModifierFlags = event.modifierFlags;
         return nil;
     }];
    [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskMouseMoved handler:^NSEvent * _Nullable(NSEvent * _Nonnull event)
     {
         mx += event.deltaX;
         my += event.deltaY;
         return event;
     }];
    [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskLeftMouseDown handler:^NSEvent * _Nullable(NSEvent * _Nonnull event)
     {
         Key_Event(200, YES);
         return event;
     }];
    [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskLeftMouseUp handler:^NSEvent * _Nullable(NSEvent * _Nonnull event)
     {
         Key_Event(200, NO);
         return event;
     }];
    [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskRightMouseDown handler:^NSEvent * _Nullable(NSEvent * _Nonnull event)
     {
         Key_Event(201, YES);
         return event;
     }];
    [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskRightMouseUp handler:^NSEvent * _Nullable(NSEvent * _Nonnull event)
     {
         Key_Event(201, NO);
         return event;
     }];
}

-(void)preferences:(NSButton*)sender
{
    AppDelegate* appDelegate = NSApplication.sharedApplication.delegate;
    [appDelegate preferences:nil];
}

-(BOOL)displaySysErrorIfNeeded
{
    if (sys_errormessage.length() > 0)
    {
        if (sys_nogamedata)
        {
            NSAlert* alert = [NSAlert new];
            [alert setMessageText:@"Game data not found"];
            [alert setInformativeText:@"Go to Preferences and set Game directory (-basedir) to your copy of the game files, or add it to the Command line."];
            [alert addButtonWithTitle:@"Go to Preferences"];
            [alert addButtonWithTitle:@"Where to buy the game"];
            [alert addButtonWithTitle:@"Where to get shareware episode"];
            [alert setAlertStyle:NSAlertStyleCritical];
            [alert beginSheetModalForWindow:self.view.window completionHandler:^(NSModalResponse returnCode)
            {
                [NSApplication.sharedApplication.mainWindow close];
                if (returnCode == NSAlertFirstButtonReturn)
                {
                    AppDelegate* appDelegate = NSApplication.sharedApplication.delegate;
                    [appDelegate preferences:nil];
                }
                else if (returnCode == NSAlertSecondButtonReturn)
                {
                    [NSWorkspace.sharedWorkspace openURL:[NSURL URLWithString:@"https://www.google.com/search?q=buy+quake+1+game"]];
                }
                else if (returnCode == NSAlertThirdButtonReturn)
                {
                    [NSWorkspace.sharedWorkspace openURL:[NSURL URLWithString:@"https://www.google.com/search?q=download+quake+1+shareware+episode"]];
                }
            }];
        }
        else
        {
            NSAlert* alert = [NSAlert new];
            [alert setMessageText:@"Sys_Error"];
            [alert setInformativeText:[NSString stringWithFormat:@"%s\n\nThe application will now close.", sys_errormessage.c_str()]];
            [alert addButtonWithTitle:@"Ok"];
            [alert setAlertStyle:NSAlertStyleCritical];
            [alert beginSheetModalForWindow:self.view.window completionHandler:^(NSModalResponse returnCode)
            {
                [NSApplication.sharedApplication.mainWindow close];
            }];
        }
        return YES;
    }
    return NO;
}

@end
