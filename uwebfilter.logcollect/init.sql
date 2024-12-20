CREATE DATABASE logsdb;
\c logsdb;


CREATE EXTENSION IF NOT EXISTS "uuid-ossp";
CREATE EXTENSION IF NOT EXISTS "pgcrypto";


CREATE TABLE IF NOT EXISTS users (
	_id           UUID PRIMARY KEY DEFAULT uuid_generate_v4 (),
	username      TEXT NOT NULL UNIQUE CHECK (username <> ''),
	password      TEXT NOT NULL CHECK (password <> ''),
	created_at    TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE OR REPLACE FUNCTION fn_users_create(username TEXT, passwd TEXT)
	RETURNS UUID
	RETURNS NULL ON NULL INPUT
	LANGUAGE SQL
	SECURITY DEFINER
AS $$
	INSERT INTO users (username, password)
	VALUES (username, crypt(passwd, gen_salt('bf')))
	RETURNING _id
$$;


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
