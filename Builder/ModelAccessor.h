#pragma once
#include <functional>
#include <memory>

namespace xcel
{

class LightAccessor;
class PartAccessor;

class ModelAccessor
{
public:
    virtual ~ModelAccessor() = default;

    virtual void VisitLight(std::function<void(std::shared_ptr<LightAccessor>)>& visitor) = 0;

    virtual void VisitPart(std::function<void(std::shared_ptr<PartAccessor>)>& visitor) = 0;
};

}
