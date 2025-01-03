#include <gtest/gtest.h>
#include "dorm.h"
#include "in_mem.h"
#include <string> 

using namespace dorm;

class Person : public Entity<Person, int>
{
    std::string name;
    int age;

    Person() {}
public:
    Person(std::string name, int age) : name(name), age(age) {}
    std::string getName() const { return name; }
    int getAge() const { return age; }    
    void setName(const std::string & name) { this-> name = name; }

    friend class PersonMap;
    friend class EntityMap<Person>;
};


class PersonMap : public EntityMap<Person>
{
    public:
    PersonMap() : EntityMap<Person>("person") {
        mapField<std::string>("name", &Person::setName);
        mapField<std::string>("name", [](Person p, const std::string& name) { p.name = name; });
    }
};

TEST(DormTest, EntityTest)
{
    in_mem::InMemDatabase db;
    db.configure<PersonMap>();

    db.init();

    auto session = db.createSession();
    auto p = session->load<Person>(1);

    ASSERT_FALSE(p == nullptr);
}