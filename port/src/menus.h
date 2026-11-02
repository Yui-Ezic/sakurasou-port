// Menu module internals: scene factories of the split source files.
#pragma once
#include "engine.h"
std::unique_ptr<Job> createSaveLoadScene(int id);   // 0x1a-0x1c, 0x24-0x26
std::unique_ptr<Job> createOmakeScene(int id);      // 0x15-0x19
std::unique_ptr<Job> createMiscScene(int id);       // 0x13, 0x1d, 0x1e, 0x1f, 0x27
