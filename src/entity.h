#include <type_traits>

namespace dorm {

    template <typename T, typename TID>
    class Entity
    {
    public:
        using id_t = TID;
        TID id() const { return _id; }
        void id(TID pid) { _id = pid; }
    protected:
        TID _id = TID();
    };

    template <typename T>
    class ValueObject
    {
        virtual bool operator==(const T& rhs) const = 0;
        bool operator!=(const T& rhs) const {
            return !(*this == rhs);
        }
    protected:
        ValueObject() {
            static_assert(std::is_copy_constructible<T>::value, "Derived class must define a copy constructor");
            static_assert(std::is_copy_assignable<T>::value, "Derived class must define a copy assignment operator");
        }
    };
}
