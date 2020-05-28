#include "server.h"
#include "redisoo.h"
#include "backend.h"

#if defined(__APPLE__)
    #include <os/lock.h>
    static os_unfair_lock spinLock;

    void initSpinLock() {
        spinLock = OS_UNFAIR_LOCK_INIT;
    }
#else
    #include <pthread.h>
    static pthread_spinlock_t spinLock;

    void initSpinLock() {
        pthread_spin_init(&spinLock, 0);
    }    
#endif

typedef struct backendSetInfo {
    sds setValue;      /* the value to set in backend */
    int setTtl;
    list* clients;     /* the clients */
} backendSetInfo;

#define BACKEND_THREAD_MAX_SLEEP_IN_US 1024
#define OBJ_ENCODING_EMBSTR_SIZE_LIMIT 44

#define GET_MODE   1
#define SET_MODE   2
#define DEL_MODE   3

/* check new read/write candidate job key in main thread, does not need to lock anything 
 * if all db backendKeys is empty, return 0
 * else, return 1 with one random candidate job of dbid & key  */
int _hasAvailableCandidate(int mode, int *dbid, sds *key) {
    dictIterator *it;
    dictEntry *entry;

    switch(mode) {
    case GET_MODE:
        for (int i = 0; i < server.dbnum; ++i) {
            if (dictSize(server.db[i].backendGetKeys) == 0) continue;

            it = dictGetIterator(server.db[i].backendGetKeys);
            entry = dictNext(it);
            serverAssert(entry != NULL);
            *key = dictGetKey(entry);
            *dbid = i;
            dictReleaseIterator(it);
            return 1;
        }
        break;

    case SET_MODE:
        for (int i = 0; i < server.dbnum; ++i) {
            if (dictSize(server.db[i].backendSetKeys) == 0) continue;

            it = dictGetIterator(server.db[i].backendSetKeys);
            entry = dictNext(it);
            serverAssert(entry != NULL);
            *key = dictGetKey(entry);
            *dbid = i;
            dictReleaseIterator(it);
            return 1;
        }
        break;

    case DEL_MODE:
        for (int i = 0; i < server.dbnum; ++i) {
            if (dictSize(server.db[i].backendDelKeys) == 0) continue;

            it = dictGetIterator(server.db[i].backendDelKeys);
            entry = dictNext(it);
            serverAssert(entry != NULL);
            *key = dictGetKey(entry);
            *dbid = i;
            dictReleaseIterator(it);
            return 1;
        }
        break;

    default:
        serverAssert("can not reach here");
    }

    return 0;
}

void _createNewJobIfHasMoreCandidates(int mode, int need_check_key_first) {
    if (need_check_key_first) {
        // need check key first, including the working key and the return key
        // when main thread deal with concurrent commands from all of clients
        // check 1. the working thread finish the job, 2. the main thread has already dealt with the return key 
        int all_null;
        rocklock();
        if (mode == GET_MODE)
            all_null = server.getJob.getKey == NULL && server.getJob.returnGetKey == NULL;
        else if (mode == SET_MODE)
            all_null = server.setJob.setKey == NULL && server.setJob.returnSetKey == NULL;
        else
            all_null = server.delJob.delKey == NULL && server.delJob.returnDelKey == NULL;
        rockunlock();
        if (!all_null)
            return; 
    }

    int dbid;
    sds key;
    if (_hasAvailableCandidate(mode, &dbid, &key) == 0)
        return;

    rocklock();

    if (mode == GET_MODE) {
        serverAssert(server.getJob.getKey == NULL);  
        serverAssert(server.getJob.returnGetKey == NULL); 

        /* we need a copy key because the key may be released, in future, the copyKey will  
         * be released by the sdsfree(returnReadKey)
         * NOTE: not now!!!!, check _clearFinishKey() */
        sds copyKey = sdsdup(key);  
        server.getJob.getKey = copyKey;
        server.getJob.dbid = dbid;

    } else if (mode == SET_MODE) {
        serverAssert(server.setJob.setKey == NULL); 
        serverAssert(server.setJob.returnSetKey == NULL);   
        serverAssert(server.setJob.valToBackend == NULL);    
        sds copyKey = sdsdup(key); 
        server.setJob.setKey = copyKey;
        server.setJob.dbid = dbid;
        server.setJob.setSyncResult = 0;
        // we need a copy of setValue
        dictEntry *de = dictFind(server.db[dbid].backendSetKeys, copyKey);
        backendSetInfo *setInfo = dictGetVal(de);
        sds copyValue = sdsdup(setInfo->setValue);
        server.setJob.valToBackend = copyValue;
        server.setJob.setTtl = setInfo->setTtl;

    } else {
        serverAssert(server.delJob.delKey == NULL); 
        serverAssert(server.delJob.returnDelKey == NULL);       
        sds copyKey = sdsdup(key); 
        server.delJob.delKey = copyKey;
        server.delJob.dbid = dbid;
        server.delJob.delSyncResult = 0;
    }

    rockunlock();
}

