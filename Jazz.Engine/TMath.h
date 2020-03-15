#pragma once

#include "Types.h"

namespace Jazz {
	
	class TMath {
	public:
		static U32 ClampU32(const U32 value, const U32 min, const U32 max);
	};

	U32 TMath::ClampU32(const U32 value, const U32 min, const U32 max) {
		if (value < min) {
			return min;
		}
		if (value > max) {
			return max;
		}
		return value;
	}
}