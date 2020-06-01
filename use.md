# How Use (example of MySQL)

[When you build succesfully](build.md), you can try Redisoo, just run

```
cd src
./redis-server
```

That is it!!!

## How Redis work with config parameter

There are several set of config parameters for Redisoo, just like config parameter for Redis.

Please reference [Redis Config](https://redis.io/topics/config)

You can use config paramter in three ways

### start with the paramter

e.g.

```
./redis-server --bind 0.0.0.0
```
 This let redis-server start to accept connection from other ip, otherwise you only can connect to Redis in the same machine.

 So **bind** is the parameter name, **0.0.0.0** is the value.

 Just like **key/value** pair

 ### start, then modify parameter online

 e.g.
```
./redis-server --bind 0.0.0.0
```
then start a client like redis-cli, then connect to Redis, assume your Redis runing at IP address: 192.168.64.5 
```
./redis-cli -h 192.168.64.5
```

Let us try "maxclients", which limit how many client can use Redis.

at redis-cli, we issue such command
```
config set maxclients 1
```
Then, you can try a second redis-cli with ./redis-cli -h 192.168.64.5, 

The second redis-cli can connect to Redis, but when you issue some command like 'get abc',

Redis will response as the following

```
(error) ERR max number of clients reached
```

So this the second way, use **config set** to change the parameter of Redis (It is the same idea for Redisoo)

### Start with config file

Your can modify the redis.conf file to change the parameters.

Then start the Redis, let the paramter take effect.

## Redisoo Config Parameter

### redisoo_db

Default is "". When it is the default, Redisoo disable any support for database. 

So when --redisoo_db "", it is a pure Redis.

You can use config set anytime to change Redisoo to be a pure Redis.

But if you want Redisoo connect to the database, you need set for the current values:

* mysql
* posgegresql
* odbc
* oracle
* db2
* sqllite
* firebird

e.g.
```
config set redisoo_db mysql
```

### redisoo_connection

The config parameter define how to connect the database.

E.g. MySQL

* Ip:  192.168.64.5
* Port: 3306
* Database: test
* User: redis (Privilege has been set for database test)
* Password: 1234abcd

You can set like this
```
config set redisoo_connection "host=192.168.64.5 port=3306 db=test user=redis password='1234abcd'"
```
Other parameter for the connection string for MySQL, [please reference here](http://soci.sourceforge.net/doc/master/backends/mysql/)

Other Database connection string, [please click here and do your try](http://soci.sourceforge.net/doc/master/backends/)

If something wrong with the connection, you can see the output error message in the Redisoo console, e.g.
```
Connecting to database error! error msg = Access denied for user 'redis;'@'localhost' (using password: YES)
Connecting to database error! error msg = Malformed connection string.
Connecting to database error! error msg = Can't connect to MySQL server on '127.0.0.1' (61)
```

### get

You can set (or do not set) the get command for the database.

There are three parameters for [Redis 'get' command](https://redis.io/commands/get)
 
* redisoo_get
* redisoo_get_sync
* redisoo_get_ttl 

#### redisoo_get

When you want Redisoo read data, i.e the key(value) frmo database, when the key not in Redis, 

You must set *redisoo_get*, like this
e.g. 

we have a table name 't1', there are two column, the primary key column is 'name' which is varchar, 

and the value column mapping to 'name' is 'address' which is varchar too.

```
config set redisoo_get "select address from t1 where name = :name"
```

So when redis client send the command 'get szstonelee', and Redis can not find the key 'szstonelee', 

it will read the database using the SQL statement like this 
```
SELECT address FROM t1  WHERE name = 'szstonelee';
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
 
``
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

the client get the value successfully and as quick as possible.

#### redisoo_get_ttl

When you want to set the key TTL automatically with get command which value is from the database, 

you can set the **redisoo_get_ttl** parameter. The unit is milli seconds.

If you set it to zero, it means NO TTL.

The default value for redisoo_get_ttl is ZERO.

You can use the set, expire command to set different keys for different TTL.

redisoo_get_ttl is for all keys from database when redisoo_get_ttl > 0.

### set

Redisoo support four set command in Redis, they are 
* [set](https://redis.io/commands/set)
* [setex](https://redis.io/commands/setex)
* [psetex](https://redis.io/commands/psetex)
* [setnx](https://redis.io/commands/setnx)

There are two config parameter for set

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

### del

For Redis [del command](https://redis.io/commands/del), but NOTE: only for one key.

If you use del for multi key, it will delete these key in Redisoo but no effect for the database.

There are two config parameters for del.

#### redisoo_del

Like redisoo_get and redisoo_set, it is the delete SQL statement for the database.

e.g.
```
config set redisoo_del "delete from t2 where name = :name" 
```

NOTE: 

We can not know how many rows have been deleted from the database. 

So maybe no row has been deleted, but the client get a successful response from Redisoo.


#### redisoo_del_sync

Like the redisoo_set_sync.

When redisoo_del_sync == yes

The client will wait until the key has been deleted from the database

And Redisoo will guarentee the key is consistenet with the database. 

i.e. If something wrong with the database, the key will not be delete from Redisoo.

When redisoo_del_sync == no

The client will get the response as quick as possible. 

There are some change with incomnsistency when something is wrong with the database.