/* when get command, we need to check whether it will go into backend state 
 * the client should go to the backend state for matching all these conditions 
 * 1. get command 
 * 2. server set config with get statement 
 * 3. the key does not exist (NOTE: the key may be inserted by itself in before)
 * 4. the backend reentry with failed result  
 * if the client match the above conditions, we return 1 to tell the caller the client needs go into backend state */
int _checkGetCommandForBackendState(client *c) {
    if (strcmp(c->cmd->name, "get") != 0)
        return 0;

    if (server.redisoo_get == NULL || strcmp(server.redisoo_get, "") == 0)
        return 0;

    robj *key = c->argv[1];
    robj *o = lookupKeyRead(c->db, key);    // will tigger ttl if key exisits
    
    if (o != NULL) 
        return 0;   // if have the key (maybe inserted by myself in previous timing), we return false

    // if the client is reentry with failed result (including not reentry but happen with the same key, we treat it as reentry)
    int reentry = 0;
    rocklock();
    if (server.getJob.valFromBackend == NULL && server.getJob.returnGetKey && sdscmp(server.getJob.returnGetKey, key->ptr) == 0)
        reentry = 1;
    rockunlock(); 

    return reentry ? 0 : 1;
}

bool _isSetCommnd(char* cmdName) {
    if (strcmp(cmdName, "set") == 0)
        return 1;
    else if (strcmp(cmdName, "setex") == 0)
        return 1;
    else if (strcmp(cmdName, "setnx") == 0)
        return 1;
    else if (strcmp(cmdName, "psetex") == 0)
        return 1;
    else
        return 0;
}

/* set the same key, but already has differnt value in backendSetKeys */
int _isSameKeyWithDifferentValForSet(client *c) {
    sds key = c->argv[1]->ptr;
    sds val = c->argv[2]->ptr;

    dict* d = c->db->backendSetKeys;

    dictEntry *de = dictFind(d, key);
    if (de == NULL)
        return 0;       // new key for d
    
    backendSetInfo *setInfo = dictGetVal(de);
    if (sdscmp(setInfo->setValue, val) == 0)
        return 0;
    else
        return 1;
}

int _checkSetCommandForBackendState(client *c) {
    if (!_isSetCommnd(c->cmd->name))
        return 0;

    if (server.redisoo_set == NULL || strcmp(server.redisoo_set, "") == 0)
        return 0;

    if (strcmp(c->cmd->name, "setnx") == 0) {
        // setnx needs the condition of not existence
        robj *key = c->argv[1];
        robj *o = lookupKeyRead(c->db, key);    // will tigger ttl if key exisits        
        if (o != NULL) 
            return 0;   // if have the key (maybe inserted by myself in previous timing), we return false
    }

    if (_isSameKeyWithDifferentValForSet(c))
        return 0;

    return 1;
}

int _checkDelCommandForBackendState(client *c) {
    if (strcmp(c->cmd->name, "del") != 0)
        return 0;

    if (server.redisoo_del == NULL || strcmp(server.redisoo_del, "") == 0)
        return 0;

    if (c->argc > 2)
        return 0;   // we only support one key deletion

    return 1;
}

#define OBJ_SET_NO_FLAGS 0
#define OBJ_SET_NX (1<<0)          /* Set if key not exists. */
#define OBJ_SET_XX (1<<1)          /* Set if key exists. */
#define OBJ_SET_EX (1<<2)          /* Set if time in seconds is given */
#define OBJ_SET_PX (1<<3)          /* Set if time in ms in given */
#define OBJ_SET_KEEPTTL (1<<4)     /* Set and keep the ttl */

