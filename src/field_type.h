#pragma once

#include <any>
#include <typeindex>

namespace dorm {
    class FieldTypeBase {
    public:
        FieldTypeBase(const std::type_index& type) : _type(type) {}
        std::type_index type() const { return _type; }
        virtual bool equal(const std::any& lhs, const std::any& rhs) const = 0;
        virtual ~FieldTypeBase() = default;
    private:
        std::type_index _type;
    };

    template<typename T>
    class FieldType: public FieldTypeBase {
    public:
        FieldType() : FieldTypeBase(typeid(T)) {}
        bool equal(const std::any& lhs, const std::any& rhs) const override {
            return (!lhs.has_value() && !rhs.has_value()) || 
                (lhs.type() == typeid(T) && rhs.type() == typeid(T) 
                    && std::any_cast<const T&>(lhs) == std::any_cast<const T&>(rhs));
        }
    };   
}