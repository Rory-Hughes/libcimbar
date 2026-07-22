/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#pragma once

#include "wirehair/wirehair.h"

namespace FountainInit {
	// An inline function's local static is shared across translation units.
	// Internal linkage here would create one guard per translation unit and
	// allow concurrent calls into Wirehair's process-global initializer.
	inline bool init()
	{
		static WirehairResult res = wirehair_init();
		return !res;
	}
}
