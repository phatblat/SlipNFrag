//
//  vid_macos.h
//  SlipNFrag-MacOS
//
//  Created by Heriberto Delgado on 5/29/19.
//  Copyright © 2019 Heriberto Delgado. All rights reserved.
//

#pragma once

#include "quakedef.h"

extern std::vector<unsigned char> vid_buffer;
extern int vid_width;
extern int vid_height;
extern unsigned d_8to24table[256];

void VID_Resize();
