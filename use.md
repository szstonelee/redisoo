# How Use (example of MySQL)

[When you build succesfully](build.md), you can try Redisoo, just run

```
cd
cd redisoo
cd lib
./redisoo
```

That is it!!!

## How Redis work with config arguments

There are several set of config arguments for Redisoo, just like config arguments for Redis.

Please reference [Redis Config](https://redis.io/topics/config)

You can use config arguments in three ways in Redis (**the same way as Redisoo**)

### 1. launch with config arguments

e.g.

```
./redis-server --bind 0.0.0.0
```
 This let redis-server start to accept connection from other ip, otherwise you only can connect to Redis in the same machine.

 So **bind** is the argument name, **0.0.0.0** is the value.

 Just like **key/value** pair

 ### 2. modify config arguments online

 e.g.
```
./redis-server --bind 0.0.0.0
```
NOTE: bind is a argument, but it can not be modified online.

then start a client like redis-cli to connect to Redis, assume your Redis runing at IP address: 192.168.64.5 
```
./redis-cli -h 192.168.64.5
```

Let us try the config argument of "maxclients", which limit how many clients can use Redis.

at the first redis-cli, we issue such command
```
config set maxclients 1
```
Then, you can try a second redis-cli with ./redis-cli -h 192.168.64.5, 

The second redis-cli can connect to Redis, but when you issue some command like 'get abc',

Redis will response as the following

```
(error) ERR max number of clients reached
```

So this the second way, use **config set** to change the config arguments of Redis

### 3. use config file

Your can modify the redis.conf file to change the arguments.

Then start Redis, let the arguments in redis.conf take effect.

## Redisoo Config Arguments

### redisoo_db

Default is empty string of "". When it is the default, Redisoo disable connection to database. 

So when redisoo_db == "", it is a pure Redis.

You can use config set anytime to change Redisoo to be a pure Redis.

But if you want Redisoo connect to database, you need set for the following values:

* mysql
* posgegresql
* odbc
* oracle
* db2
* sqllite
* firebird
* grpc

e.g.
```
config set redisoo_db mysql
```

### redisoo_connection

**redisoo_connection** is the connection string for the way to connect to the database.

e.g. MySQL server is installed in IP 192.168.64.5 with default listening port 3306

* Ip:  192.168.64.5
* Port: 3306
* Database: test
* User: redis
* Password: 1234abcd

The SQL statements for MySQL to add a user of redis for password of 1234abcd is like the following 
(when use root to login to MySQL)
```
CREATE DATABASE test;
CREATE USER 'redis'@'localhost' IDENTIFIED BY '1234abcd';
GRANT ALL PRIVILEGES ON test.* TO 'redis'@'localhost';
FLUSH PRIVILEGES;
``` 
NOTE: you should replace 'localhost' with the IP address where redisoo runs.

TIP1: you can use the following MySQL SQL statement to change the user ip address
```
SELECT User, Host FROM mysql.user;
RENAME user 'redis'@'localhost' to 'redis'@'192.168.64.%';
```

TIP2: MySQL server bind ip address may be 127.0.0.1 (when first install, it is the default), you can changed it to 0.0.0.0
```
sudo find /etc -name my.cnf (and go on to find the correct config file of MySQL)
sudo vi /etc/mysql/mysql.conf.d/mysqld.cnf (my linux search result)
and changed bind-address = 127.0.0.1 to bind-address = 0.0.0.0
sudo systemctl stop mysql
sudo systemctl start mysql
```

You can set Redisoo like this
```
config set redisoo_connection "host=192.168.64.5 port=3306 db=test user=redis password='1234abcd'"
```
Other arguments for the connection string for MySQL, [please reference here](http://soci.sourceforge.net/doc/master/backends/mysql/)

Other Database connection string, [please click here and do your try](http://soci.sourceforge.net/doc/master/backends/)

If something wrong with the connection, you can see the output error message in the Redisoo console, e.g.
```
Connecting to database error! error msg = Access denied for user 'redis'@'localhost' (using password: YES)
Connecting to database error! error msg = Malformed connection string.
Connecting to database error! error msg = Can't connect to MySQL server on '127.0.0.1' (61)
```
Check your MySQL config and try to use mysql client tool for some tests to figure out what is wrong with your connection string.

### arguments for get command for Redis

You can enable (or disable) the get command of Redis to use database.

There are three arguments for [Redis 'get' command](https://redis.io/commands/get)
 
* redisoo_get
* redisoo_get_sync
* redisoo_get_ttl 

#### redisoo_get

When you want Redisoo read data, i.e the key(value) from database when the key not in cache, 

You must set **redisoo_get**, like this

e.g. 
Suppose we have MySQL table in database 'test' like this:
```
CREATE TABLE t1 (name varchar(20) NOT NULL, address varchar(200) NOT NULL, PRIMARY KEY(name));
INSERT INTO t1 (name, address) VALUES ('szstonelee', 'bay area');
```

We have a table name 't1', there are two column, the primary key column is 'name' which is varchar, 

and the value column mapping to 'name' is 'address' which is varchar too.

Then we set redisoo_get like this:
```
config set redisoo_get "select address from t1 where name = :name"
```

So when redis client send the command 'get szstonelee', and Redisoo can not find the key 'szstonelee', 

it will read the database using the SQL statement like this 
```
SELECT address FROM t1  WHERE name = 'szstonelee';
```
When you use such Redis command
```
get szstonelee
```
So you need to use your specific SQL statement like the above example.

When the key has ttl, Redisoo will read the value from database when the key is expired.

NOTE: 

if the database want Redisoo know the value does not exist, it must return NULL.

like
```
SELECT NULL FROM t1 WHERE name = 'not_exist'
```
if SELECT return zero rows to Redisoo, Redisoo treats it as an error and will show an error message in the console of Redisoo server.
 
```
No row return with get statement!
```

#### redisoo_get_sync

Two value, "yes" or "no". Default value is "no"

e.g.

```
config set redisoo_get_sync yes
```

* When redisoo_get_sync == yes

If redis client "get key_which_need_read_from_database", the client need to wait 

until Redisoo read something from the database or failed for some issues (like database crash, network unqvailable)

If something wrong, there will an error message in the console of Redisoo server, and the client will receive 'nil' response.

* When redisoo_get_sync == no

If redis client "get key_which_need_read_from_database", 

the client always get 'nil' response but the response is as quick as possible.

Sometime later, the client try the same key, this time, the value has been read by Redisoo from the database,

the client can get the value successfully.

#### redisoo_get_ttl

When you want to set the key TTL automatically with get command which value is from the database, 

you can set the **redisoo_get_ttl** argument. The unit is milli seconds.

If you set it to 0, it means NO TTL.

The default value for redisoo_get_ttl is ZERO.

You can use the set, expire command to set different keys for different TTL.

redisoo_get_ttl is for all keys from database when redisoo_get_ttl > 0.

### arguments fro set comands for Redis

Redisoo support four set commands in Redis, they are 
* [set](https://redis.io/commands/set)
* [setex](https://redis.io/commands/setex)
* [psetex](https://redis.io/commands/psetex)
* [setnx](https://redis.io/commands/setnx)

There are two config arguments for set

#### redisoo_set

It is the same way as redisoo_get, a SQL statement for update(or insert)

Because it only support simple SQL statement, I suggest you use UpSert SQL Statement.

e.g. for MySQL
```
config set redisoo_set "replace into t1 (name, address) values (:name, :address)"
```
The UpSert SQL Statement can Insert/Update using one statement. Most database support this way.

#### redisoo_set_sync

When redisoo_set_sync == yes

Like redisoo_get_sync, the client will wait until the database finish the update job.

If something wrong, the client will receive [Null Reply](https://redis.io/topics/protocol#nil-reply) 

And it will gurantee that the Rediso and the database have the same value for the set key.

When redisoo_set_sync == no

The client will always get the success response, and the value will be set in Redisoo. 

It would be the same value in the database in later time if the database work correctly. 

But if there is something wrong with the database, the value has been changed in the Redisoo but not in the database.

So there are some chance of inconsistency. But the tradeoff is that the client can get the response as quick as possible.

### arguments for del command for Redis

For Redis [del command](https://redis.io/commands/del), but NOTE: only for one key. 

If you use del for multi key, it will delete these key in cache but no effect for database.

There are two config arguments for del.

#### redisoo_del

Like redisoo_get and redisoo_set, it is the delete SQL statement for the database.

e.g.
```
config set redisoo_del "delete from t2 where name = :name" 
```

NOTE: 

We can not know how many rows have been deleted from the database. 

And Redisoo do not know whether you use DELETE statement. If you like, you can use SELECT for del command, but no deletion with db.

So maybe no row has been deleted, but the client get a successful response from Redisoo.


#### redisoo_del_sync

Like the redisoo_set_sync.

When redisoo_del_sync == yes

The client will wait until the key has been deleted from the database. (Acutally, the sql statement finished from database)

And Redisoo will guarentee the key is consistent with the database. 

i.e. If something wrong with the database, the key will not be delete from Redisoo.

When redisoo_del_sync == no

The client will get the response as quick as possible. 

There are some change with incomnsistency when something is wrong with the database 

but as tradeoff, the client get the response as quick as possible.

## Run Redisoo Examples

### Run with config arguments

```
cd
cd redisoo
cd lib
./redisoo --bind 0.0.0.0 --redisoo_db mysql --redisoo_connection "host=192.168.64.5 port=3306 db=test user=redis password='1234abcd'" --redisoo_get "select address from t1 where name = :name" --redisoo_get_sync yes --redisoo_get_ttl 60000
```

### Run then change config arguments

```
cd 
cd redisoo
cd lib
./redisoo --bind 0.0.0.0
```

then in the client machine, start redis-cli, like this (Suppose redisoo run in IP: 192.168.64.1)
```
./redis-cli -h 192.168.64.1
```
in the redis-cli terminal, run the following comand
```
config set redisoo_db mysql
config set redisoo_connection "host=192.168.64.5 port=3306 db=test user=redis password='1234abcd'"
config set redisoo_get "select address from t1 where name = :name"
config set redisoo_get_sync yes
config set redisoo_get_ttl 60000
```

### Sample of gRpc Golang server

In the project, I include a sample gRpc server writing in Golang.

[You can build it](build.md)

After that, run it 
```
cd
cd go_sample_server
./server
```

This server will simply reverse each key as value and return to redisoo.

How test the redisoo with the golang grpc server.

run redisoo as 
```
cd 
cd redisoo
cd lib
./redisoo --bind 0.0.0.0 --redisoo_db grpc --redisoo_connection "localhost:40051" --redisoo_get_sync yes
```

then in another terminal or machine in the same LAN, run a redis-cli, and try to get a key which is not in memory.

You can code your specific gRpc middleware for redisoo, like to connect the backend database.

Or using other language like Python, Java which useing gRpc protocol. Check gRpc for more details.

