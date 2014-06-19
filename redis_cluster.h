#ifndef REDIS_CLUSTER_H
#define REDIS_CLUSTER_H

#include "cluster_library.h"
#include <php.h>
#include <stddef.h>

/* Redis cluster hash slots and N-1 which we'll use to find it */
#define REDIS_CLUSTER_SLOTS 16384
#define REDIS_CLUSTER_MOD   (REDIS_CLUSTER_SLOTS-1)

/* Get attached object context */
#define GET_CONTEXT() \
    ((redisCluster*)zend_object_store_get_object(getThis() TSRMLS_CC))

/* Command building/processing is identical for every command */
#define CLUSTER_BUILD_CMD(name, c, cmd, cmd_len, slot) \
    redis_##name##_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, c->flags, &cmd, \
                       &cmd_len, &slot)

/* Append information required to handle MULTI commands to the tail of our MULTI 
 * linked list. */
#define CLUSTER_ENQUEUE_RESPONSE(c, slot, cb, ctx) \
    clusterFoldItem *_item; \
    _item = emalloc(sizeof(clusterFoldItem)); \
    _item->callback = cb; \
    _item->slot = slot; \
    _item->ctx = ctx; \
    _item->next = NULL; \
    if(c->multi_head == NULL) { \
        c->multi_head = _item; \
        c->multi_curr = _item; \
    } else { \
        c->multi_curr->next = _item; \
        c->multi_curr = _item; \
    } \

/* Simple macro to free our enqueued callbacks after we EXEC */
#define CLUSTER_FREE_QUEUE(c) \
    clusterFoldItem *_item = c->multi_head, *_tmp; \
    while(_item) { \
        _tmp = _item->next; \
        if(_item->ctx) { \
            zval **z_arr = (zval**)_item->ctx; int i=0; \
            while(z_arr[i]!=NULL) { \
                zval_dtor(z_arr[i]); \
                efree(z_arr[i]); \
                i++; \
            } \
            efree(z_arr); \
        } \
        efree(_item); \
        _item = _tmp; \
    } \
    c->multi_head = c->multi_curr = NULL; \

/* Reset anything flagged as MULTI */
#define CLUSTER_RESET_MULTI(c) \
    redisClusterNode **_node; \
    for(zend_hash_internal_pointer_reset(c->nodes); \
        zend_hash_get_current_data(c->nodes, (void**)&_node); \
        zend_hash_move_forward(c->nodes)) \
    { \
        (*_node)->sock->watching = 0; \
        (*_node)->sock->mode     = ATOMIC; \
    } \
    c->flags->watching = 0; \
    c->flags->mode     = ATOMIC; \

/* Simple 1-1 command -> response macro */
#define CLUSTER_PROCESS_CMD(cmdname, resp_func) \
    redisCluster *c = GET_CONTEXT(); \
    char *cmd; int cmd_len; short slot; void *ctx=NULL; \
    if(redis_##cmdname##_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU,c->flags, &cmd, \
                             &cmd_len, &slot, &ctx)==FAILURE) { \
        RETURN_FALSE; \
    } \
    if(cluster_send_command(c,slot,cmd,cmd_len TSRMLS_CC)<0 || c->err!=NULL) {\
        efree(cmd); \
        RETURN_FALSE; \
    } \
    efree(cmd); \
    if(c->flags->mode == MULTI) { \
        CLUSTER_ENQUEUE_RESPONSE(c, slot, resp_func, ctx); \
        RETURN_ZVAL(getThis(), 1, 0); \
    } \
    resp_func(INTERNAL_FUNCTION_PARAM_PASSTHRU, c, ctx); 
        
