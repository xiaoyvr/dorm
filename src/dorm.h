#pragma once

#include <memory>
#include <any>
#include <typeindex>
#include <map>

namespace dorm {

    template <typename T, typename TID>
    class Entity
    {
    public:
        using id_t = TID;
        TID id() const { return _id; }
        void id(TID pid) { _id = pid; }
    protected:
        TID _id;
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

    struct DbRecord
    {        
        virtual std::any get(const std::string& columnName) = 0;
        virtual void set(const std::string& columnName, std::any&& value) = 0;
        virtual ~DbRecord() = default;
    };

    struct Session;

    class EntityMapBase;
    template<typename T> class EntityMap;

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
    

    class Database
    {
    protected:
        std::map<std::type_index, std::unique_ptr<EntityMapBase>> entityMaps;
        virtual ~Database() = default;
    public:
        virtual std::unique_ptr<DbRecord> load(const std::any& id, const std::type_info& type_info) = 0;
        virtual std::unique_ptr<DbRecord> create(const std::type_info& type_info) = 0;
        virtual void save(DbRecord* record, const std::type_info& type) = 0;
        virtual std::unique_ptr<Session> createSession() = 0;
        virtual void doInit() {};

        std::vector<std::unique_ptr<FieldTypeBase>> SupportedFieldTypes;
        Database() {
            SupportedFieldTypes.push_back(std::make_unique<FieldType<int>>());
            SupportedFieldTypes.push_back(std::make_unique<FieldType<std::string>>());
            doInit();
        };

        template<typename T>
        void configure() {
            static_assert(std::is_base_of<EntityMap<typename T::entity_t>, T>::value, "T must be derived from EntityMap");
            entityMaps.try_emplace(typeid(typename T::entity_t), std::make_unique<T>());
        }

        template<typename T>
        const EntityMap<T>& getEntityMap() const {
            const EntityMapBase& m = *entityMaps.at(typeid(T));
            return static_cast<const EntityMap<T>&>(m);
        }
    };


    struct Session
    {
    private:
        Database* _db;
    public:
        Session(Database* db) : _db(db) {}

        template<typename T>
        std::unique_ptr<T> load(typename T::id_t id) {
            auto map = _db->getEntityMap<T>();
            std::unique_ptr<DbRecord> record = _db->load(id, typeid(T));
            if (!record) {
                return nullptr;
            }
            return map.create(record.get());
        }

        template<typename T>
        void save(T& entity) {
            auto map = _db->getEntityMap<T>();
            std::unique_ptr<DbRecord> record = _db->create(typeid(T));
            map.fill(record.get(), entity);
            _db->save(record.get(), typeid(T));
        }
    };


    template <typename T>
    struct Repository
    {
        Repository(const Session& session) : session(session) {}

        std::unique_ptr<T> load(typename T::id_t id) {
            return session.load<T>(id);
        }
        virtual void save(T& entity) = 0;
        virtual void del(typename T::id_t id) = 0;
        // virtual QueryResult<T> query(const QueryClause&) = 0;
    private: 
        const Session& session;
    };


    struct EntityMapBase {
        std::string tableName() const { return _tableName; }
        virtual std::vector<std::tuple<std::string, std::type_index>> columns() const = 0;
        virtual ~EntityMapBase() = default;
    protected:
        EntityMapBase(const std::string& tableName) : _tableName(tableName) {}
        std::string _tableName;        
    };

    template<typename T>
    class EntityMap : public EntityMapBase {
        std::map<std::string, std::tuple<
            std::type_index, 
            std::function<void(T&, const std::any&)>, 
            std::function<std::any(const T&)>
        >> accessors;

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

        void id(const std::string& columnName, typename T::id_t T::*idField) {
            field(columnName, idField);
        }

        template<typename TF>
        void field(const std::string& columnName, TF T::*field) {
            accessors.try_emplace(columnName, std::make_tuple(
                std::type_index(typeid(TF)),
                [field](T& t, const std::any& v) { 
                    (t.*field) = cast_from_any<TF>(v); 
                }, 
                [field](const T& t) -> std::any { 
                    return std::any(t.*field); 
                }));
        };

    public:
        using entity_t = T;
        std::unique_ptr<T> create(DbRecord* record) {
            auto instance = std::unique_ptr<T>(new T());
            for (auto& [columnName, accessor] : accessors) {
                auto [_, setter, __] = accessor;
                setter (*instance, record->get(columnName));
            }
            return instance;
        }

        void fill(DbRecord* record, const entity_t& entity) {
            for (auto& [columnName, accessor] : accessors) {
                auto [_, __, getter] = accessor;
                record->set(columnName, getter(entity));
            }
        }

        template<typename TPtr>
        struct MemberPtrTraits;
        
        template<typename TM>
        struct MemberPtrTraits<TM entity_t::*> {
            using member_t = TM;
        };

        std::vector<std::tuple<std::string, std::type_index>> columns() const override {
            std::vector<std::tuple<std::string, std::type_index>> result;
            for (auto& [columnName, accessor] : accessors) {
                result.push_back(std::make_tuple(columnName, std::get<0>(accessor)));
            }
            return result;
        };
    };

}


/*
client side manage the lifecycle -> return unique_ptr to the client side
second call will return a new entity, reconstruct from database record, database record could be cached in one session
std::unique_ptr<Obj> obj = repo->load(1).as_unique();

orm manage the lifecycle -> return shared_ptr to the client side
second call will return the same object
std::shared_ptr<Obj> obj = repo->load(1).as_shared();


second call will return a new object
Obj obj = repo->load(1).as_copy();

decision, only support unique_ptr, client side manage the lifecycle.
Entity level cache can be implement at the repository level like CachedRepository.


repo->save(Obj& obj)
flush data to the database, and refresh the object with the new data in db.

query:
Iterable<Person> result = repo->query(where("name").eq("John").and("age").gt(20))
Db record can be cached, but do we need it being cached? this could return many records, high chance of update conflict.

projection:
Iterable<PersonWithNameOnly> result = repo->query(where("age").gt(20)).select("name");
No Cache.

database -> session -> repo
database creates sessions, session creates repos, repo is stateless, can be created or dropped anytime.
session store the L1 cache for database record, to save the databse trip
session should be short lived, create new if needed.
session will be destroyed when the scope ends, along with the cache.

*/