/* in milliseconds */
int _getTtlForSetCommand(client *c) {
    char *cmdName = c->cmd->name;

    if (strcmp(cmdName, "set") == 0) {
        // check t_string.c setCommand()
        int j;
        robj *expire = NULL;
        int unit = UNIT_SECONDS;
        int flags = OBJ_SET_NO_FLAGS;

        for (j = 3; j < c->argc; j++) {
            char *a = c->argv[j]->ptr;
            robj *next = (j == c->argc-1) ? NULL : c->argv[j+1];

            if ((a[0] == 'n' || a[0] == 'N') &&
                (a[1] == 'x' || a[1] == 'X') && a[2] == '\0' &&
                !(flags & OBJ_SET_XX))
            {
                flags |= OBJ_SET_NX;
            } else if ((a[0] == 'x' || a[0] == 'X') &&
                    (a[1] == 'x' || a[1] == 'X') && a[2] == '\0' &&
                    !(flags & OBJ_SET_NX))
            {
                flags |= OBJ_SET_XX;
            } else if (!strcasecmp(c->argv[j]->ptr,"KEEPTTL") &&
                    !(flags & OBJ_SET_EX) && !(flags & OBJ_SET_PX))
            {
                flags |= OBJ_SET_KEEPTTL;
            } else if ((a[0] == 'e' || a[0] == 'E') &&
                    (a[1] == 'x' || a[1] == 'X') && a[2] == '\0' &&
                    !(flags & OBJ_SET_KEEPTTL) &&
                    !(flags & OBJ_SET_PX) && next)
            {
                flags |= OBJ_SET_EX;
                unit = UNIT_SECONDS;
                expire = next;
                j++;
            } else if ((a[0] == 'p' || a[0] == 'P') &&
                    (a[1] == 'x' || a[1] == 'X') && a[2] == '\0' &&
                    !(flags & OBJ_SET_KEEPTTL) &&
                    !(flags & OBJ_SET_EX) && next)
            {
                flags |= OBJ_SET_PX;
                unit = UNIT_MILLISECONDS;
                expire = next;
                j++;
            } else {
                return 0;
            }
        }

        long long val;
        if (getLongLongFromObject(expire, &val) != C_OK)
            return 0;
        if (unit == UNIT_SECONDS)
            return 1000*val;
        else
            return val;     // UNIT_MILLISECONDS

    } else if (strcmp(cmdName, "setex") == 0) {
        robj *expire = c->argv[2];
        long long val;
        if (getLongLongFromObject(expire, &val) != C_OK)
            return 0;
        // UNIT_SECONDS
        return val*1000;
    } else if (strcmp(cmdName, "psetex") == 0) {
        robj *expire = c->argv[2];
        long long val;
        if (getLongLongFromObject(expire, &val) != C_OK)
            return 0;
        // UNIT_MILLISECONDS
        return val;
    } else {    // include setnx
        return 0;
    }
}

/* add a key to backend keys (if exist, just add the client to the list) as a candidate */
void _addKeyToBackendKeys(int mode, client *c, int sync) {
    sds copyKey = sdsdup(c->argv[1]->ptr);
    dictEntry *de;

    if (mode == GET_MODE) {
        de = dictFind(c->db->backendGetKeys, copyKey);
        if (!de) {
            dictAdd(c->db->backendGetKeys, copyKey, listCreate());
            de = dictFind(c->db->backendGetKeys, copyKey);
        }
    } else if (mode == SET_MODE) {
        de = dictFind(c->db->backendSetKeys, copyKey);
        int ttl = _getTtlForSetCommand(c);
        if (!de) {
            sds copyValue = sdsdup(c->argv[2]->ptr);
            backendSetInfo *setInfo = zmalloc(sizeof(*setInfo));
            setInfo->setValue = copyValue;
            setInfo->clients = listCreate();
            setInfo->setTtl = ttl;
            dictAdd(c->db->backendSetKeys, copyKey, setInfo);
            de = dictFind(c->db->backendSetKeys, copyKey);
        } else {
            // guarentee to have the same value
            backendSetInfo *setInfo = dictGetVal(de);
            serverAssert(sdscmp(setInfo->setValue, c->argv[2]->ptr) == 0);
            setInfo->setTtl = ttl;
        }
    } else {
        de = dictFind(c->db->backendDelKeys, copyKey);
        if (!de) {
            dictAdd(c->db->backendDelKeys, copyKey, listCreate());
            de = dictFind(c->db->backendDelKeys, copyKey);
        }
    }
    
    if (sync) {
        list *clients;
        if (mode == SET_MODE) {
            backendSetInfo *setInfo = dictGetVal(de);
            clients = setInfo->clients;
        } else {
            clients = dictGetVal(de);
        }
        listAddNodeTail(clients, c);
    }
    
    if (dictGetKey(de) != copyKey) 
        sdsfree(copyKey);
}

/* check whether the client needs to be in the backend state 
 * only when 1. redisoo feature enable 2. get or set command, 3. not in transaction
 * side effects: 
 * 1. update the backend state of client  
 * 2. update the db->backendKeys, i.e. candidates
 * 3. if no job exist, init a current job 
 * */
