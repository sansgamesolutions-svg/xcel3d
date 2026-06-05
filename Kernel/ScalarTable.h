#pragma once
#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

namespace xcel {

// ============================================================================
// ScalarTable — abstract base for per-element scalar field strategies
// ============================================================================
//
// The tessellator reads one float per element via operator[](i), normalises it
// against [MinValue, MaxValue], and maps it through a ColorTable to produce
// the per-vertex baked color.
//
// Subclasses:
//   ElementScalarTable   — one float per element stored in a vector (original behaviour)
//   ConstantScalarTable  — all elements return the same fixed value
//   ComputedScalarTable  — scalar computed on demand from a functor: float(size_t)
// ============================================================================
class ScalarTable {
public:
    virtual ~ScalarTable() = default;

    virtual size_t Size()            const = 0;
    virtual float  operator[](size_t i) const = 0;
    virtual float  MinValue()        const = 0;
    virtual float  MaxValue()        const = 0;

protected:
    ScalarTable() = default;
    ScalarTable(const ScalarTable&)            = delete;
    ScalarTable& operator=(const ScalarTable&) = delete;
};

// --- ElementScalarTable ------------------------------------------------------
// One float per element stored in a contiguous vector.
// This is the original ScalarTable behaviour.
class ElementScalarTable : public ScalarTable {
public:
    ElementScalarTable();
    explicit ElementScalarTable(std::vector<float> scalars);
    ~ElementScalarTable() override;

    void AddScalar(float v);
    const std::vector<float>& Data() const;

    size_t Size()               const override;
    float  operator[](size_t i) const override;
    float  MinValue()           const override;
    float  MaxValue()           const override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// --- ConstantScalarTable -----------------------------------------------------
// Every element returns the same fixed scalar value.
class ConstantScalarTable : public ScalarTable {
public:
    ConstantScalarTable(float value, size_t count);
    ~ConstantScalarTable() override;

    size_t Size()               const override;
    float  operator[](size_t i) const override;
    float  MinValue()           const override;
    float  MaxValue()           const override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// --- ComputedScalarTable -----------------------------------------------------
// Scalar computed on demand via a stored functor: float(size_t elementIndex).
// MinValue/MaxValue scan all elements on first call and cache the result.
//
// The templated constructor accepts any callable matching float(size_t), then
// type-erases it to std::function inside MakeImpl (defined in the .cpp),
// keeping the Impl struct out of this header.
class ComputedScalarTable : public ScalarTable {
public:
    template<typename Fn>
        requires std::invocable<Fn, size_t> &&
                 std::same_as<std::invoke_result_t<Fn, size_t>, float>
    ComputedScalarTable(size_t count, Fn&& fn)
        : m_impl(MakeImpl(count, std::function<float(size_t)>(std::forward<Fn>(fn)))) {}

    ~ComputedScalarTable() override;

    size_t Size()               const override;
    float  operator[](size_t i) const override;
    float  MinValue()           const override;
    float  MaxValue()           const override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    static std::unique_ptr<Impl> MakeImpl(size_t count,
                                          std::function<float(size_t)> fn);
};

} // namespace xcel
