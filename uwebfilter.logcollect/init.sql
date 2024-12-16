CREATE EXTENSION IF NOT EXISTS "uuid-ossp";
CREATE EXTENSION IF NOT EXISTS "pgcrypto";


CREATE DATABASE logsdb;
\c logsdb;



CREATE TABLE IF NOT EXISTS users (
	_id           UUID PRIMARY KEY DEFAULT uuid_generate_v4 (),
	username      TEXT NOT NULL CHECK (username <> ''),
	password      TEXT NOT NULL CHECK (password <> '')
);

CREATE OR REPLACE FUNCTION fn_users_authenticate(username TEXT, passwd TEXT)
        RETURNS TEXT
        RETURNS NULL ON NULL INPUT
        LANGUAGE SQL
        IMMUTABLE
	LEAKPROOF
	SECURITY DEFINER
AS $$
	SELECT users._id
	FROM users
	WHERE username = username
	  AND password = crypt(passwd, password)
	LIMIT 1
$$;



CREATE TABLE IF NOT EXISTS logs_v1 (
	user_id       UUID REFERENCES users(_id),
	srcip         TEXT NOT NULL CHECK (logs_v1.srcip <> ''),
	dstip         TEXT NOT NULL CHECK (logs_v1.dstip <> ''),
	srcport       INTEGER,
	dstport       INTEGER,
	proto         TEXT NOT NULL CHECK (logs_v1.proto <> ''),
	domain        TEXT NOT NULL CHECK (logs_v1.domain <> ''),
	path          TEXT NOT NULL,
	action        TEXT NOT NULL CHECK (logs_v1.action <> ''),
	category      INTEGER,
	application   INTEGER,
	blocktype     INTEGER,
	logtime       INTEGER
);