/* More generic processing, where only the keyword differs */
#define CLUSTER_PROCESS_KW_CMD(kw, cmdfunc, resp_func) \
    redisCluster *c = GET_CONTEXT(); \
    char *cmd; int cmd_len; short slot; void *ctx=NULL; \
    if(cmdfunc(INTERNAL_FUNCTION_PARAM_PASSTHRU, c->flags, kw, &cmd, &cmd_len,\
               &slot,&ctx)==FAILURE) { \
        RETURN_FALSE; \
    } \
    if(cluster_send_command(c,slot,cmd,cmd_len TSRMLS_CC)<0 || c->err!=NULL) { \
        efree(cmd); \
        RETURN_FALSE; \
    } \
    efree(cmd); \
    if(c->flags->mode == MULTI) { \
        CLUSTER_ENQUEUE_RESPONSE(c, slot, resp_func, ctx); \
        RETURN_ZVAL(getThis(), 1, 0); \
    } \
    resp_func(INTERNAL_FUNCTION_PARAM_PASSTHRU, c, ctx); 

/* For the creation of RedisCluster specific exceptions */
PHPAPI zend_class_entry *rediscluster_get_exception_base(int root TSRMLS_DC);

/* Create cluster context */
zend_object_value create_cluster_context(zend_class_entry *class_type 
                                         TSRMLS_DC);

/* Free cluster context struct */
void free_cluster_context(void *object TSRMLS_DC);

/* Inittialize our class with PHP */
void init_rediscluster(TSRMLS_D);

