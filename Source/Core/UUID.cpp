#include "UUID.h"

namespace AstralEngine {

	std::random_device UUID::s_RandomDevice;
	std::mt19937_64 UUID::s_Engine(s_RandomDevice());
	std::uniform_int_distribution<uint64_t> UUID::s_UniformDistribution;

}
