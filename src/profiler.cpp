#include "profiler.h"

// Global profiler
Profiler main_profiler;
Profiler *g_profiler = &main_profiler;
bool g_profiler_enabled;