void checkNeedToBackendState(client *c) {
    serverAssert(c->backend_state == CLIENT_BACKEND_NORMAL);

    if (server.redisoo_db_type == 0)
        return;

    if (c->flags & CLIENT_MULTI)         
        return;     // if in transaction, return

    int sync;

    if (_checkGetCommandForBackendState(c)) {
        sync = server.redisoo_get_sync;
        // 1. upate state
        if (sync)
            c->backend_state = CLIENT_BACKEND_GET;
        // 2. update backendKeys
        _addKeyToBackendKeys(GET_MODE, c, sync);
        // 3. check whether needs to init a new job 
        _createNewJobIfHasMoreCandidates(GET_MODE, 1);

        return;
    }

    if (_checkSetCommandForBackendState(c)) {
        sync = server.redisoo_set_sync;
        if (sync)
            c->backend_state = CLIENT_BACKEND_SET;
        _addKeyToBackendKeys(SET_MODE, c, sync);
        _createNewJobIfHasMoreCandidates(SET_MODE, 1);
        
        return;
    }

    if (_checkDelCommandForBackendState(c)) {
        sync = server.redisoo_del_sync;
        if (sync)
            c->backend_state = CLIENT_BACKEND_DEL;
        _addKeyToBackendKeys(DEL_MODE, c, sync);
        _createNewJobIfHasMoreCandidates(DEL_MODE, 1);

        return;
    }
}

/* if no job, return 0, else return 1 with the job in dbid & key
 * side effects: after getting the job, we set jobKey to NULL */
int _getJobInWorkingThread(int mode, int *dbid, sds *key) {
    int have_job = 0;
    rocklock();

    if (mode == GET_MODE) {
        if (server.getJob.returnGetKey == NULL && server.getJob.getKey != NULL) {
            serverAssert(server.getJob.returnGetKey == NULL);
            *key = server.getJob.getKey;
            *dbid = server.getJob.dbid;
            have_job = 1;
        }
    } else if (mode == SET_MODE) {
        if (server.setJob.returnSetKey == NULL && server.setJob.setKey != NULL) {
            serverAssert(server.setJob.returnSetKey == NULL);
            *key = server.setJob.setKey;
            *dbid = server.setJob.dbid;
            have_job = 1;
        }
    } else {
        if (server.delJob.returnDelKey == NULL && server.delJob.delKey != NULL) {
            serverAssert(server.delJob.returnDelKey == NULL);
            *key = server.delJob.delKey;
            *dbid = server.setJob.dbid;
            have_job = 1;
        }
    }

    rockunlock();
    return have_job;
}

void _implementGetJobByBackendInWorkingThread(sds key, sds *val) {
    char *val_db_ptr;
    size_t val_db_len;

    int ret = db_get(server.redisoo_db_type, server.redisoo_connection, server.redisoo_get,
                     key, sdslen(key), &val_db_ptr, &val_db_len);

    if (ret == 0) {
        /* this means the backend returns a valid response, but the result is wrong */
        *val = NULL;    
        return;
    } else if (val_db_ptr == NULL) {
        // return indication it is NULL
        *val = NULL;
        return;
    }

    serverAssert(val_db_ptr && val_db_len);

    *val = sdsnewlen(val_db_ptr, val_db_len);
        
    /* we need to free the memory allocated by the backend api */
    zfree(val_db_ptr);
}

int _implementSetJobByBackendInWorkingThread(sds key, sds val) {
    int ret = db_set(server.redisoo_db_type, server.redisoo_connection, server.redisoo_set,
                     key, sdslen(key), val, sdslen(val));
    return ret;    
}

int _implementDelJobByBackendInWorkingThread(sds key) {
    int ret = db_del(server.redisoo_db_type, server.redisoo_connection, server.redisoo_del,
                     key, sdslen(key));
    return ret;    
}


int _haveJobThenDoJobInWorkingThread(int mode) {
    int dbid;
    sds key;
    if (_getJobInWorkingThread(mode, &dbid, &key) == 0)
        return 0;   // no job

    serverAssert(dbid >= 0 && dbid < server.dbnum);
    if (mode == GET_MODE) {
        sds valFromBackend; 
        _implementGetJobByBackendInWorkingThread(key, &valFromBackend);

        /* after finish a job, we need return the job result and notify the main thread */
        rocklock();
        serverAssert(server.getJob.getKey == key && server.getJob.returnGetKey == NULL); 
        server.getJob.returnGetKey = key;                  // NOTE: key moved from readKey to returnReadKey
        server.getJob.valFromBackend = valFromBackend;     // NOTE: valFromBackend could be NULL
        rockunlock();

        /* signal main thread rockPipeReadHandler()*/
        char tmpUseBuf[1] = "g";
        write(server.backend_get_pipe_work_end, tmpUseBuf, 1);

    } else if (mode == SET_MODE) {
        sds valToBackend;
        rocklock();
        serverAssert(server.setJob.valToBackend);
        valToBackend = server.setJob.valToBackend;
        rockunlock(); 
        int ret = _implementSetJobByBackendInWorkingThread(key, valToBackend);

        /* after finish a job, we need return the job result and notify the main thread */
        rocklock();
        serverAssert(server.setJob.setKey == key && server.setJob.returnSetKey == NULL);
        server.setJob.returnSetKey = key;
        server.setJob.setSyncResult = ret;
        rockunlock();

        /* signal main thread _readPipeMainThreadHandler()*/
        char tmpUseBuf[1] = "s";
        write(server.backend_set_pipe_work_end, tmpUseBuf, 1);

    } else {
        int ret = _implementDelJobByBackendInWorkingThread(key);

        rocklock();
        serverAssert(server.delJob.delKey == key && server.delJob.returnDelKey == NULL);
        server.delJob.returnDelKey = key;
        server.delJob.delSyncResult = ret;
        rockunlock();

        char tmpUseBuf[1] = "d";
        write(server.backend_del_pipe_work_end, tmpUseBuf, 1);
    }

    return 1;
}

