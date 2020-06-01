import redis
import random
import threading
import time

POOL = redis.ConnectionPool(host='192.168.64.5',
                            port='6379',
                            db=0,
                            decode_responses=True,
                            encoding='utf-8',
                            socket_connect_timeout=2)


def _get():
    r = redis.StrictRedis(connection_pool=POOL)
    key = "key" + str(random.randint(0, 9))
    val = r.get(key)
    print("get key = " + key + ", val = " + ("None" if val is None else val))


def _set():
    r = redis.StrictRedis(connection_pool=POOL)
    key = "key" + str(random.randint(0, 9))
    val = "value is a long string with a random number of " + str(random.randint(0, 999))
    r.setex(name=key, value=val, time=10)
    # r.set(name=key, value=val)


def _del():
    r = redis.StrictRedis(connection_pool=POOL)
    key = "key" + str(random.randint(0, 9))
    r.delete(key)


def _thread_for_a_while(thread_number: int, to_do, total_seconds: int, sleep_least_ms: int, sleep_most_ms: int):
    def thread_func(start_time):

        while True:
            curr_time = time.time()
            if curr_time - start_time > total_seconds:
                break
            else:
                to_do()
                sleep = random.randint(sleep_least_ms, sleep_most_ms) / 1000
                time.sleep(sleep)

    ts = []
    for _ in range(thread_number):
        t = threading.Thread(target=thread_func, args=[time.time()])
        ts.append(t)
        t.start()

    return ts


def _main():
    all_ts = []
    ts = _thread_for_a_while(4, _get, 50, 5, 50)
    all_ts.extend(ts)
    ts = _thread_for_a_while(3, _set, 50, 5, 50)
    all_ts.extend(ts)
    ts = _thread_for_a_while(2, _del, 50, 5, 50)
    all_ts.extend(ts)

    for i in range(len(all_ts)):
        all_ts[i].join()


if __name__ == '__main__':
    _main()
