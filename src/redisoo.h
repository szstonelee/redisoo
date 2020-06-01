#ifndef __TESTMYSQL_H
#define __TESTMYSQL_H

/* DatabaseType can not be zero, which means the wrong type */
enum DatabaseType {
  Mysql = 1,
  PostegreSql = 2,
  Oracle = 3,
  Odbc = 4,
  Db2 = 5,
  SqlLite = 6,  
  Firebird = 7,
  Empty = 8,
};

int db_get(int db_type, char* connection, char* statement, 
           char* key, size_t key_sz, char** val, size_t* val_sz);

int db_set(int db_type, char* connection, char* statement,
           char* key, size_t key_sz, char* val, size_t val_sz);

int db_del(int db_type, char* connection, char* statement,
           char* key, size_t key_sz);

#endif