/* called when the server starts */
void initBackendZeroJobs() {
    rocklock();

    server.getJob.dbid = -1;
    server.getJob.getKey = NULL;
    server.getJob.returnGetKey = NULL;
    server.getJob.valFromBackend = NULL;

    server.setJob.dbid = -1;
    server.setJob.setKey = NULL;
    server.setJob.returnSetKey = NULL;
    server.setJob.valToBackend = NULL;
    server.setJob.setSyncResult = 0;
    server.setJob.setTtl = 0;

    server.delJob.dbid = -1;
    server.delJob.delKey = NULL;
    server.delJob.returnDelKey = NULL;
    server.delJob.delSyncResult = 0;

    rockunlock();
}

/* we resume the client state from BACKEND to nomral */
void _resumeBackendClient(client *c) {
    serverAssert(c->backend_state != CLIENT_BACKEND_NORMAL);
    c->backend_state = CLIENT_BACKEND_NORMAL;

    server.current_client = c;

    call(c, CMD_CALL_FULL); 

    c->woff = server.master_repl_offset;
    if (listLength(server.ready_keys))
        handleClientsBlockedOnKeys();

    if (c->flags & CLIENT_MASTER && !(c->flags & CLIENT_MULTI)) {
        /* Update the applied replication offset of our master. */
        c->reploff = c->read_reploff - sdslen(c->querybuf) + c->qb_pos;
    }

    serverAssert(!(c->flags & CLIENT_BLOCKED) || c->btype != BLOCKED_MODULE);

    resetClient(c);

    processInputBuffer(c);
}

robj* _createStringObject(sds s) {
    serverAssert(s);
    size_t val_len = sdslen(s);
    if (val_len <= OBJ_ENCODING_EMBSTR_SIZE_LIMIT)
        return createEmbeddedStringObject(s, val_len); 
    else
        return createRawStringObject(s, val_len);    
}

/* when finish reading/writing a value from/to backend, the main thread needs to 
 * insert/update the key with the value in the original db if the return value is not null. 
 * If the return value is null, keep the failed value with the return key until all resumed clients process it
 * After all clients relating to the key have been processed, changed the return key to NULL to indicate room for a new job 
 * NOTE1: call in transaction will not be routed here (i.e when transaction, we will skip the backend call) 
 * NOTE2: if key exist because during the time other clients had set the key, we replace the vaild value 
 * NOTE3: if return val is NULL, i.e. backend failed to retrive the value, we will keep the NULL value for the resumed clients 
 * NOTE4: last we will clear the return key for new room of a new job 
 */
