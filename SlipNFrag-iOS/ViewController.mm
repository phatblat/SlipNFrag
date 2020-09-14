//
//  ViewController.m
//  SlipNFrag-iOS
//
//  Created by Heriberto Delgado on 9/13/20.
//  Copyright © 2020 Heriberto Delgado. All rights reserved.
//

#import "ViewController.h"
#import "GameView.h"
#import <ARKit/ARKit.h>
#include "sys_ios.h"
#include "vid_ios.h"
#include "in_ios.h"

enum AppMode
{
    AppStartupMode,
    AppScreenMode,
    AppWorldMode,
    AppNoGameDataMode
};

@interface ViewController () <MTKViewDelegate, ARSessionDelegate>

@end

@implementation ViewController
{
    UIButton* playButton;
    UIButton* preferencesButton;
    id<MTLCommandQueue> commandQueue;
    GameView* gameView;
    UIButton* leftButton;
    UIButton* upButton;
    UIButton* rightButton;
    UIButton* downButton;
    UIButton* aButton;
    UIButton* bButton;
    UIButton* xButton;
    UIButton* yButton;
    id<MTLRenderPipelineState> pipelineState;
    id<MTLTexture> screen;
    id<MTLTexture> console;
    id<MTLTexture> palette;
    id<MTLSamplerState> screenSamplerState;
    id<MTLSamplerState> consoleSamplerState;
    id<MTLSamplerState> paletteSamplerState;
    id<MTLBuffer> screenPlane;
    ARSession* session;
    NSTimeInterval previousTime;
    NSTimeInterval currentTime;
    BOOL firstFrame;
    double defaultFOV;
    AppMode mode;
    AppMode previousMode;
    float forcedAspect;
    float yaw;
    float pitch;
    float roll;
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    playButton = [[UIButton alloc] initWithFrame:CGRectZero];
    [playButton setImage:[UIImage imageNamed:@"play"] forState:UIControlStateNormal];
    [playButton setImage:[UIImage imageNamed:@"play-pressed"] forState:UIControlStateHighlighted];
    playButton.translatesAutoresizingMaskIntoConstraints = NO;
    [playButton addTarget:self action:@selector(play:) forControlEvents:UIControlEventTouchUpInside];
    [self.view addSubview:playButton];
    [playButton.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor].active = YES;
    [playButton.centerYAnchor constraintEqualToAnchor:self.view.centerYAnchor].active = YES;
    [playButton.widthAnchor constraintEqualToConstant:150].active = YES;
    [playButton.heightAnchor constraintEqualToConstant:150].active = YES;
    preferencesButton = [[UIButton alloc] initWithFrame:CGRectZero];
    [preferencesButton setImage:[UIImage systemImageNamed:@"gear"] forState:UIControlStateNormal];
    preferencesButton.translatesAutoresizingMaskIntoConstraints = NO;
    [preferencesButton addTarget:self action:@selector(preferences:) forControlEvents:UIControlEventTouchUpInside];
    [self.view addSubview:preferencesButton];
    [preferencesButton.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor constant:-45].active = YES;
    [preferencesButton.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor constant:-45].active = YES;
    [preferencesButton.widthAnchor constraintEqualToConstant:24].active = true;
    [preferencesButton.heightAnchor constraintEqualToConstant:24].active = true;
    mode = AppStartupMode;
    previousMode = AppStartupMode;
    forcedAspect = 320.0f / 240.0f;
    session = [[ARSession alloc] init];
    session.delegate = self;
    AROrientationTrackingConfiguration* configuration = [[AROrientationTrackingConfiguration alloc] init];
    [session runWithConfiguration:configuration];
}

- (void)viewWillAppear:(BOOL)animated
{
    self.navigationController.navigationBarHidden = YES;
}

