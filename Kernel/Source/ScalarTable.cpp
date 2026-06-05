#include "Kernel/ScalarTable.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace xcel {

// ============================================================================
// ElementScalarTable
// ============================================================================

struct ElementScalarTable::Impl {
    std::vector<float> scalars;
};

ElementScalarTable::ElementScalarTable()
    : m_impl(std::make_unique<Impl>()) {}

ElementScalarTable::ElementScalarTable(std::vector<float> scalars)
    : m_impl(std::make_unique<Impl>())
{
    m_impl->scalars = std::move(scalars);
}

ElementScalarTable::~ElementScalarTable() = default;

void ElementScalarTable::AddScalar(float v) { m_impl->scalars.push_back(v); }

const std::vector<float>& ElementScalarTable::Data() const { return m_impl->scalars; }

size_t ElementScalarTable::Size()               const { return m_impl->scalars.size(); }
float  ElementScalarTable::operator[](size_t i) const { return m_impl->scalars[i]; }

float ElementScalarTable::MinValue() const
{
    if (m_impl->scalars.empty()) return 0.f;
    return *std::min_element(m_impl->scalars.begin(), m_impl->scalars.end());
}

float ElementScalarTable::MaxValue() const
{
    if (m_impl->scalars.empty()) return 1.f;
    return *std::max_element(m_impl->scalars.begin(), m_impl->scalars.end());
}

// ============================================================================
// ConstantScalarTable
// ============================================================================

struct ConstantScalarTable::Impl {
    float  value;
    size_t count;
};

ConstantScalarTable::ConstantScalarTable(float value, size_t count)
    : m_impl(std::make_unique<Impl>())
{
    m_impl->value = value;
    m_impl->count = count;
}

ConstantScalarTable::~ConstantScalarTable() = default;

size_t ConstantScalarTable::Size()               const { return m_impl->count; }
float  ConstantScalarTable::operator[](size_t /*i*/) const { return m_impl->value; }
float  ConstantScalarTable::MinValue()           const { return m_impl->value; }
float  ConstantScalarTable::MaxValue()           const { return m_impl->value; }

// ============================================================================
// ComputedScalarTable
// ============================================================================

struct ComputedScalarTable::Impl {
    size_t                       count;
    std::function<float(size_t)> fn;
    mutable bool  rangeCached = false;
    mutable float cachedMin   = 0.f;
    mutable float cachedMax   = 1.f;
};

std::unique_ptr<ComputedScalarTable::Impl>
ComputedScalarTable::MakeImpl(size_t count, std::function<float(size_t)> fn)
{
    auto impl  = std::make_unique<Impl>();
    impl->count = count;
    impl->fn    = std::move(fn);
    return impl;
}

ComputedScalarTable::~ComputedScalarTable() = default;

size_t ComputedScalarTable::Size()               const { return m_impl->count; }
float  ComputedScalarTable::operator[](size_t i) const { return m_impl->fn(i); }

float ComputedScalarTable::MinValue() const
{
    if (!m_impl->rangeCached) {
        float lo = std::numeric_limits<float>::max();
        float hi = std::numeric_limits<float>::lowest();
        for (size_t i = 0; i < m_impl->count; ++i) {
            float v = m_impl->fn(i);
            if (v < lo) lo = v;
            if (v > hi) hi = v;
        }
        m_impl->cachedMin   = (m_impl->count > 0) ? lo : 0.f;
        m_impl->cachedMax   = (m_impl->count > 0) ? hi : 1.f;
        m_impl->rangeCached = true;
    }
    return m_impl->cachedMin;
}

float ComputedScalarTable::MaxValue() const
{
    MinValue(); // ensures cache is populated
    return m_impl->cachedMax;
}

} // namespace xcel
