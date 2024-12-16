package config

import (
	"log"
	"os"

	"github.com/joho/godotenv"
)

var (
	config = struct {
		jwtSecret []byte

		tls struct {
			certPath string
			keyPath  string
		}

		db struct {
			username string
			password string
			ipAddr   string
			port     string
			dbName   string
		}
	}{}
)

func init() {
	if err := godotenv.Load("files/.env"); err != nil {
		log.Fatal(err)
	}

	config.jwtSecret = []byte(getOrFatal("JWTSECRET"))

	config.tls.certPath = getOrFatal("CERTPEM")
	config.tls.keyPath = getOrFatal("KEYPEM")

	config.db.username = getOrFatal("PGUSERNAME")
	config.db.password = getOrFatal("PGPASSWORD")
	config.db.ipAddr = getOrFatal("PGIPADDR")
	config.db.port = getOrFatal("PGPORT")
	config.db.dbName = getOrFatal("PGDBNAME")
}

func getOrFatal(key string) string {
	value := os.Getenv(key)
	if value == "" {
		log.Fatalf(".env: %s isn't set", key)
	}
	return value
}

func GetJWTSecret() []byte {
	return config.jwtSecret
}

func GetTlsCertPath() string {
	return config.tls.certPath
}

func GetTlsKeyPath() string {
	return config.tls.keyPath
}

func GetPGUsername() string {
	return config.db.username
}

func GetPGPassword() string {
	return config.db.password
}

func GetPGIPAddr() string {
	return config.db.ipAddr
}

func GetPGPort() string {
	return config.db.port
}

func GetPGDBName() string {
	return config.db.dbName
}