- (void)viewWillDisappear:(BOOL)animated
{
    self.navigationController.navigationBarHidden = NO;
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
    if (mode == AppWorldMode)
    {
        cl.viewangles[YAW] = yaw + 90;
        cl.viewangles[PITCH] = -pitch;
        cl.viewangles[ROLL] = -roll;
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
    if (mode != AppStartupMode && mode != AppNoGameDataMode)
    {
        if (cls.demoplayback || cl.intermission)
        {
            mode = AppScreenMode;
        }
        else
        {
            mode = AppWorldMode;
        }
    }
    bool modeChanged = NO;
    if (previousMode != mode)
    {
        if (mode == AppWorldMode)
        {
            forcedAspect = gameView.bounds.size.width / gameView.bounds.size.height;
            Cvar_SetValue("fov", 30 * forcedAspect);
        }
        else
        {
            forcedAspect = 320.0f / 240.0f;
            Cvar_SetValue("fov", defaultFOV);
        }
        modeChanged = YES;
        previousMode = mode;
    }
    auto width = gameView.bounds.size.width;
    auto height = gameView.bounds.size.height;
    if (firstFrame || vid_width != (int)width || vid_height != (int)height || modeChanged)
    {
        vid_width = (int)width;
        vid_height = (int)height;
        [self calculateConsoleDimensions];
        VID_Resize(forcedAspect);
        MTLTextureDescriptor* screenDescriptor = [MTLTextureDescriptor new];
        screenDescriptor.pixelFormat = MTLPixelFormatR8Unorm;
        screenDescriptor.width = vid_width;
        screenDescriptor.height = vid_height;
        screen = [gameView.device newTextureWithDescriptor:screenDescriptor];
        MTLTextureDescriptor* consoleDescriptor = [MTLTextureDescriptor new];
        consoleDescriptor.pixelFormat = MTLPixelFormatR8Unorm;
        consoleDescriptor.width = con_width;
        consoleDescriptor.height = con_height;
        console = [gameView.device newTextureWithDescriptor:consoleDescriptor];
        firstFrame = NO;
    }
    if (r_cache_thrash)
    {
        VID_ReallocSurfCache();
    }
    auto nodrift = cl.nodrift;
    if (mode == AppWorldMode)
    {
        cl.nodrift = true;
    }
    Sys_Frame(frame_lapse);
    if ([self displaySysErrorIfNeeded])
    {
        return;
    }
    if (mode == AppWorldMode)
    {
        cl.nodrift = nodrift;
    }
    MTLRegion screenRegion = MTLRegionMake2D(0, 0, vid_width, vid_height);
    [screen replaceRegion:screenRegion mipmapLevel:0 withBytes:vid_buffer.data() bytesPerRow:vid_width];
    MTLRegion consoleRegion = MTLRegionMake2D(0, 0, con_width, con_height);
    [console replaceRegion:consoleRegion mipmapLevel:0 withBytes:con_buffer.data() bytesPerRow:con_width];
    MTLRegion paletteRegion = MTLRegionMake1D(0, 256);
    [palette replaceRegion:paletteRegion mipmapLevel:0 withBytes:d_8to24table bytesPerRow:0];
    id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
    MTLRenderPassDescriptor* renderPassDescriptor = gameView.currentRenderPassDescriptor;
    id<CAMetalDrawable> currentDrawable = gameView.currentDrawable;
    if (renderPassDescriptor != nil && currentDrawable != nil)
    {
        renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
        renderPassDescriptor.depthAttachment.loadAction = MTLLoadActionClear;
        id<MTLRenderCommandEncoder> commandEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
        [commandEncoder setRenderPipelineState:pipelineState];
        [commandEncoder setVertexBuffer:screenPlane offset:0 atIndex:0];
        [commandEncoder setFragmentTexture:screen atIndex:0];
        [commandEncoder setFragmentTexture:console atIndex:1];
        [commandEncoder setFragmentTexture:palette atIndex:2];
        [commandEncoder setFragmentSamplerState:screenSamplerState atIndex:0];
        [commandEncoder setFragmentSamplerState:consoleSamplerState atIndex:1];
        [commandEncoder setFragmentSamplerState:paletteSamplerState atIndex:2];
        [commandEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
        [commandEncoder endEncoding];
        [commandBuffer presentDrawable:currentDrawable];
    }
    [commandBuffer commit];
}

-(void)play:(UIButton*)sender
{
    [playButton removeFromSuperview];
    [preferencesButton removeFromSuperview];
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    gameView = [[GameView alloc] initWithFrame:CGRectZero device:device];
    gameView.delegate = self;
    gameView.parentViewController = self;
    gameView.translatesAutoresizingMaskIntoConstraints = NO;
    gameView.multipleTouchEnabled = YES;
    [self.view addSubview:gameView];
    [gameView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor].active = YES;
    [gameView.topAnchor constraintEqualToAnchor:self.view.topAnchor].active = YES;
    [gameView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor].active = YES;
    [gameView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor].active = YES;
    leftButton = [[UIButton alloc] initWithFrame:CGRectZero];
    [leftButton setTitle:@"L" forState:UIControlStateNormal];
    [leftButton setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    leftButton.layer.borderWidth = 1;
    leftButton.layer.borderColor = [UIColor whiteColor].CGColor;
    leftButton.layer.cornerRadius = 35;
    leftButton.translatesAutoresizingMaskIntoConstraints = NO;
    leftButton.userInteractionEnabled = NO;
    [self.view addSubview:leftButton];
    [leftButton.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor constant:55].active = YES;
    [leftButton.centerYAnchor constraintEqualToAnchor:self.view.centerYAnchor].active = YES;
    [leftButton.widthAnchor constraintEqualToConstant:70].active = YES;
    [leftButton.heightAnchor constraintEqualToConstant:70].active = YES;
    [gameView.buttons addObject:leftButton];
    [gameView.downActions addObject:[NSValue valueWithPointer:@selector(leftTouchDown)]];
    [gameView.upActions addObject:[NSValue valueWithPointer:@selector(leftTouchUp)]];
    upButton = [[UIButton alloc] initWithFrame:CGRectZero];
    [upButton setTitle:@"U" forState:UIControlStateNormal];
    [upButton setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    upButton.layer.borderWidth = 1;
    upButton.layer.borderColor = [UIColor whiteColor].CGColor;
    upButton.layer.cornerRadius = 35;
    upButton.translatesAutoresizingMaskIntoConstraints = NO;
    upButton.userInteractionEnabled = NO;
    [self.view addSubview:upButton];
    [upButton.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor constant:110].active = YES;
    [upButton.centerYAnchor constraintEqualToAnchor:self.view.centerYAnchor constant:-55].active = YES;
    [upButton.widthAnchor constraintEqualToConstant:70].active = YES;
    [upButton.heightAnchor constraintEqualToConstant:70].active = YES;
    [gameView.buttons addObject:upButton];
    [gameView.downActions addObject:[NSValue valueWithPointer:@selector(upTouchDown)]];
    [gameView.upActions addObject:[NSValue valueWithPointer:@selector(upTouchUp)]];
    rightButton = [[UIButton alloc] initWithFrame:CGRectZero];
    [rightButton setTitle:@"R" forState:UIControlStateNormal];
    [rightButton setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    rightButton.layer.borderWidth = 1;
    rightButton.layer.borderColor = [UIColor whiteColor].CGColor;
    rightButton.layer.cornerRadius = 35;
    rightButton.translatesAutoresizingMaskIntoConstraints = NO;
    rightButton.userInteractionEnabled = NO;
    [self.view addSubview:rightButton];
    [rightButton.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor constant:165].active = YES;
    [rightButton.centerYAnchor constraintEqualToAnchor:self.view.centerYAnchor].active = YES;
    [rightButton.widthAnchor constraintEqualToConstant:70].active = YES;
    [rightButton.heightAnchor constraintEqualToConstant:70].active = YES;
    [gameView.buttons addObject:rightButton];
    [gameView.downActions addObject:[NSValue valueWithPointer:@selector(rightTouchDown)]];
    [gameView.upActions addObject:[NSValue valueWithPointer:@selector(rightTouchUp)]];
    downButton = [[UIButton alloc] initWithFrame:CGRectZero];
    [downButton setTitle:@"D" forState:UIControlStateNormal];
    [downButton setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    downButton.layer.borderWidth = 1;
    downButton.layer.borderColor = [UIColor whiteColor].CGColor;
    downButton.layer.cornerRadius = 35;
    downButton.translatesAutoresizingMaskIntoConstraints = NO;
    downButton.userInteractionEnabled = NO;
    [self.view addSubview:downButton];
    [downButton.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor constant:110].active = YES;
    [downButton.centerYAnchor constraintEqualToAnchor:self.view.centerYAnchor constant:55].active = YES;
    [downButton.widthAnchor constraintEqualToConstant:70].active = YES;
    [downButton.heightAnchor constraintEqualToConstant:70].active = YES;
    [gameView.buttons addObject:downButton];
    [gameView.downActions addObject:[NSValue valueWithPointer:@selector(downTouchDown)]];
    [gameView.upActions addObject:[NSValue valueWithPointer:@selector(downTouchUp)]];
    bButton = [[UIButton alloc] initWithFrame:CGRectZero];
    [bButton setTitle:@"B" forState:UIControlStateNormal];
    [bButton setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    bButton.layer.borderWidth = 1;
    bButton.layer.borderColor = [UIColor whiteColor].CGColor;
    bButton.layer.cornerRadius = 35;
    bButton.translatesAutoresizingMaskIntoConstraints = NO;
    bButton.userInteractionEnabled = NO;
    [self.view addSubview:bButton];
    [bButton.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor constant:-55].active = YES;
    [bButton.centerYAnchor constraintEqualToAnchor:self.view.centerYAnchor].active = YES;
    [bButton.widthAnchor constraintEqualToConstant:70].active = YES;
    [bButton.heightAnchor constraintEqualToConstant:70].active = YES;
    [gameView.buttons addObject:bButton];
    [gameView.downActions addObject:[NSValue valueWithPointer:@selector(bTouchDown)]];
    [gameView.upActions addObject:[NSValue valueWithPointer:@selector(bTouchUp)]];
    yButton = [[UIButton alloc] initWithFrame:CGRectZero];
    [yButton setTitle:@"Y" forState:UIControlStateNormal];
    [yButton setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    yButton.layer.borderWidth = 1;
    yButton.layer.borderColor = [UIColor whiteColor].CGColor;
    yButton.layer.cornerRadius = 35;
    yButton.translatesAutoresizingMaskIntoConstraints = NO;
    yButton.userInteractionEnabled = NO;
    [self.view addSubview:yButton];
    [yButton.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor constant:-110].active = YES;
    [yButton.centerYAnchor constraintEqualToAnchor:self.view.centerYAnchor constant:-55].active = YES;
    [yButton.widthAnchor constraintEqualToConstant:70].active = YES;
    [yButton.heightAnchor constraintEqualToConstant:70].active = YES;
    [gameView.buttons addObject:yButton];
    [gameView.downActions addObject:[NSValue valueWithPointer:@selector(yTouchDown)]];
    [gameView.upActions addObject:[NSValue valueWithPointer:@selector(yTouchUp)]];
    xButton = [[UIButton alloc] initWithFrame:CGRectZero];
    [xButton setTitle:@"X" forState:UIControlStateNormal];
    [xButton setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    xButton.layer.borderWidth = 1;
    xButton.layer.borderColor = [UIColor whiteColor].CGColor;
    xButton.layer.cornerRadius = 35;
    xButton.translatesAutoresizingMaskIntoConstraints = NO;
    xButton.userInteractionEnabled = NO;
    [self.view addSubview:xButton];
    [xButton.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor constant:-165].active = YES;
    [xButton.centerYAnchor constraintEqualToAnchor:self.view.centerYAnchor].active = YES;
    [xButton.widthAnchor constraintEqualToConstant:70].active = YES;
    [xButton.heightAnchor constraintEqualToConstant:70].active = YES;
    [gameView.buttons addObject:xButton];
    [gameView.downActions addObject:[NSValue valueWithPointer:@selector(xTouchDown)]];
    [gameView.upActions addObject:[NSValue valueWithPointer:@selector(xTouchUp)]];
    aButton = [[UIButton alloc] initWithFrame:CGRectZero];
    [aButton setTitle:@"A" forState:UIControlStateNormal];
    [aButton setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    aButton.layer.borderWidth = 1;
    aButton.layer.borderColor = [UIColor whiteColor].CGColor;
    aButton.layer.cornerRadius = 35;
    aButton.translatesAutoresizingMaskIntoConstraints = NO;
    aButton.userInteractionEnabled = NO;
    [self.view addSubview:aButton];
    [aButton.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor constant:-110].active = YES;
    [aButton.centerYAnchor constraintEqualToAnchor:self.view.centerYAnchor constant:55].active = YES;
    [aButton.widthAnchor constraintEqualToConstant:70].active = YES;
    [aButton.heightAnchor constraintEqualToConstant:70].active = YES;
    [gameView.buttons addObject:aButton];
    [gameView.downActions addObject:[NSValue valueWithPointer:@selector(aTouchDown)]];
    [gameView.upActions addObject:[NSValue valueWithPointer:@selector(aTouchUp)]];
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
    pipelineStateDescriptor.colorAttachments[0].pixelFormat = gameView.colorPixelFormat;
    pipelineStateDescriptor.colorAttachments[0].blendingEnabled = YES;
    pipelineStateDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    pipelineStateDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
    pipelineStateDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    pipelineStateDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    pipelineStateDescriptor.sampleCount = gameView.sampleCount;
    NSError* error = nil;
    pipelineState = [device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];
    if (error != nil)
    {
        NSLog(@"%@", error);
        exit(0);
    }
    MTLTextureDescriptor* paletteDescriptor = [MTLTextureDescriptor new];
    paletteDescriptor.textureType = MTLTextureType1D;
    paletteDescriptor.width = 256;
    palette = [device newTextureWithDescriptor:paletteDescriptor];
    MTLSamplerDescriptor* screenSamplerDescriptor = [MTLSamplerDescriptor new];
    screenSamplerState = [device newSamplerStateWithDescriptor:screenSamplerDescriptor];
    MTLSamplerDescriptor* consoleSamplerDescriptor = [MTLSamplerDescriptor new];
    consoleSamplerState = [device newSamplerStateWithDescriptor:consoleSamplerDescriptor];
    MTLSamplerDescriptor* paletteSamplerDescriptor = [MTLSamplerDescriptor new];
    paletteSamplerState = [device newSamplerStateWithDescriptor:paletteSamplerDescriptor];
    float screenPlaneVertices[] = { -1, 1, 0, 1, 0, 0, 1, 1, 0, 1, 1, 0, -1, -1, 0, 1, 0, 1, 1, -1, 0, 1, 1, 1 };
    screenPlane = [device newBufferWithBytes:screenPlaneVertices length:sizeof(screenPlaneVertices) options:0];
    vid_width = (int)self.view.frame.size.width;
    vid_height = (int)self.view.frame.size.height;
    [self calculateConsoleDimensions];
    mode = AppWorldMode;
    NSString* version = NSBundle.mainBundle.infoDictionary[@"CFBundleShortVersionString"];
    sys_version = std::string("iOS ") + [version cStringUsingEncoding:NSString.defaultCStringEncoding];
    std::vector<std::string> arguments;
    arguments.emplace_back("SlipNFrag-iOS");
    NSURL* url = [NSFileManager.defaultManager URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask][0];
    [url startAccessingSecurityScopedResource];
    arguments.emplace_back("-basedir");
    arguments.emplace_back([url.path cStringUsingEncoding:NSString.defaultCStringEncoding]);
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
        auto game = [NSUserDefaults.standardUserDefaults stringForKey:@"game_text"];
        if (game != nil && ![game isEqualToString:@""])
        {
            arguments.emplace_back("-game");
            arguments.emplace_back([game cStringUsingEncoding:NSString.defaultCStringEncoding]);
        }
    }
    auto commandLine = [NSUserDefaults.standardUserDefaults stringForKey:@"command_line_text"];
    if (commandLine != nil && ![commandLine isEqualToString:@""])
    {
        NSArray<NSString*>* components = [commandLine componentsSeparatedByString:@" "];
        for (NSString* component : components)
        {
            auto componentAsString = [component cStringUsingEncoding:NSString.defaultCStringEncoding];
            arguments.emplace_back(componentAsString);
        }
    }
    sys_argc = (int)arguments.size();
    sys_argv = new char*[sys_argc];
    for (auto i = 0; i < arguments.size(); i++)
    {
        sys_argv[i] = new char[arguments[i].length() + 1];
        strcpy(sys_argv[i], arguments[i].c_str());
    }
    Sys_Init(sys_argc, sys_argv);
    if ([self displaySysErrorIfNeeded])
    {
        return;
    }
    previousTime = -1;
    currentTime = -1;
    firstFrame = YES;
    defaultFOV = (int)Cvar_VariableValue("fov");
}

-(void)preferences:(UIButton*)sender
{
}

-(void)calculateConsoleDimensions
{
    auto factor = (double)vid_width / 320;
    double new_conwidth = 320;
    auto new_conheight = ceil((double)vid_height / factor);
    if (new_conheight < 200)
    {
        factor = (double)vid_height / 200;
        new_conheight = 200;
        new_conwidth = (double)(((int)ceil((double)vid_width / factor) + 3) & ~3);
    }
    con_width = (int)new_conwidth;
    con_height = (int)new_conheight;
}

-(BOOL)displaySysErrorIfNeeded
{
    if (sys_errormessage.length() > 0)
    {
        if (sys_nogamedata)
        {
            mode = AppNoGameDataMode;
            UIAlertController* alert = [UIAlertController alertControllerWithTitle:@"Game data not found" message:@"Copy your game files into the application by using the Files app or your desktop to begin playing." preferredStyle:UIAlertControllerStyleAlert];
            [alert addAction:[UIAlertAction actionWithTitle:@"Where to buy the game" style:UIAlertActionStyleDefault handler:^(UIAlertAction * _Nonnull action)
            {
                [UIApplication.sharedApplication openURL:[NSURL URLWithString:@"https://www.google.com/search?q=buy+quake+1+game"] options:@{} completionHandler:nil];
            }]];
            [alert addAction:[UIAlertAction actionWithTitle:@"Where to get shareware episode" style:UIAlertActionStyleDefault handler:^(UIAlertAction * _Nonnull action)
            {
                [UIApplication.sharedApplication openURL:[NSURL URLWithString:@"https://www.google.com/search?q=download+quake+1+shareware+episode"] options:@{} completionHandler:nil];
            }]];
            [self presentViewController:alert animated:YES completion:nil];
        }
        else
        {
            UIAlertController* alert = [UIAlertController alertControllerWithTitle:@"Sys_Error" message:[NSString stringWithFormat:@"%s\n\nThe application will now close.", sys_errormessage.c_str()] preferredStyle:UIAlertControllerStyleAlert];
            [alert addAction:[UIAlertAction actionWithTitle:@"Dismiss" style:UIAlertActionStyleDefault handler:^(UIAlertAction * _Nonnull action)
            {
                exit(0);
            }]];
            [self presentViewController:alert animated:YES completion:nil];
        }
        return YES;
    }
    return NO;
}

- (void)leftTouchDown
{
    [leftButton setTitleColor:[UIColor clearColor] forState:UIControlStateNormal];
    leftButton.backgroundColor = [[UIColor whiteColor] colorWithAlphaComponent:0.4];
    if (key_dest == key_game && !cls.demoplayback && !cl.intermission)
    {
        Cmd_ExecuteString("+moveleft", src_command);
    }
    else
    {
        Key_Event(K_LEFTARROW, true);
    }
}

- (void)leftTouchUp
{
    [leftButton setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    leftButton.backgroundColor = [UIColor clearColor];
    if (key_dest == key_game && !cls.demoplayback && !cl.intermission)
    {
        Cmd_ExecuteString("-moveleft", src_command);
    }
    else
    {
        Key_Event(K_LEFTARROW, false);
    }
}

- (void)upTouchDown
{
    [upButton setTitleColor:[UIColor clearColor] forState:UIControlStateNormal];
    upButton.backgroundColor = [[UIColor whiteColor] colorWithAlphaComponent:0.4];
    if (key_dest == key_game && !cls.demoplayback && !cl.intermission)
    {
        Cmd_ExecuteString("+forward", src_command);
    }
    else
    {
        Key_Event(K_UPARROW, true);
    }
}

- (void)upTouchUp
{
    [upButton setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    upButton.backgroundColor = [UIColor clearColor];
    if (key_dest == key_game && !cls.demoplayback && !cl.intermission)
    {
        Cmd_ExecuteString("-forward", src_command);
    }
    else
    {
        Key_Event(K_UPARROW, false);
    }
}

- (void)rightTouchDown
{
    [rightButton setTitleColor:[UIColor blackColor] forState:UIControlStateNormal];
    rightButton.backgroundColor = [[UIColor whiteColor] colorWithAlphaComponent:0.4];
    if (key_dest == key_game && !cls.demoplayback && !cl.intermission)
    {
        Cmd_ExecuteString("+moveright", src_command);
    }
    else
    {
        Key_Event(K_RIGHTARROW, true);
    }
}

- (void)rightTouchUp
{
    [rightButton setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    rightButton.backgroundColor = [UIColor clearColor];
    if (key_dest == key_game && !cls.demoplayback && !cl.intermission)
    {
        Cmd_ExecuteString("-moveright", src_command);
    }
    else
    {
        Key_Event(K_RIGHTARROW, false);
    }
}

- (void)downTouchDown
{
    [downButton setTitleColor:[UIColor blackColor] forState:UIControlStateNormal];
    downButton.backgroundColor = [[UIColor whiteColor] colorWithAlphaComponent:0.4];
    if (key_dest == key_game && !cls.demoplayback && !cl.intermission)
    {
        Cmd_ExecuteString("+back", src_command);
    }
    else
    {
        Key_Event(K_DOWNARROW, true);
    }
}

- (void)downTouchUp
{
    [downButton setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    downButton.backgroundColor = [UIColor clearColor];
    if (key_dest == key_game && !cls.demoplayback && !cl.intermission)
    {
        Cmd_ExecuteString("-back", src_command);
    }
    else
    {
        Key_Event(K_DOWNARROW, false);
    }
}

- (void)aTouchDown
{
    [aButton setTitleColor:[UIColor blackColor] forState:UIControlStateNormal];
    aButton.backgroundColor = [[UIColor whiteColor] colorWithAlphaComponent:0.4];
    if (key_dest == key_game && !cls.demoplayback && !cl.intermission)
    {
        Cmd_ExecuteString("+movedown", src_command);
    }
    else
    {
        Key_Event(K_ENTER, true);
    }
}

- (void)aTouchUp
{
    [aButton setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    aButton.backgroundColor = [UIColor clearColor];
    if (key_dest == key_game && !cls.demoplayback && !cl.intermission)
    {
        Cmd_ExecuteString("-movedown", src_command);
    }
    else
    {
        Key_Event(K_ENTER, false);
    }
}

- (void)bTouchDown
{
    [bButton setTitleColor:[UIColor blackColor] forState:UIControlStateNormal];
    bButton.backgroundColor = [[UIColor whiteColor] colorWithAlphaComponent:0.4];
    if (key_dest == key_game && !cls.demoplayback && !cl.intermission)
    {
        Cmd_ExecuteString("impulse 10", src_command);
    }
    else
    {
        Key_Event(K_ESCAPE, true);
    }
}

- (void)bTouchUp
{
    [bButton setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    bButton.backgroundColor = [UIColor clearColor];
    if (key_dest != key_game || cls.demoplayback || cl.intermission)
    {
        Key_Event(K_ESCAPE, false);
    }
}

- (void)xTouchDown
{
    [xButton setTitleColor:[UIColor blackColor] forState:UIControlStateNormal];
    xButton.backgroundColor = [[UIColor whiteColor] colorWithAlphaComponent:0.4];
    if (key_dest == key_game && !cls.demoplayback && !cl.intermission)
    {
        Cmd_ExecuteString("+attack", src_command);
    }
}

- (void)xTouchUp
{
    [xButton setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    xButton.backgroundColor = [UIColor clearColor];
    if (key_dest == key_game && !cls.demoplayback && !cl.intermission)
    {
        Cmd_ExecuteString("-attack", src_command);
    }
}

- (void)yTouchDown
{
    [yButton setTitleColor:[UIColor blackColor] forState:UIControlStateNormal];
    yButton.backgroundColor = [[UIColor whiteColor] colorWithAlphaComponent:0.4];
    if (key_dest == key_game && !cls.demoplayback && !cl.intermission)
    {
        Cmd_ExecuteString("+jump", src_command);
    }
}

- (void)yTouchUp
{
    [yButton setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    yButton.backgroundColor = [UIColor clearColor];
    if (key_dest == key_game && !cls.demoplayback && !cl.intermission)
    {
        Cmd_ExecuteString("-jump", src_command);
    }
}

- (void)session:(ARSession *)session didUpdateFrame:(ARFrame *)frame
{
    if (frame.camera != nil)
    {
        pitch = frame.camera.eulerAngles[0] * 180 / M_PI;
        yaw = frame.camera.eulerAngles[1] * 180 / M_PI;
        roll = frame.camera.eulerAngles[2] * 180 / M_PI;
    }
}

@end
