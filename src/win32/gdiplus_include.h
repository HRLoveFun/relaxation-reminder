#pragma once

// GDI+ headers from the Windows SDK rely on min/max macros and on OLE/Propidl
// declarations that WIN32_LEAN_AND_MEAN leaves out. Provide them only while
// including gdiplus.h; the rest of the project uses std::min/std::max.
#include <windows.h>

#include <objidl.h>
#include <propidl.h>

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

#pragma warning(push)
#pragma warning(disable : 4458)
#include <gdiplus.h>
#pragma warning(pop)

#undef min
#undef max