void _clearFinishKey(int mode, int dbid, sds key, sds val, int setOrDelResult, int setTtl) {    
    listIter li;
    listNode *ln;
    list *clients, *copyClients;
    int ret;

    if (mode == GET_MODE && val != NULL) {  
        serverLog(LL_NOTICE, "_clearFinishKey(), GET_MODE, key = %s, val = %s", key, val);
        // check t_string.c setGenericCommand() for reference
        robj* keyObj = _createStringObject(key);
        robj* valObj = _createStringObject(val);
        genericSetKey(NULL,&server.db[dbid],keyObj,valObj,1,1);
        server.dirty++;
        if (server.redisoo_get_ttl > 0)
            setExpire(NULL,&server.db[dbid],keyObj,mstime()+server.redisoo_get_ttl);
        notifyKeyspaceEvent(NOTIFY_STRING,"set",keyObj,dbid);
        if (server.redisoo_get_ttl > 0)
            notifyKeyspaceEvent(NOTIFY_GENERIC,"expire",keyObj,dbid);
        decrRefCount(keyObj);       
        decrRefCount(valObj);   
    } else if (mode == SET_MODE && setOrDelResult) {
        serverAssert(val);
        serverLog(LL_NOTICE, "_clearFinishKey(), SET_MODE, key = %s, val = %s", key, val);
        robj* keyObj = _createStringObject(key);
        robj* valObj = _createStringObject(val);
        genericSetKey(NULL,&server.db[dbid],keyObj,valObj,1,1);
        server.dirty++;
        if (setTtl > 0)
            setExpire(NULL,&server.db[dbid],keyObj,mstime()+setTtl);
        notifyKeyspaceEvent(NOTIFY_STRING,"set",keyObj,dbid);
        if (setTtl > 0)
            notifyKeyspaceEvent(NOTIFY_GENERIC,"expire",keyObj,dbid);
        decrRefCount(keyObj);       
        decrRefCount(valObj);   
    }

    /* get all the waiting clients
     * NOTE: clients maybe empty, because client may be disconnected or async call */
    if (mode == GET_MODE) {
        clients = dictFetchValue(server.db[dbid].backendGetKeys, key);
    } else if (mode == SET_MODE) {
        backendSetInfo *setInfo = dictFetchValue(server.db[dbid].backendSetKeys, key);
        clients = setInfo->clients;
    } else {
        clients = dictFetchValue(server.db[dbid].backendDelKeys, key);
    }  
    serverAssert(clients != NULL);
    copyClients = listCreate();
    listRewind(clients, &li);
    while((ln = listNext(&li))) {
        client *c = listNodeValue(ln);
        listAddNodeTail(copyClients, c);
    }

    /* delete the entry in backendKeys, NOTE the clients will be invalid, but we already have the copyClients */
    if (mode == GET_MODE) {
        ret = dictDelete(server.db[dbid].backendGetKeys, key);
    } else if (mode == SET_MODE) {
        backendSetInfo *setInfo = dictFetchValue(server.db[dbid].backendSetKeys, key);
        sdsfree(setInfo->setValue);
        listRelease(setInfo->clients);
        ret = dictDelete(server.db[dbid].backendSetKeys, key);
        zfree(setInfo);
    } else {
        ret = dictDelete(server.db[dbid].backendDelKeys, key);
    }    
    serverAssert(ret == DICT_OK);

    /* try resume the copyClients */
    listRewind(copyClients, &li);
    while((ln = listNext(&li))) {
        client *c = listNodeValue(ln);
        if (mode == SET_MODE && setOrDelResult == 0) {
            c->backend_set_sync_reply = 1;
        } else if (mode == DEL_MODE && setOrDelResult == 0) {
            c->backend_del_sync_reply = 1;
        }

        _resumeBackendClient(c);
    }

    listRelease(copyClients);

    rocklock();
    if (mode == GET_MODE) {
        serverAssert(server.getJob.returnGetKey != NULL && server.getJob.getKey == server.getJob.returnGetKey); 
        sdsfree(server.getJob.returnGetKey);
        server.getJob.returnGetKey = NULL;
        server.getJob.getKey = NULL;
        if (server.getJob.valFromBackend)
            sdsfree(server.getJob.valFromBackend);
        server.getJob.valFromBackend = NULL;
        server.getJob.dbid = -1;
        
    } else if (mode == SET_MODE) {
        serverAssert(server.setJob.returnSetKey != NULL && server.setJob.setKey == server.setJob.returnSetKey); 
        sdsfree(server.setJob.returnSetKey);
        server.setJob.returnSetKey = NULL;
        server.setJob.setKey = NULL;
        if (server.setJob.valToBackend)
            sdsfree(server.setJob.valToBackend);
        server.setJob.valToBackend = NULL;
        server.setJob.dbid = -1;
        server.setJob.setSyncResult = 0;
        server.setJob.setTtl = 0;
        
    } else {
        serverAssert(server.delJob.returnDelKey != NULL && server.delJob.delKey == server.delJob.returnDelKey); 
        sdsfree(server.delJob.returnDelKey);
        server.delJob.returnDelKey = NULL;
        server.delJob.delKey = NULL;
        server.delJob.dbid = -1;
        server.delJob.delSyncResult = 0;
    }
    rockunlock();

    _createNewJobIfHasMoreCandidates(mode, 0);
}

/*
void initBackendJob(int mode) {
    rocklock();

    if (mode == GET_MODE) {
        server.getJob.dbid = -1;
        serverAssert(server.getJob.getKey == NULL);
        if (server.getJob.returnGetKey) 
            sdsfree(server.getJob.returnGetKey);
        server.getJob.returnGetKey = NULL;
        if (server.getJob.valFromBackend)
            decrRefCount(server.getJob.valFromBackend);
        server.getJob.valFromBackend = NULL;
    } else if (mode == SET_MODE) {
        server.setJob.dbid = -1;
        serverAssert(server.setJob.setKey == NULL);
        if (server.setJob.returnSetKey) 
            sdsfree(server.setJob.returnSetKey);
        server.setJob.returnSetKey = NULL;
        server.setJob.valToBackend = NULL;
    } else {
        server.delJob.dbid = -1;
        serverAssert(server.delJob.delKey == NULL);
        if (server.delJob.returnDelKey) 
            sdsfree(server.delJob.returnDelKey);
        server.delJob.returnDelKey = NULL;
    }

    rockunlock();
}
*/

