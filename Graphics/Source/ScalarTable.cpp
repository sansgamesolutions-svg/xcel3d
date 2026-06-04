#include "Graphics/ScalarTable.h"
#include <algorithm>
#include <limits>

namespace xcel {

struct ScalarTable::Impl {
    std::vector<float> scalars;
};

ScalarTable::ScalarTable()
    : m_impl(std::make_unique<Impl>()) {}

ScalarTable::ScalarTable(std::vector<float> scalars)
    : m_impl(std::make_unique<Impl>())
{
    m_impl->scalars = std::move(scalars);
}

ScalarTable::~ScalarTable() = default;

void   ScalarTable::AddScalar(float v)         { m_impl->scalars.push_back(v); }
size_t ScalarTable::Size() const               { return m_impl->scalars.size(); }
float  ScalarTable::operator[](size_t i) const { return m_impl->scalars[i]; }
const std::vector<float>& ScalarTable::Data() const { return m_impl->scalars; }

float ScalarTable::MinValue() const {
    if (m_impl->scalars.empty()) return 0.f;
    return *std::min_element(m_impl->scalars.begin(), m_impl->scalars.end());
}

float ScalarTable::MaxValue() const {
    if (m_impl->scalars.empty()) return 1.f;
    return *std::max_element(m_impl->scalars.begin(), m_impl->scalars.end());
}

} // namespace xcel
