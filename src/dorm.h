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
    protected:
        TID id;
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
        virtual ~DbRecord() = default;
    };

    struct Session;
    class EntityMapBase;
    template<typename T> class EntityMap;

    class Database
    {
    protected:
        std::map<std::type_index, std::unique_ptr<EntityMapBase>> entityMaps;
    public:
        virtual std::unique_ptr<DbRecord> loadRecord(const std::any& id, const std::type_info& type_info) = 0;
        virtual std::unique_ptr<Session> createSession() = 0;

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
        Database* database;
    public:
        Session(Database* database) : database(database) {}

        template<typename T>
        std::unique_ptr<T> load(typename T::id_t id) {
            auto map = database->getEntityMap<T>();
            std::unique_ptr<DbRecord> record = database->loadRecord(id, typeid(T));
            return map.create(record.get());
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


    struct EntityMapBase {};

    template<typename T>
    class EntityMap : public EntityMapBase {
        std::string tableName;
        std::map<std::string, std::function<void(T&, const std::any&)>> setters;

        template<typename TF>
        static const TF& cast_from_any(const std::any& v) {
            if ( v.type() == typeid(TF))
            {
                return std::any_cast<const TF&>(v); 
            }
            std::cout << "setter failed due to type mismatch, from " << typeid(v).name() 
                << " to " << typeid(TF).name() << std::endl;
            throw std::bad_any_cast();
        }

    protected:
        EntityMap(const std::string& tableName) : tableName(tableName) {}

        template<typename TF>
        void mapField(const std::string& columnName, void (T::*setter)(const TF& value)) {
            setters.try_emplace(columnName, [setter](T& t, const std::any& v) { 
                (t.*setter)(cast_from_any<TF>(v)); 
            });
            
        };

        template<typename TF>
        void mapField(const std::string& columnName, std::function<void(T&, const TF&)> setter) {
            setters.try_emplace(columnName, [setter](T& t, const std::any& v) { 
                setter(t, cast_from_any<TF>(v)); 
            });
        };
    public:
        using entity_t = T;
        std::string getTableName() const { return tableName; }
        std::unique_ptr<T> create(DbRecord* record) {
            auto instance = std::unique_ptr<T>(new T());
            for (auto& [columnName, setter] : setters) {
                setter(*instance, record->get(columnName));
            }
            return instance;
        }
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