/* the event handler is executed from main thread, which is signaled by the pipe
 * from the read working thread. When it is called by the event loop, there is 
 * a return result in readJob */
void _getPipeMainThreadHandler(struct aeEventLoop *eventLoop, int fd, void *clientData, int mask) {
    UNUSED(mask);
    UNUSED(clientData);
    UNUSED(eventLoop);

    int finishDbid;
    sds finishKey;
    sds finishVal;

    /* deal with return result */
    rocklock();
    serverAssert(server.getJob.dbid != -1);
    serverAssert(server.getJob.getKey != NULL);
    serverAssert(server.getJob.returnGetKey != NULL);  
    serverAssert(server.getJob.getKey == server.getJob.returnGetKey);
    finishDbid = server.getJob.dbid;
    finishKey = server.getJob.returnGetKey;
    finishVal = server.getJob.valFromBackend;
    rockunlock();

    _clearFinishKey(GET_MODE, finishDbid, finishKey, finishVal, 0, 0);

    char tmpUseBuf[1];
    read(fd, tmpUseBuf, 1);     /* maybe unblock the rockdb thread by read the pipe */ 
}

void _setPipeMainThreadHandler(struct aeEventLoop *eventLoop, int fd, void *clientData, int mask) {
    UNUSED(mask);
    UNUSED(clientData);
    UNUSED(eventLoop);

    int finishDbid;
    sds finishKey;
    sds valToBackend;
    int setResult;
    int setTtl;

    /* deal with return result */
    rocklock();
    serverAssert(server.setJob.dbid != -1);
    serverAssert(server.setJob.setKey != NULL);
    serverAssert(server.setJob.returnSetKey != NULL);  
    serverAssert(server.setJob.setKey == server.setJob.returnSetKey);
    finishDbid = server.setJob.dbid;
    finishKey = server.setJob.returnSetKey;
    setResult = server.setJob.setSyncResult;
    setTtl = server.setJob.setTtl;
    valToBackend = server.setJob.valToBackend;
    rockunlock();

    _clearFinishKey(SET_MODE, finishDbid, finishKey, valToBackend, setResult, setTtl);

    char tmpUseBuf[1];
    read(fd, tmpUseBuf, 1);     /* maybe unblock the rockdb thread by read the pipe */ 
}

void _delPipeMainThreadHandler(struct aeEventLoop *eventLoop, int fd, void *clientData, int mask) {
    UNUSED(eventLoop);
    UNUSED(mask);
    UNUSED(clientData);

    int finishDbid;
    sds finishKey;
    int delResult;

    /* deal with return result */
    rocklock();
    serverAssert(server.delJob.dbid != -1);
    serverAssert(server.delJob.delKey != NULL);
    serverAssert(server.delJob.returnDelKey != NULL);  
    serverAssert(server.delJob.delKey == server.delJob.returnDelKey);
    finishDbid = server.delJob.dbid;
    finishKey = server.delJob.returnDelKey;
    delResult = server.delJob.delSyncResult;
    rockunlock();

    _clearFinishKey(DEL_MODE, finishDbid, finishKey, NULL, delResult, 0);

    char tmpUseBuf[1];
    read(fd, tmpUseBuf, 1);     /* maybe unblock the rockdb thread by read the pipe */ 
}

/* this is the read thread entrance, working together with the main thread */
void* _entryInGetThread(void *arg) {
    UNUSED(arg);
    int sleepMicro = 1;

    while(1) {
        if (_haveJobThenDoJobInWorkingThread(GET_MODE)) {
            sleepMicro = 1;
        } else {
            if (sleepMicro >= BACKEND_THREAD_MAX_SLEEP_IN_US) 
                sleepMicro = BACKEND_THREAD_MAX_SLEEP_IN_US;
            usleep(sleepMicro);
            sleepMicro <<= 1;
        }
    }

    return NULL;
}

void* _entryInSetThread(void *arg) {
    UNUSED(arg);
    int sleepMicro = 1;

    while(1) {
        if (_haveJobThenDoJobInWorkingThread(SET_MODE)) {
            sleepMicro = 1;
        } else {
            if (sleepMicro >= BACKEND_THREAD_MAX_SLEEP_IN_US) 
                sleepMicro = BACKEND_THREAD_MAX_SLEEP_IN_US;
            usleep(sleepMicro);
            sleepMicro <<= 1;
        }
    }

    return NULL;
}

void* _entryInDelThread(void *arg) {
    UNUSED(arg);
    int sleepMicro = 1;

    while(1) {
        if (_haveJobThenDoJobInWorkingThread(DEL_MODE)) {
            sleepMicro = 1;
        } else {
            if (sleepMicro >= BACKEND_THREAD_MAX_SLEEP_IN_US) 
                sleepMicro = BACKEND_THREAD_MAX_SLEEP_IN_US;
            usleep(sleepMicro);
            sleepMicro <<= 1;
        }
    }

    return NULL;
}

