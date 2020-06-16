#include "soci/soci.h"
#include "redisoo.h"

#include <iostream>
// #include <istream>
// #include <ostream>
#include <string>
#include <exception>

extern "C" void *zmalloc(size_t size);

namespace testmysql {

bool _open_connection(int db_type, char* connection, soci::session* sql) {
    // assert(sql);

    try {
        switch (db_type) {
        case Mysql:
            sql->open("mysql", std::string(connection));
            break;

        case PostegreSql:
            sql->open("postegresql", std::string(connection));
            break;

        case SqlLite:
            sql->open("sqllit3", std::string(connection));
            break;

        case Oracle:
            sql->open("oracle", std::string(connection));
            break;

        default:
            std::cout << "No such supported database type! db_type = " << db_type << std::endl;
            return false;
        }

    } catch (const std::exception& e) {
        std::cout << "Connecting to database error! error msg = " << e.what() << std::endl;
        return false;
    }

    return true;
}

extern "C" int db_del(int db_type, char* connection, char* statement,
                      char* key, size_t key_sz) {
    try {
        soci::session sql;

        if (!_open_connection(db_type, connection, &sql))
            return 0;

        std::string use_key(key, key_sz);

        sql << std::string(statement), soci::use(use_key);

    } catch (const std::exception& e) {
        std::cout << "Del statement execution failed! error msg = " << e.what() << std::endl;
        return 0;
    }

    return 1;
}

extern "C" int db_set(int db_type, char* connection, char* statement,
                      char* key, size_t key_sz, char* val, size_t val_sz) {
    try {
        soci::session sql;

        if (!_open_connection(db_type, connection, &sql))
            return 0;

        std::string use_key(key, key_sz);
        std::string use_val(val, val_sz);

        sql << std::string(statement), soci::use(use_key), soci::use(use_val);

    } catch (const std::exception& e) {
        std::cout << "Set statement execution failed! error msg = " << e.what() << std::endl;
        return 0;
    }

    return 1;
}

extern "C" int db_get(int db_type, char* connection, char* statement, 
                      char* key, size_t key_sz, char** val, size_t* val_sz) {
    try {
        soci::session sql;

        if (!_open_connection(db_type, connection, &sql))
            return 0;

        std::string use_key(key, key_sz);
        std::string into_value;
        soci::indicator ind;

        sql << std::string(statement), soci::use(use_key), soci::into(into_value, ind);

        if (!sql.got_data()) {
            std::cout << "No row return with get statement!" << std::endl;
            return 0;
        }

        if (ind == soci::i_ok) {
            size_t sz = into_value.size();
            void* new_heap_mem = zmalloc(sz);
            memcpy(new_heap_mem, into_value.data(), sz);
            *val = static_cast<char*>(new_heap_mem);
            *val_sz = sz;
            return 1;
        } else if (ind == soci::i_null) {
            *val = NULL;
            *val_sz = 0;
            return 1;
        } else {
            std::cout << "return is neither i_ok or i_null. ind = " << ind << std::endl;
            return 0;
        }

    } catch (const std::exception& e) {
        std::cout << "Get statement execution failed! error msg = " << e.what() << std::endl;
        return 0;
    }
}

}
