#pragma once
#include <cstddef>
#include <memory>
#include <glm/glm.hpp>

namespace xcel {

// ============================================================================
// ColorTable — abstract base for element coloring strategies
// ============================================================================
//
// The single virtual method ColorForElement() receives everything a subtype
// could need: the element index (for MeshColor), the raw scalar value and its
// global min/max range (for PaletteColor), or nothing at all (for SingleColor).
//
// Subclasses:
//   SingleColor  — fixed color for every element; ignores all arguments
//   PaletteColor — 256-entry cool-to-warm ramp; scalar -> [0,1] -> RGB
//   MeshColor    — per-element stored glm::vec3 array; ignores scalar args
// ============================================================================
class ColorTable {
public:
    virtual ~ColorTable() = default;

    virtual glm::vec3 ColorForElement(size_t elem,
                                      float  value,
                                      float  minVal,
                                      float  maxVal) const = 0;

protected:
    ColorTable() = default;
    ColorTable(const ColorTable&)            = delete;
    ColorTable& operator=(const ColorTable&) = delete;
};

// --- SingleColor -------------------------------------------------------------
// Returns the same fixed color for every element regardless of scalar data.
class SingleColor : public ColorTable {
public:
    explicit SingleColor(glm::vec3 color);
    ~SingleColor() override;

    glm::vec3 ColorForElement(size_t elem,
                              float  value,
                              float  minVal,
                              float  maxVal) const override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// --- PaletteColor ------------------------------------------------------------
// 256-entry cool-to-warm ramp (blue->cyan->green->yellow->red).
// Normalises scalar to [0,1] against [minVal,maxVal] and looks up the table.
// This is the original ColorTable behaviour.
class PaletteColor : public ColorTable {
public:
    PaletteColor();
    ~PaletteColor() override;

    glm::vec3 ColorForElement(size_t elem,
                              float  value,
                              float  minVal,
                              float  maxVal) const override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// --- MeshColor ---------------------------------------------------------------
// Per-element stored color array. ColorForElement(elem,...) returns colors[elem].
// Scalar arguments are ignored; only the element index is used.
class MeshColor : public ColorTable {
public:
    MeshColor();
    ~MeshColor() override;

    void AddColor(glm::vec3 color);
    void SetColor(size_t elem, glm::vec3 color);

    glm::vec3 ColorForElement(size_t elem,
                              float  value,
                              float  minVal,
                              float  maxVal) const override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace xcel
