#pragma once
#include <functional>
#include <memory>

namespace xcel
{

class LightAccessor;
class MaterialAccessor;
class MeshAccessor;
class SkeletonAccessor;

class PartAccessor
{
public:
    virtual ~PartAccessor() = default;

    virtual void VisitLight(std::function<void(std::shared_ptr<LightAccessor>)>& visitor) = 0;

    virtual std::shared_ptr<MaterialAccessor> GetMaterial() const = 0;

    virtual std::shared_ptr<MeshAccessor> GetMesh() const = 0;

    virtual std::shared_ptr<SkeletonAccessor> GetSkeleton() const = 0;
};

}
