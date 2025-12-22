#pragma once

#include <random>
#include <unordered_map>

namespace AstralEngine {

	class UUID
	{
	public:
		UUID()
			: m_UUID(s_UniformDistribution(s_Engine)) {}

		UUID(uint64_t uuid)
			: m_UUID(uuid) {}

		operator uint64_t() const { return m_UUID; }

	private:
		uint64_t m_UUID;

		static std::random_device s_RandomDevice;
		static std::mt19937_64 s_Engine;
		static std::uniform_int_distribution<uint64_t> s_UniformDistribution;
	};

}

namespace std {
	template<>
	struct hash<AstralEngine::UUID>
	{
		std::size_t operator()(const AstralEngine::UUID& uuid) const
		{
			return hash<uint64_t>()((uint64_t)uuid);
		}
	};
}
