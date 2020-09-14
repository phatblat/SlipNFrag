//
//  sys_ios.h
//  SlipNFrag-iOS
//
//  Created by Heriberto Delgado on 9/13/20.
//  Copyright © 2020 Heriberto Delgado. All rights reserved.
//

#pragma once

#include <string>

extern int sys_argc;
extern char** sys_argv;
extern float frame_lapse;
extern std::string sys_errormessage;
extern int sys_nogamedata;

void Sys_Init(int argc, char** argv);
void Sys_Frame(float frame_lapse);