/* RedisCluster method implementation */
PHP_METHOD(RedisCluster, __construct);
PHP_METHOD(RedisCluster, close);
PHP_METHOD(RedisCluster, get);
PHP_METHOD(RedisCluster, set);
PHP_METHOD(RedisCluster, mget);
PHP_METHOD(RedisCluster, mset);
PHP_METHOD(RedisCluster, msetnx);
PHP_METHOD(RedisCluster, mset);
PHP_METHOD(RedisCluster, del);
PHP_METHOD(RedisCluster, dump);
PHP_METHOD(RedisCluster, setex);
PHP_METHOD(RedisCluster, psetex);
PHP_METHOD(RedisCluster, setnx);
PHP_METHOD(RedisCluster, getset);
PHP_METHOD(RedisCluster, exists);
PHP_METHOD(RedisCluster, keys);
PHP_METHOD(RedisCluster, type);
PHP_METHOD(RedisCluster, persist);
PHP_METHOD(RedisCluster, lpop);
PHP_METHOD(RedisCluster, rpop);
PHP_METHOD(RedisCluster, spop);
PHP_METHOD(RedisCluster, rpush);
PHP_METHOD(RedisCluster, lpush);
PHP_METHOD(RedisCluster, blpop);
PHP_METHOD(RedisCluster, brpop);
PHP_METHOD(RedisCluster, rpushx);
PHP_METHOD(RedisCluster, lpushx);
PHP_METHOD(RedisCluster, linsert);
PHP_METHOD(RedisCluster, lrem);
PHP_METHOD(RedisCluster, brpoplpush);
PHP_METHOD(RedisCluster, rpoplpush);
PHP_METHOD(RedisCluster, llen);
PHP_METHOD(RedisCluster, scard);
PHP_METHOD(RedisCluster, smembers);
PHP_METHOD(RedisCluster, sismember);
PHP_METHOD(RedisCluster, sadd);
PHP_METHOD(RedisCluster, srem);
PHP_METHOD(RedisCluster, sunion);
PHP_METHOD(RedisCluster, sunionstore);
PHP_METHOD(RedisCluster, sinter);
PHP_METHOD(RedisCluster, sinterstore);
PHP_METHOD(RedisCluster, sdiff);
PHP_METHOD(RedisCluster, sdiffstore);
PHP_METHOD(RedisCluster, strlen);
PHP_METHOD(RedisCluster, ttl);
PHP_METHOD(RedisCluster, pttl);
PHP_METHOD(RedisCluster, zcard);
PHP_METHOD(RedisCluster, zscore);
PHP_METHOD(RedisCluster, zcount);
PHP_METHOD(RedisCluster, zrem);
PHP_METHOD(RedisCluster, zremrangebyscore);
PHP_METHOD(RedisCluster, zrank);
PHP_METHOD(RedisCluster, zrevrank);
PHP_METHOD(RedisCluster, zadd);
PHP_METHOD(RedisCluster, zincrby);
PHP_METHOD(RedisCluster, hlen);
PHP_METHOD(RedisCluster, hget);
PHP_METHOD(RedisCluster, hkeys);
PHP_METHOD(RedisCluster, hvals);
PHP_METHOD(RedisCluster, hmget);
PHP_METHOD(RedisCluster, hmset);
PHP_METHOD(RedisCluster, hdel);
PHP_METHOD(RedisCluster, hgetall);
PHP_METHOD(RedisCluster, hexists);
PHP_METHOD(RedisCluster, hincrby);
PHP_METHOD(RedisCluster, hincrbyfloat);
PHP_METHOD(RedisCluster, hset);
PHP_METHOD(RedisCluster, hsetnx);
PHP_METHOD(RedisCluster, incr);
PHP_METHOD(RedisCluster, decr);
PHP_METHOD(RedisCluster, incrby);
PHP_METHOD(RedisCluster, decrby);
PHP_METHOD(RedisCluster, incrbyfloat);
PHP_METHOD(RedisCluster, expire);
PHP_METHOD(RedisCluster, expireat);
PHP_METHOD(RedisCluster, pexpire);
PHP_METHOD(RedisCluster, pexpireat);
PHP_METHOD(RedisCluster, append);
PHP_METHOD(RedisCluster, getbit);
PHP_METHOD(RedisCluster, setbit);
PHP_METHOD(RedisCluster, bitop);
PHP_METHOD(RedisCluster, bitpos);
PHP_METHOD(RedisCluster, bitcount);
PHP_METHOD(RedisCluster, lget);
PHP_METHOD(RedisCluster, getrange);
PHP_METHOD(RedisCluster, ltrim);
PHP_METHOD(RedisCluster, lrange);
PHP_METHOD(RedisCluster, zremrangebyrank);
PHP_METHOD(RedisCluster, publish);
PHP_METHOD(RedisCluster, lset);
PHP_METHOD(RedisCluster, rename);
PHP_METHOD(RedisCluster, renamenx);
PHP_METHOD(RedisCluster, pfcount);
PHP_METHOD(RedisCluster, pfadd);
PHP_METHOD(RedisCluster, pfmerge);
PHP_METHOD(RedisCluster, restore);
PHP_METHOD(RedisCluster, setrange);
PHP_METHOD(RedisCluster, smove);
PHP_METHOD(RedisCluster, srandmember);
PHP_METHOD(RedisCluster, zrange);
PHP_METHOD(RedisCluster, zrevrange);
PHP_METHOD(RedisCluster, zrangebyscore);
PHP_METHOD(RedisCluster, zunionstore);
PHP_METHOD(RedisCluster, zinterstore);
PHP_METHOD(RedisCluster, sort);
PHP_METHOD(RedisCluster, object);
PHP_METHOD(RedisCluster, subscribe);
PHP_METHOD(RedisCluster, psubscribe);
PHP_METHOD(RedisCluster, unsubscribe);
PHP_METHOD(RedisCluster, punsubscribe);

/* Transactions */
PHP_METHOD(RedisCluster, multi);
PHP_METHOD(RedisCluster, exec);
PHP_METHOD(RedisCluster, discard);
PHP_METHOD(RedisCluster, watch);
PHP_METHOD(RedisCluster, unwatch);

/* Introspection */
PHP_METHOD(RedisCluster, getoption);
PHP_METHOD(RedisCluster, setoption);
PHP_METHOD(RedisCluster, _prefix);
PHP_METHOD(RedisCluster, _serialize);
PHP_METHOD(RedisCluster, _unserialize);

#endif
