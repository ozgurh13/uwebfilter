package main

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

func PostgresSelectDomainsWithoutCategories(limit uint) []string {
	rows, err := db.Query(`
		SELECT domain FROM view_domains_without_categories
			LIMIT $1
	`, limit)
	if err != nil {
		slog.Error("db.Query", "error", err.Error())
		return make([]string, 0)
	}
	defer rows.Close()

	domains := make([]string, 0)
	for rows.Next() {
		var domain string
		err := rows.Scan(&domain)
		if err != nil {
			slog.Error("rows.Scan", "error", err.Error())
			continue
		}
		domains = append(domains, domain)
	}

	return domains
}

func PostgresInsertDomain(domain string, category int) {
	_, err := db.Query(`
		INSERT INTO domains_provisional (domain, category)
			VALUES ($1, $2)
	`, domain, category)
	if err != nil {
		slog.Error("db.Query", "error", err.Error())
	}
}

func PostgresInsertDomainFailed(domain string) {
	_, err := db.Query(`
		INSERT INTO domains_failed (domain)
			VALUES ($1)
	`, domain)
	if err != nil {
		slog.Error("db.Query", "error", err.Error())
	}
}
