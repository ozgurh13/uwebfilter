package service

import (
	"context"
	"log/slog"

	"github.com/redis/go-redis/v9"
)

const (
	redisDBDomainInfo = 0
)

var (
	redisContextDomainInfo = context.Background()
	redisClientDomainInfo  = redis.NewClient(&redis.Options{
		Addr:     "localhost:6380",
		Password: "",
		DB:       redisDBDomainInfo,
	})
)

func RedisGetDomainInfos(domains []string) []interface{} {
	result, err := redisClientDomainInfo.MGet(redisContextDomainInfo, domains...).Result()
	if err != nil {
		slog.Error("redis.MGet", "error", err.Error())
		return make([]interface{}, 0)
	}

	return result
}
