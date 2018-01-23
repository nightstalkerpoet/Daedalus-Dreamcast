#include "windef.h"

UINT timeGetTime()
{
    return (UINT)timer_ms_gettime64();
}
