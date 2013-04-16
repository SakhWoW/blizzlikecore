/*
 * Copyright (C) 2013  BlizzLikeGroup
 * BlizzLikeCore integrates as part of this file: CREDITS.md and LICENSE.md
 */

#include <cstdlib>
#include "ObjectLifeTime.h"

namespace BlizzLike
{
    extern "C" void external_wrapper(void *p)
    {
        std::atexit( (void (*)())p );
    }

    void at_exit( void (*func)() )
    {
        external_wrapper((void*)func);
    }
}

