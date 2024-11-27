package service

import (
	"database/sql"
	"log"
	"log/slog"

	_ "github.com/lib/pq"
)

const (
	dbName string = `postgresql://postgres:password@localhost:5432/domainsdb?sslmode=disable`
)

var (
	db *sql.DB
)

func init() {
	var err error
	db, err = sql.Open("postgres", dbName)
	if err != nil {
		log.Fatal(err)
	}
}

func PostgresUpdateDomainHitCount(domain string) {
	_, err := db.Exec("INSERT INTO hitcount (domain) VALUES ($1)", domain)
	if err != nil {
		slog.Error("postgres.Exec", "error", err.Error())
	}
}
