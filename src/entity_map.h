#pragma once

#include <functional>
#include <vector>
#include <tuple>
#include <typeindex>
#include <memory>
#include <iostream>
#include <any>
#include <string>

namespace dorm {

    struct DbRecord
    {
        virtual std::any get(const std::string& columnName) = 0;
        virtual void set(const std::string& columnName, const std::any& value) = 0;
        virtual ~DbRecord() = default;
    };


    template<typename T>
    struct ColumnConfig {
        ColumnConfig(const std::string& name, const std::type_index& type,
            std::function<std::any(const T&)> getter,
            std::function<void(T&, const std::any&)> setter):
            _name(name),
            _type(type),
            _getter(getter),
            _setter(setter),
            _isKey(false) {};
        virtual ~ColumnConfig() = default;
        template<typename> friend class EntityMap;
    protected:
        std::type_index _type;
        bool _isKey;
        std::string _name;
        std::function<std::any(const T&)> _getter;
        std::function<void(T&, const std::any&)> _setter;
    };
    template<typename T>
    struct IdColumnConfig : public ColumnConfig<T> {
        bool _generated;
        IdColumnConfig(const std::string& name, const std::type_index& type,
            std::function<std::any(const T&)> getter,
            std::function<void(T&, const std::any&)> setter)
            : ColumnConfig<T>(name, type, getter, setter) {
            this->_isKey = true;
        }

        IdColumnConfig<T>* generated(bool generated = true) {
            _generated = generated;
            return this;
        }
        bool generated() const { return _generated; }
    };

    struct EntityMapBase {
        std::string tableName() const { return _tableName; }
        virtual std::vector<std::tuple<std::string, std::type_index, bool, bool>> columns() const = 0;
        virtual ~EntityMapBase() = default;
    protected:
        EntityMapBase(const std::string& tableName) : _tableName(tableName) {}
        std::string _tableName;
    };

    template<typename T>
    class EntityMap : public EntityMapBase {

        EntityMap(const EntityMap& other) = delete;
        std::vector<std::unique_ptr<ColumnConfig<T>>> configs;

        template<typename TF>
        static const TF& cast_from_any(const std::any& v) {
            if ( v.type() == typeid(TF))
            {
                return std::any_cast<const TF&>(v);
            }
            std::cout << "Failed due to type mismatch, from " << typeid(v).name()
                << " to " << typeid(TF).name() << std::endl;
            throw std::bad_any_cast();
        }

    protected:
        EntityMap(const std::string& tableName) : EntityMapBase(tableName) {}

        IdColumnConfig<T>* id(const std::string& columnName, typename T::id_t T::*field) {
            auto pconfig = new IdColumnConfig<T>(
                columnName,
                typeid(typename T::id_t),
                [field](const T& t) {
                    return std::any(t.*field);
                },
                [field](T& t, const std::any& v) {
                    (t.*field) = cast_from_any<typename T::id_t>(v);
                });
            configs.emplace_back(std::unique_ptr<ColumnConfig<T>>(pconfig));
            return pconfig;
        }

        template<typename TF>
        ColumnConfig<T>* field(const std::string& columnName, TF T::*field) {
            auto pconfig = new ColumnConfig<T>(
                columnName,
                typeid(TF),
                [field](const T& t) {
                    return std::any(t.*field);
                },
                [field](T& t, const std::any& v) {
                    (t.*field) = cast_from_any<TF>(v);
                });
            configs.emplace_back(std::unique_ptr<ColumnConfig<T>>(pconfig));
            return pconfig;
        };

    public:
        using entity_t = T;
        std::unique_ptr<T> create(DbRecord* record) const {
            auto instance = std::unique_ptr<T>(new T());
            for (auto& c : configs) {
                c->_setter (*instance, record->get(c->_name));
            }
            return instance;
        }

        void update(entity_t& entity, DbRecord* record) const {
            for (auto& c : configs) {
                c->_setter (entity, record->get(c->_name));
            }
        }

        void fill(DbRecord* record, const entity_t& entity) const {
            for (auto& c : configs) {
                record->set(c->_name, c->_getter(entity));
            }
        }

        template<typename TPtr>
        struct MemberPtrTraits;

        template<typename TM>
        struct MemberPtrTraits<TM entity_t::*> {
            using member_t = TM;
        };

        std::vector<std::tuple<std::string, std::type_index, bool, bool>> columns() const override {
            std::vector<std::tuple<std::string, std::type_index, bool, bool>> result;
            for (auto& c : configs) {

                auto* pIdColumnConfig = dynamic_cast<IdColumnConfig<T>*>(c.get());
                if(pIdColumnConfig) {
                    result.push_back(std::make_tuple(pIdColumnConfig->_name, pIdColumnConfig->_type,
                        pIdColumnConfig->_isKey, pIdColumnConfig->_generated));
                } else {
                    result.push_back(std::make_tuple(c->_name, c->_type, c->_isKey, false));
                }
            }
            return result;
        };
    };

}