/* the main thread will call initRockPipe() to create two working thread, i.e. backend thread,
 * one for read, and the other for write, which will read/write data from/to backend, i.e. database 
 * main thread and backend threads will be synchronized by a spinning lock,
 * and signal (wakeup) the backend thread by the pipe, server.backend_pipe */
void initBackendWorkingThreads() {
    pthread_t get_thread, set_thread, del_thread;
    int getPipefds[2];
    int setPipeFds[2];
    int delPipeFds[2];

    if (pipe(getPipefds) == -1) serverPanic("Can not create get pipe for backend.");
    if (pipe(setPipeFds) == -1) serverPanic("Can not create set pipe for backend.");
    if (pipe(delPipeFds) == -1) serverPanic("Can not create del pipe for backend.");

    server.backend_get_pipe_main_end = getPipefds[0];     // main thread get signal by the read-end, i.e. [0]
    server.backend_get_pipe_work_end = getPipefds[1];     // work thread send signal from the write-end, i.e. [1]

    server.backend_set_pipe_main_end = setPipeFds[0];
    server.backend_set_pipe_work_end = setPipeFds[1];

    server.backend_del_pipe_main_end = delPipeFds[0];
    server.backend_del_pipe_work_end = delPipeFds[1];

    if (aeCreateFileEvent(server.el, server.backend_get_pipe_main_end, 
        AE_READABLE, _getPipeMainThreadHandler,NULL) == AE_ERR) {
        serverPanic("Unrecoverable error creating server.get_pipe file event.");
    }

    if (aeCreateFileEvent(server.el, server.backend_set_pipe_main_end, 
        AE_READABLE, _setPipeMainThreadHandler,NULL) == AE_ERR) {
        serverPanic("Unrecoverable error creating server.set_pipe file event.");
    }

    if (aeCreateFileEvent(server.el, server.backend_del_pipe_main_end, 
        AE_READABLE, _delPipeMainThreadHandler,NULL) == AE_ERR) {
        serverPanic("Unrecoverable error creating server.del_pipe file event.");
    }

    if (pthread_create(&get_thread, NULL, _entryInGetThread, NULL) != 0) {
        serverPanic("Unable to create a get thread.");
    }

    if (pthread_create(&set_thread, NULL, _entryInSetThread, NULL) != 0) {
        serverPanic("Unable to create a set thread.");
    }

    if (pthread_create(&del_thread, NULL, _entryInDelThread, NULL) != 0) {
        serverPanic("Unable to create a del thread.");
    }
}

/* when a client is free, it need to clear its node
 * in each db wokring keys list of clients, for the later safe call
 * from the main thread pipeHandler() */
void releaseWhenFreeClient(client *c) {
    dictIterator *dit;
    dictEntry *de;
    list *clients;
    backendSetInfo *setInfo;

    for (int i = 0; i< server.dbnum; ++i) {
        if (dictSize(server.db[i].backendGetKeys) == 0) continue;

        dit = dictGetIterator(server.db[i].backendGetKeys);
        while ((de = dictNext(dit))) {
            clients = dictGetVal(de);
            serverAssert(clients);
            listIter lit;
            listNode *ln;
            listRewind(clients, &lit);
            while ((ln = listNext(&lit))) {
                if (listNodeValue(ln) == c) listDelNode(clients, ln);
            }
        }
        dictReleaseIterator(dit);
    }

    for (int i = 0; i< server.dbnum; ++i) {
        if (dictSize(server.db[i].backendSetKeys) == 0) continue;

        dit = dictGetIterator(server.db[i].backendSetKeys);
        while ((de = dictNext(dit))) {
            setInfo = dictGetVal(de);
            clients = setInfo->clients;
            serverAssert(clients);
            listIter lit;
            listNode *ln;
            listRewind(clients, &lit);
            while ((ln = listNext(&lit))) {
                if (listNodeValue(ln) == c) listDelNode(clients, ln);
            }
        }
        dictReleaseIterator(dit);
    }

    for (int i = 0; i< server.dbnum; ++i) {
        if (dictSize(server.db[i].backendDelKeys) == 0) continue;

        dit = dictGetIterator(server.db[i].backendDelKeys);
        while ((de = dictNext(dit))) {
            clients = dictGetVal(de);
            serverAssert(clients);
            listIter lit;
            listNode *ln;
            listRewind(clients, &lit);
            while ((ln = listNext(&lit))) {
                if (listNodeValue(ln) == c) listDelNode(clients, ln);
            }
        }
        dictReleaseIterator(dit);
    }
}
