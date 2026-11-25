#pragma once
// Infernis: the build data the Level Editor writes for the compilers (build.prj and the rest) is read by Infernis's
// xrLC, xrDO_Light and xrAI with GSC's structures, so the editor uses the compilers' own definitions. IX-Ray's copy
// changed b_texture (an image object instead of GSC's surface pointer) and added a mesh LOD chunk Infernis's xrLC
// does not read.
#include "../../../utils/editors/LevelEditor/Engine/communicate.h"
