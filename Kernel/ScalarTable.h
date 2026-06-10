#pragma once
#include <cstddef>
#include <functional>
#include <vector>

namespace xcel {

class ScalarTable
{
public:
    virtual ~ScalarTable() = default;

    virtual size_t Size()               const = 0;
    virtual float  operator[](size_t i) const = 0;
    virtual float  MinValue()           const = 0;
    virtual float  MaxValue()           const = 0;

protected:
    ScalarTable() = default;
    ScalarTable(const ScalarTable&)            = delete;
    ScalarTable& operator=(const ScalarTable&) = delete;
};

class ElementScalarTable : public ScalarTable
{
public:
    ElementScalarTable() = default;
    explicit ElementScalarTable(std::vector<float> scalars);

    void AddScalar(float v);
    const std::vector<float>& Data() const;

    size_t Size()               const override;
    float  operator[](size_t i) const override;
    float  MinValue()           const override;
    float  MaxValue()           const override;

private:
    std::vector<float> m_scalars;
};

class ConstantScalarTable : public ScalarTable
{
public:
    ConstantScalarTable(float value, size_t count);

    size_t Size()               const override;
    float  operator[](size_t i) const override;
    float  MinValue()           const override;
    float  MaxValue()           const override;

private:
    float  m_value = 0.f;
    size_t m_count = 0;
};

class ComputedScalarTable : public ScalarTable
{
public:
    template<typename Fn>
        requires std::invocable<Fn, size_t> &&
                 std::same_as<std::invoke_result_t<Fn, size_t>, float>
    ComputedScalarTable(size_t count, Fn&& fn)
        : m_count(count), m_fn(std::forward<Fn>(fn)) {}

    size_t Size()               const override;
    float  operator[](size_t i) const override;
    float  MinValue()           const override;
    float  MaxValue()           const override;

private:
    size_t                       m_count = 0;
    std::function<float(size_t)> m_fn;
    mutable bool                 m_rangeCached = false;
    mutable float                m_cachedMin   = 0.f;
    mutable float                m_cachedMax   = 1.f;
};

} // namespace xcel
