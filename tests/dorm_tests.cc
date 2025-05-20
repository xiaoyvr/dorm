#include <string>
#include <gtest/gtest.h>
#include "dorm.h"
#include "entity.h"
#include "entity_map.h"
#include "in_mem.h"


using namespace dorm;

class Person : public Entity<Person, int>
{
    std::string _name;
    int _age;
    Person() {}
public:
    Person(std::string name, int age) : _name(name), _age(age) {}
    std::string name() const { return _name; }
    int age() const { return _age; }
    void name(const std::string & name) { _name = name; }

    friend class PersonMap;
    friend class EntityMap<Person>;
};


class PersonMap : public EntityMap<Person>
{
public:
    PersonMap() : EntityMap<Person>("person") {
        id("id", &Person::_id)->generated(true);
        field("name", &Person::_name);
        field("age", &Person::_age);
    }
};

TEST(DormTest, should_return_null_if_nothing_in_database)
{
    in_mem::InMemDatabase db;
    db.configure<PersonMap>();
    db.initialize();

    auto session = db.createSession();
    auto p = session->load<Person>(1);

    ASSERT_EQ(p, nullptr);
}

TEST(DormTest, should_return_if_entity_already_in_database)
{
    in_mem::InMemDatabase db;
    db.configure<PersonMap>();
    db.initialize();

    auto session = db.createSession();
    auto p = Person("John Doe", 30);
    session->save(p);

    auto p1 = session->load<Person>(p.id());

    ASSERT_NE(p1, nullptr);
    ASSERT_EQ(p1->age(), 30);
    ASSERT_EQ(p1->name(), "John Doe");
}

TEST(DormTest, should_return_generated_id)
{
    in_mem::InMemDatabase db;
    db.configure<PersonMap>();
    db.initialize();

    auto session = db.createSession();
    auto p = Person("John Doe", 30);
    session->save(p);

    ASSERT_NE(p.id(), 0);
}

// TEST(DormTest, should_store_two_record_in_database)
// {
//     in_mem::InMemDatabase db;
//     db.configure<PersonMap>();
//     db.initialize();

//     auto session = db.createSession();
//     auto p = Person("John Doe", 30);
//     session->save(p);

//     auto p1 = Person("John Smith", 35);
//     session->save(p1);

//     auto p11 = session->load<Person>(p1.id());

//     ASSERT_NE(p11, nullptr);
//     ASSERT_EQ(p11->age(), 30);
//     ASSERT_EQ(p11->name(), "John Doe");
// }
