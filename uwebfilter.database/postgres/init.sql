CREATE DATABASE domainsdb;
\c domainsdb;


CREATE TABLE IF NOT EXISTS categories (
	id INTEGER PRIMARY KEY,
	name TEXT NOT NULL CHECK (name <> '')
);
INSERT INTO categories VALUES
	(0, 'unknown');


CREATE TABLE IF NOT EXISTS application (
	id INTEGER PRIMARY KEY,
	name TEXT NOT NULL CHECK (name <> '')
);
INSERT INTO application VALUES
	(0, 'unknown');


CREATE TABLE IF NOT EXISTS domains (
	_id SERIAL PRIMARY KEY,
	domain TEXT NOT NULL UNIQUE CHECK (domain <> ''),
	category INTEGER NOT NULL DEFAULT 0 REFERENCES categories(id),
	application INTEGER NOT NULL DEFAULT 0 REFERENCES application(id),
	ttl INTEGER DEFAULT 300 CHECK (ttl > 0),
	cache_domains TEXT[] NOT NULL DEFAULT '{}'
);

CREATE TABLE IF NOT EXISTS domains_provisional (
	_id SERIAL PRIMARY KEY,
	domain TEXT NOT NULL UNIQUE CHECK (domain <> ''),
	category INTEGER NOT NULL DEFAULT 0 REFERENCES categories(id),
	application INTEGER NOT NULL DEFAULT 0 REFERENCES application(id),
	ttl INTEGER DEFAULT 300 CHECK (ttl > 0),
	cache_domains TEXT[] NOT NULL DEFAULT '{}'
);


CREATE TABLE IF NOT EXISTS hitcount (
	_id SERIAL PRIMARY KEY,
	domain TEXT NOT NULL CHECK (domain <> ''),
	_datetime TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
