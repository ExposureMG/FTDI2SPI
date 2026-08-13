#pragma once

#ifdef _WIN32
#include <synchapi.h>
#include <windows.h>
#else
#include <condition_variable>
#include <mutex>
#include "stdafx.h"
// Define aliases or light wrapper classes for Linux
#endif
