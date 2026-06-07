#include "Graphics/ScalarTable.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace xcel {

// ============================================================================
// ElementScalarTable
// ============================================================================

ElementScalarTable::ElementScalarTable(std::vector<float> scalars)
    {
    m_scalars = std::move(scalars);
}

void ElementScalarTable::AddScalar(float v) { m_scalars.push_back(v); }

const std::vector<float>& ElementScalarTable::Data() const { return m_scalars; }

size_t ElementScalarTable::Size()               const { return m_scalars.size(); }
float  ElementScalarTable::operator[](size_t i) const { return m_scalars[i]; }

float ElementScalarTable::MinValue() const
{
    if (m_scalars.empty()) return 0.f;
    return *std::min_element(m_scalars.begin(), m_scalars.end());
}

float ElementScalarTable::MaxValue() const
{
    if (m_scalars.empty()) return 1.f;
    return *std::max_element(m_scalars.begin(), m_scalars.end());
}

// ============================================================================
// ConstantScalarTable
// ============================================================================

ConstantScalarTable::ConstantScalarTable(float value, size_t count)
    {
    m_value = value;
    m_count = count;
}

size_t ConstantScalarTable::Size()               const { return m_count; }
float  ConstantScalarTable::operator[](size_t /*i*/) const { return m_value; }
float  ConstantScalarTable::MinValue()           const { return m_value; }
float  ConstantScalarTable::MaxValue()           const { return m_value; }

// ============================================================================
// ComputedScalarTable
// ============================================================================

size_t ComputedScalarTable::Size()               const { return m_count; }
float  ComputedScalarTable::operator[](size_t i) const { return m_fn(i); }

float ComputedScalarTable::MinValue() const
{
    if (!m_rangeCached) {
        float lo = std::numeric_limits<float>::max();
        float hi = std::numeric_limits<float>::lowest();
        for (size_t i = 0; i < m_count; ++i) {
            float v = m_fn(i);
            if (v < lo) lo = v;
            if (v > hi) hi = v;
        }
        m_cachedMin   = (m_count > 0) ? lo : 0.f;
        m_cachedMax   = (m_count > 0) ? hi : 1.f;
        m_rangeCached = true;
    }
    return m_cachedMin;
}

float ComputedScalarTable::MaxValue() const
{
    MinValue(); // ensures cache is populated
    return m_cachedMax;
}

} // namespace xcel
