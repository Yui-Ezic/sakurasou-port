// Scene constructors of the simulation module, one file per job.
#pragma once
#include "engine.h"
#include <memory>
std::unique_ptr<Job> makeChoice();    // 8
std::unique_ptr<Job> makeAction();    // 9
std::unique_ptr<Job> makeChart();     // 0xa
std::unique_ptr<Job> makeCalendar();  // 0xc
std::unique_ptr<Job> makeTaskList();  // 0xd
std::unique_ptr<Job> makeContest();   // 0xe
