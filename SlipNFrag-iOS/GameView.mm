//
//  GameView.m
//  SlipNFrag-iOS
//
//  Created by Heriberto Delgado on 9/13/20.
//  Copyright © 2020 Heriberto Delgado. All rights reserved.
//

#import "GameView.h"

@implementation GameView
{
    NSMutableArray<UITouch*>* recordedTouches;
    NSMutableArray<UIButton*>* recordedButtons;
}

- (instancetype)initWithFrame:(CGRect)frameRect device:(id<MTLDevice>)device
{
    self = [super initWithFrame:frameRect device:device];
    if (self != nil)
    {
        self->_buttons = [NSMutableArray<UIButton*> new];
        self->_downActions = [NSMutableArray<NSValue*> new];
        self->_upActions = [NSMutableArray<NSValue*> new];
        self->recordedTouches = [NSMutableArray<UITouch*> new];
        self->recordedButtons = [NSMutableArray<UIButton*> new];
        self.userInteractionEnabled = YES;
    }
    return self;
}

- (void)awakeFromNib
{
    [super awakeFromNib];
    _buttons = [NSMutableArray<UIButton*> new];
    recordedTouches = [NSMutableArray<UITouch*> new];
    recordedButtons = [NSMutableArray<UIButton*> new];
    self.userInteractionEnabled = YES;
}

- (void)addOrEditTouches:(NSSet<UITouch*>*)touches
{
    for (UITouch* touch in touches)
    {
        NSInteger previousIndex = -1;
        for (NSUInteger i = 0; i < recordedTouches.count; i++)
        {
            if (recordedTouches[i] == touch)
            {
                for (NSUInteger j = 0; j < _buttons.count; j++)
                {
                    if (_buttons[j] == recordedButtons[i])
                    {
                        previousIndex = j;
                        break;
                    }
                }
                break;
            }
        }
        CGPoint location = [touch locationInView:self];
        float nearestDistanceSquared = FLT_MAX;
        NSInteger index = -1;
        for (NSUInteger i = 0; i < _buttons.count; i++)
        {
            CGRect frame = _buttons[i].frame;
            if (CGRectContainsPoint(frame, location))
            {
                float centerX = frame.origin.x + frame.size.width / 2;
                float centerY = frame.origin.y + frame.size.height / 2;
                float deltaX = location.x - centerX;
                float deltaY = location.y - centerY;
                float distanceSquared = deltaX * deltaX + deltaY * deltaY;
                if (nearestDistanceSquared > distanceSquared)
                {
                    nearestDistanceSquared = distanceSquared;
                    index = i;
                }
                break;
            }
        }
        if (index >= 0 && previousIndex < 0)
        {
            [recordedTouches addObject:touch];
            [recordedButtons addObject:_buttons[index]];
            SEL downAction = (SEL)[_downActions[index] pointerValue];
            IMP downImplementation = [_parentViewController methodForSelector:downAction];
            void (*downFunction)(id, SEL) = (void (*)(id, SEL))downImplementation;
            downFunction(_parentViewController, downAction);
        }
        else if (index < 0 && previousIndex >= 0)
        {
            SEL upAction = (SEL)[_upActions[previousIndex] pointerValue];
            IMP upImplementation = [_parentViewController methodForSelector:upAction];
            void (*upFunction)(id, SEL) = (void (*)(id, SEL))upImplementation;
            upFunction(_parentViewController, upAction);
            for (NSUInteger i = 0; i < recordedTouches.count; i++)
            {
                if (recordedTouches[i] == touch)
                {
                    [recordedTouches removeObjectAtIndex:i];
                    [recordedButtons removeObjectAtIndex:i];
                    break;
                }
            }
        }
        else if (index >= 0 && previousIndex >= 0 && index != previousIndex)
        {
            SEL upAction = (SEL)[_upActions[previousIndex] pointerValue];
            IMP upImplementation = [_parentViewController methodForSelector:upAction];
            void (*upFunction)(id, SEL) = (void (*)(id, SEL))upImplementation;
            upFunction(_parentViewController, upAction);
            for (NSUInteger i = 0; i < recordedTouches.count; i++)
            {
                if (recordedTouches[i] == touch)
                {
                    [recordedTouches removeObjectAtIndex:i];
                    [recordedButtons removeObjectAtIndex:i];
                    break;
                }
            }
            [recordedTouches addObject:touch];
            [recordedButtons addObject:_buttons[index]];
            SEL downAction = (SEL)[_downActions[index] pointerValue];
            IMP downImplementation = [_parentViewController methodForSelector:downAction];
            void (*downFunction)(id, SEL) = (void (*)(id, SEL))downImplementation;
            downFunction(_parentViewController, downAction);
        }
    }
}

- (void)deleteTouches:(NSSet<UITouch*>*)touches
{
    for (UITouch* touch in touches)
    {
        NSInteger index = -1;
        for (NSUInteger i = 0; i < recordedTouches.count; i++)
        {
            if (recordedTouches[i] == touch)
            {
                for (NSUInteger j = 0; j < _buttons.count; j++)
                {
                    if (_buttons[j] == recordedButtons[i])
                    {
                        index = j;
                        break;
                    }
                }
                break;
            }
        }
        if (index >= 0)
        {
            SEL upAction = (SEL)[_upActions[index] pointerValue];
            IMP upImplementation = [_parentViewController methodForSelector:upAction];
            void (*upFunction)(id, SEL) = (void (*)(id, SEL))upImplementation;
            upFunction(_parentViewController, upAction);
            for (NSUInteger i = 0; i < recordedTouches.count; i++)
            {
                if (recordedTouches[i] == touch)
                {
                    [recordedTouches removeObjectAtIndex:i];
                    [recordedButtons removeObjectAtIndex:i];
                    break;
                }
            }
        }
    }
}

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    [self addOrEditTouches:touches];
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    [self addOrEditTouches:touches];
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    [self deleteTouches:touches];
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [self deleteTouches:touches];
}

@end
