import argparse
import atexit
import json

import psycopg2
import redis


CLIENT_COUNT = 1  # read from outside source

r = redis.Redis(
    host='localhost',
    port=6379,
    decode_responses=True,
)

conn = psycopg2.connect(
    host="localhost",
    port=5432,
    dbname='domainsdb',
    user="postgres",
    password="password",
)
atexit.register(conn.close)



# this should run as a cron job, daily
def ttls_update(_args):
    '''
    update ttl of domains based on query frequency
    '''
    cur = conn.cursor()

    # number of time the domain has been queried in the past day
    cur.execute('''
        SELECT x.domain, x.cnt, domains.ttl FROM (
            SELECT hc.domain AS domain, COUNT(1) AS cnt FROM hitcount hc
            WHERE hc._datetime > CURRENT_TIMESTAMP - INTERVAL '1 DAY'
            GROUP BY hc.domain
        ) x
        JOIN domains ON x.domain = domains.domain
    ''')
    results = cur.fetchall()

    # update logic goes here
    updated = []
    for (domain, count, ttl) in results:
        print(f'> [ttl:{ttl}] `{domain}`: count({count})')

        # some random heuristic, may change later
        #     a domain shouldn't be queried more than 30 times in a day per client
        #     this means that if the domain was queried the moment it expires
        #         day 1: ttl=300  queries=288
        #         day 2: ttl=840  queries=102
        #         day 3: ttl=1020 queries=84
        #         day 4: ttl=1140 queries=75
        #      and so on ...
        #      we also don't want the domain to overstay its welcome
        #          so we try to keep the ttl as low as possible
        minutes = count // (30 * CLIENT_COUNT)
        inc = minutes * 60
        if inc > 0:
            cur.execute('''
                UPDATE domains SET ttl = ttl + %s
                    WHERE domain = %s
            ''', [inc, domain])
            print(f'+> ttl: {ttl} + {inc}')

            updated.append(domain)

    cur.execute('''
        SELECT domain, ttl, category, application, cache_domains FROM domains
            WHERE domain = ANY(%s)
    ''', [updated])
    result = cur.fetchall()
    for (domain, ttl, category, application, cache_domains) in result:
        key = domain
        value = json.dumps({
            'category': category,
            'application': application,
            'ttl': ttl,
            'cache_domains': cache_domains,
        })
        print(f'''{key = } : {value = }''')
        r.set(key, value)

    conn.commit()
    cur.close()




def db_domain_add(args):
    '''
    add domain to database
    '''
    cur = conn.cursor()

    domain = args.domain
    ttl = args.ttl
    category = args.category
    application = args.application
    cache_domains = args.cache_domains

    cur.execute('''
        INSERT INTO domains (domain, ttl, category, application, cache_domains)
           VALUES (%s, %s, %s, %s, %s);
    ''', [domain, ttl, category, application, cache_domains])

    key = domain
    value = json.dumps({
        'category': category,
        'application': application,
        'ttl': ttl,
        'cache_domains': cache_domains,
    })
    r.set(key, value)

    print(f'[ttl:{ttl}] {domain}: category({category}) application({application}) domains({cache_domains})')

    conn.commit()
    cur.close()


def db_domain_del(args):
    '''
    delete domain from database
    '''
    cur = conn.cursor()

    cur.execute('''
        DELETE FROM domains
           WHERE domain = %s
    ''', [args.domain])

    conn.commit()
    cur.close()


def db_domain_get(args):
    '''
    get domain from database
    '''
    cur = conn.cursor()

    cur.execute('''
        SELECT domain, category, application, ttl, cache_domains FROM domains
           WHERE domain = %s
    ''', [args.domain])

    try:
        (domain, category, application, ttl, domains) = cur.fetchone()
        print(f'[ttl:{ttl}] {domain}: category({category}) application({application}) domains({domains})')
    except TypeError:
        print(f'`{args.domain}` not in database')

    conn.commit()
    cur.close()


def db_dump_to_redis(_args):
    '''
    populate redis with domains from db
    '''
    cur = conn.cursor()

    cur.execute('''
        SELECT domain, ttl, category, application, cache_domains FROM domains
    ''')
    result = cur.fetchall()
    for (domain, ttl, category, application, cache_domains) in result:
        key = domain
        value = json.dumps({
            'category': category,
            'application': application,
            'ttl': ttl,
            'cache_domains': cache_domains,
        })
        print(f'''{key = } : {value = }''')
        r.set(key, value)



# update domain ttls (postgres), update redis accordingly
#     ttls-update
# database management (postgres)
#     domain-add DOMAIN [--ttl TTL] [--category CATEGORY] [--application APPLICATION] [--cache-domains DOMAIN1 DOMAIN2 ...]
#     domain-del DOMAIN
#     domain-get DOMAIN
# populate redis
#     db-dump-to-redis

if __name__ == '__main__':
    cmdline = argparse.ArgumentParser(description='manage domain database and cache')
    subcmdline = cmdline.add_subparsers(required=True)

    _ttls_update = subcmdline.add_parser('ttls-update')
    _ttls_update.set_defaults(func=ttls_update)

    _db_dump_to_redis = subcmdline.add_parser('db-dump-to-redis')
    _db_dump_to_redis.set_defaults(func=db_dump_to_redis)

    _domain_add = subcmdline.add_parser('domain-add')
    _domain_add.add_argument('domain', type=str)
    _domain_add.add_argument('--ttl', type=int, default=300)
    _domain_add.add_argument('--category', type=int, default=0)
    _domain_add.add_argument('--application', type=int, default=0)
    _domain_add.add_argument('--cache-domains', nargs='*', default=[])
    _domain_add.set_defaults(func=db_domain_add)

    _domain_del = subcmdline.add_parser('domain-del')
    _domain_del.add_argument('domain', type=str)
    _domain_del.set_defaults(func=db_domain_del)

    _domain_get = subcmdline.add_parser('domain-get')
    _domain_get.add_argument('domain', type=str)
    _domain_get.set_defaults(func=db_domain_get)

    args = cmdline.parse_args()
    args.func(args)

