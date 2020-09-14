//
//  Shaders.metal
//  SlipNFrag-MacOS
//
//  Created by Heriberto Delgado on 5/29/19.
//  Copyright © 2019 Heriberto Delgado. All rights reserved.
//

#include <metal_stdlib>

using namespace metal;

struct VertexIn
{
    float4 position [[attribute(0)]];
    float2 texCoords [[attribute(1)]];
};

struct VertexOut
{
    float4 position [[position]];
    float2 texCoords;
};

vertex VertexOut vertexMain(VertexIn inVertex [[stage_in]])
{
    VertexOut outVertex;
    outVertex.position = inVertex.position;
    outVertex.texCoords = inVertex.texCoords;
    return outVertex;
}

fragment half4 fragmentMain(VertexOut input [[stage_in]], texture2d<half> screenTexture [[texture(0)]], texture2d<half> consoleTexture [[texture(1)]], texture1d<float> paletteTexture [[texture(2)]], sampler screenSampler [[sampler(0)]], sampler consoleSampler [[sampler(1)]], sampler paletteSampler [[sampler(2)]])
{
    half consoleEntry = consoleTexture.sample(consoleSampler, input.texCoords)[0];
    if (consoleEntry < 1)
    {
        return half4(paletteTexture.sample(paletteSampler, consoleEntry));
    }
    else
    {
        half screenEntry = screenTexture.sample(screenSampler, input.texCoords)[0];
        return half4(paletteTexture.sample(paletteSampler, screenEntry));
    }
}
