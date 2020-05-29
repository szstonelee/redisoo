
# What is Redisoo?

Redisoo pronouces /rediso͞o/, means **Redis + Through**.

In short, Redisoo make Redis directly connect to the backend database, 

including Mysql, Postegresql, Oracle, ODBC(SQL Server), SqlLite, DB2, Firbird.

It make Redis the 'Cache Through' pattern to solve some problems.

# Cache Through vs Cache Aside
[You can check this artile for the comparsion](https://codeahoy.com/2017/08/11/caching-strategies-and-how-to-choose-the-right-one/)


# The problems Redisoo solve

## Consistency

In "Cache Aside" pattern, there is an inconsistency problem.

<img src="inconsistency.jpg" width=600>

In "Cache Through" pattern, no such problem.

## Duplicated Action, worst for peak time

In the above diagram, if application 1 & 2 get the same key from Redis at the same time.

If they can not find the key in Redis, they both get the value from database then write to Redis. 

The second action is a duplicated one, and it can not be avoid in Cache Aside pattern. 

The Cache Through pattern can solve the duplicated problem. 

In peak time, the fan-out number may be hundreds or thousands depenging on how many application you have in a system.

## Duplicated applicaton component for read/write to data store

You need develop the same logic for different application.

E.g. 

If you have Python/Java/Php/C++ application, you need develop the same logic for all kinds of application.

Even in one language with serveral applicatons, if the abstraction or common library is not good enough, 

You need to write the different codes for the same logic.

This time, Cache Through can save your code because every component only need deal with Get/Set/Del common operation.

The applications only see the Redis, no database. Redisoo deal with database for you.



