#include "soci/soci.h"
#include "redisoo.h"

#include <grpcpp/grpcpp.h>
#include "redisoo.grpc.pb.h"

#include <iostream>
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

namespace testgrpc {

class RedisooGrpcClient {
public:
    RedisooGrpcClient(std::shared_ptr<grpc::Channel> channel) 
        : stub_(redisoo::Redisoo::NewStub(channel)) {}


    bool GetString(char* key, size_t key_sz, char** val, size_t* val_sz, long long* ttl) {
        redisoo::GetStringRequest request;
        request.set_key(key, key_sz);

        redisoo::GetStringResponse response;
        grpc::ClientContext context;

        grpc::Status status = stub_->GetString(&context, request, &response);
        if (!status.ok()) {
            std::cout << "error: " << status.error_code() << ": " << status.error_message() << std::endl;
            return false;
        } else {
            // std::cout << "success: " << response.value() << std::endl;
            size_t sz = response.value().size();
            void* new_heap_mem = zmalloc(sz);
            memcpy(new_heap_mem, response.value().data(), sz);
            *val = static_cast<char*>(new_heap_mem);
            *val_sz = sz;
            if (response.ttl_ms() > 0)
                *ttl = static_cast<long long>(response.ttl_ms());
            else
                *ttl = 0;
            return true;
        }
    }

private:
    std::unique_ptr<redisoo::Redisoo::Stub> stub_;   
};

extern "C" int grpc_get(char* connection, char* key, size_t key_sz, char** val, size_t* val_sz, long long* ttl) {
    std::string cnn(connection);
    // std::cout << "connection string = " << cnn << std::endl;
    RedisooGrpcClient client(grpc::CreateChannel(cnn, grpc::InsecureChannelCredentials()));
    if (client.GetString(key, key_sz, val, val_sz, ttl))
        return 1;
    else
        return 0;
}

}
