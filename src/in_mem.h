#pragma once

#include "dorm.h"

namespace dorm::in_mem {

    class InMemRecord : public DbRecord
    {
        std::map<std::string, std::any> _values;
    public:
        void set(const std::string& columnName, const std::any& value) override {
            _values.try_emplace(columnName, value);
        }

        std::any get(const std::string& columnName) override {
            return _values.at(columnName);
        }
    };

    struct Column {
        std::string name;
        std::type_index type;
        bool isKey;
    };

    class Table {
    public:
        using row_t = std::vector<std::any>;

        Table(const std::string& name, Database* db) : _name(name), _db(db) {}
        const std::string& name() const { return _name; }

        void addColumn(const std::string& columnName, std::type_index fieldType, bool isKey=false) {
            _columns.push_back( {columnName, fieldType, isKey} );
        }

        const std::vector<Column>& columns() const { return _columns; }

        std::optional<std::reference_wrapper<row_t>> get(const std::any& id) {
            
            auto it = std::find_if(_rows.begin(), _rows.end(), [&](const auto& row) {
                auto keys = getKeysWithColumn(row);
                if (keys.size() == 1)
                {
                    const auto& k = keys[0];
                    const auto& column = std::get<0>(keys[0]);
                    const std::any& v = std::get<1>(keys[0]);
                    return GetSupportedFieldType(std::get<0>(keys[0])->type)
                        ->equal(*std::get<1>(keys[0]), id);
                }
                throw std::runtime_error("Composite keys not supported yet");
            });
            if (it == _rows.end()) {

                return std::nullopt;
            }
            return *it;
        }

        void upsert(row_t&& values) {
            auto it = std::find_if(_rows.begin(), _rows.end(), [&](const auto& row) {
                auto rowKeys = getKeys(row);
                auto valueKeys = getKeys(values);
                return std::equal(rowKeys.begin(), rowKeys.end(), valueKeys.begin(), 
                    [this](const auto& lhs, const auto& rhs) {
                        auto it = std::find_if(_db->SupportedFieldTypes.begin(), _db->SupportedFieldTypes.end(), [&](const auto& ft) {
                            return ft->type() == lhs.type();
                        });
                        if (it == _db->SupportedFieldTypes.end())
                        {
                            throw std::runtime_error(std::string("Unsupported field type ") + lhs.type().name());
                        }
                        return (*it)->equal(lhs, rhs);
                    });
            });

            if (it != _rows.end()) {
                *it = std::move(values);
            } else {
                _rows.push_back(std::move(values));
            }
        }
    private:
        std::string _name;
        Database* _db;
        std::vector<Column> _columns;
        std::vector<row_t> _rows; // rows should be sorted by id column

        std::vector<std::any> getKeys(const row_t& row) {
            std::vector<std::any> keys;
            for (size_t i = 0; i < _columns.size(); i++)
            {
                if (_columns[i].isKey) {
                    keys.push_back(row[i]);
                }
            }
            return keys;
        }

        std::vector<std::tuple<const Column*, const std::any*>> getKeysWithColumn(const row_t& row) {
            std::vector<std::tuple<const Column*, const std::any*>> pk;
            for (size_t i = 0; i < _columns.size(); i++)
            {
                if (_columns[i].isKey) {
                    pk.push_back(std::make_tuple(&_columns[i], &row[i]));
                }
            }
            return pk;
        }

        FieldTypeBase* GetSupportedFieldType(std::type_index type) {
            auto it = std::find_if(_db->SupportedFieldTypes.begin(), _db->SupportedFieldTypes.end(), [&](const auto& ft) {
                return ft->type() == type;
            });
            if (it == _db->SupportedFieldTypes.end())
            {
                throw std::runtime_error(std::string("Unsupported field type ") + type.name());
            }
            return (*it).get();
        }
    };

    class InMemDatabase : public Database
    {
        std::vector<std::unique_ptr<Table>> tables;

        Table* getTable(const std::string& name){
            auto it = std::find_if(tables.begin(), tables.end(), [&](const auto& t) {
                return t->name() == name;
            });

            if (it == tables.end()) {
                throw std::runtime_error("Table not found " + name);
            }
            return (*it).get();
        }

    public:
        virtual ~InMemDatabase() = default;
        std::unique_ptr<Session> createSession() override {
            return std::make_unique<Session>(this);
        }

        std::unique_ptr<DbRecord> load(const std::any& id, const std::type_info& type) override {
            auto& map = entityMaps[type];
            auto ptable = getTable(map->tableName());
            auto optRow = ptable->get(id);
            
            if (optRow)
            {
                
                auto result = std::make_unique<InMemRecord>();
                auto& row = optRow->get();
                for(auto i = 0; i < ptable->columns().size() ; i++) {
                    result->set(ptable->columns()[i].name, row[i]);
                }
                return result;
            }
            return nullptr;
        }

        std::unique_ptr<DbRecord> create(const std::type_info& type_info) override {
            return std::make_unique<InMemRecord>();
        }
        
        void save(DbRecord* pRecord, const std::type_info& type) override {
            auto& map = entityMaps[type];
            auto ptable = getTable(map->tableName());
            Table::row_t values;
            for (auto& [columnName, _, __] : map->columns()) {
                values.push_back(std::forward<std::any>(pRecord->get(columnName)));
            } 
            ptable->upsert(std::move(values));
        }


        void initialize() override {
            for (auto& [t, map] : entityMaps) {
                auto ptable = std::make_unique<Table>(map->tableName(), this);
                for (auto& [columnName, fieldType, isKey] : map->columns()) {
                    ptable->addColumn(columnName, fieldType, isKey);
                }
                tables.push_back(std::move(ptable));
            }
        }
    };

}