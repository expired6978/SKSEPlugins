#pragma once

#include "skse/GameTypes.h"

namespace std {
	template <> struct hash < BSFixedString >
	{
		size_t operator()(const BSFixedString & x) const
		{
			return hash<const char*>()(x.data);
		}
	};
}