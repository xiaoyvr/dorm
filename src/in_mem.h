#pragma once

#include "dorm.h"

namespace dorm::in_mem {

    class InMemRecord : public DbRecord
    {
    public:
        std::any get(const std::string& columnName) override {
            return std::any();
        }
    };


    class InMemDatabase : public Database
    {
        public:
        std::unique_ptr<Session> createSession() override {
            return std::make_unique<Session>(this);
        }

        std::unique_ptr<DbRecord> loadRecord(const std::any& id, const std::type_info& type_info) override {
            return std::make_unique<InMemRecord>();
        }

        void init() {
            // create inmem tables based on the registed entity maps
            
        }
    }; 
}