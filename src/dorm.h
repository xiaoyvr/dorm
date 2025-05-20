#pragma once

#include <memory>
#include <string>
#include <type_traits>
#include <typeindex>
#include <map>
#include <any>
#include <typeinfo>
#include "field_type.h"
#include "entity_map.h"

namespace dorm {

    struct Session;

    class Database
    {
    protected:
        std::map<std::type_index, std::unique_ptr<EntityMapBase>> entityMaps;
        virtual ~Database() = default;
        virtual void initialize() {};
    public:
        virtual std::unique_ptr<DbRecord> load(const std::any& id, const std::type_info& type_info) = 0;
        virtual std::unique_ptr<DbRecord> create(const std::type_info& type_info) = 0;
        virtual void save(DbRecord* record, const std::type_info& type) = 0;
        virtual std::unique_ptr<Session> createSession() = 0;

        std::vector<std::unique_ptr<FieldTypeBase>> SupportedFieldTypes;
        Database() {
            SupportedFieldTypes.push_back(std::make_unique<FieldType<int>>());
            SupportedFieldTypes.push_back(std::make_unique<FieldType<std::string>>());
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
            auto& map = _db->getEntityMap<T>();
            std::unique_ptr<DbRecord> record = _db->load(id, typeid(T));
            if (!record) {
                return nullptr;
            }
            return map.create(record.get());
        }

        template<typename T>
        void save(T& entity) {
            auto& map = _db->getEntityMap<T>();
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
