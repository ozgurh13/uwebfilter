package service

import (
	"database/sql"
	"fmt"
	"log"
	"log/slog"

	_ "github.com/lib/pq"

	config "uwebfilter.logcollect/pkg/config"
	types "uwebfilter.logcollect/pkg/types"
)

var (
	db *sql.DB
)

func init() {
	dbConn := fmt.Sprintf(
		`postgresql://%s:%s@%s:%s/%s?sslmode=disable`,
		config.GetPGUsername(),
		config.GetPGPassword(),
		config.GetPGIPAddr(),
		config.GetPGPort(),
		config.GetPGDBName(),
	)
	var err error
	db, err = sql.Open("postgres", dbConn)
	if err != nil {
		log.Fatal(err)
	}
}

func PostgresSelectUserUUID(username string, password string) string {
	var uuid string

	err := db.QueryRow(
		`SELECT fn_users_authenticate($1, $2)`,
		username,
		password,
	).Scan(&uuid)
	if err != nil {
		slog.Error("db.QueryRow", "error", err.Error())
		return ""
	}

	return uuid
}

func PostgresInsertLogsV1(userID string, logs []types.LogV1) {
	for _, log := range logs {
		_, err := db.Exec(`
			INSERT INTO logs_v1 (
				user_id, srcip, dstip, srcport, dstport,
				proto, domain, path, action, category,
				application, blocktype, logtime
			) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13)
			`,
			userID,
			log.SrcIP.String(),
			log.DstIP.String(),
			log.SrcPort,
			log.DstPort,
			log.Proto,
			log.Domain,
			log.Path,
			log.Action,
			log.Category,
			log.Application,
			log.BlockType,
			log.LogTime,
		)
		if err != nil {
			slog.Error("postgres.Exec", "error", err.Error())
		}
	}
}
