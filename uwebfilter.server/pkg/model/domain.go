package model

import (
	"encoding/json"
	"log/slog"
	"time"

	service "uwebfilter.server/pkg/service"

	"github.com/jellydator/ttlcache/v3"
)

type (
	domainTTLCacheKeyType = string
	domainTTLCacheValType = *DomainInfo
)

var (
	domainTTLCache *ttlcache.Cache[domainTTLCacheKeyType, domainTTLCacheValType]
)

type DomainInfo struct {
	Category     int      `json:"category"`
	Application  int      `json:"application"`
	TTL          uint     `json:"ttl"`
	CacheDomains []string `json:"cache_domains"`
}

func GetDomainInfo(domains []string) *DomainInfo {
	for _, domain := range domains {
		if domainInfoItem := domainTTLCache.Get(domain); domainInfoItem != nil {
			return domainInfoItem.Value()
		}
	}

	domainInfoStrings := service.RedisGetDomainInfos(domains)

	for index, domainInfoString := range domainInfoStrings {
		if domainInfoString != nil {
			var domainInfo DomainInfo

			err := json.Unmarshal([]byte(domainInfoString.(string)), &domainInfo)
			if err != nil {
				slog.Error("json.Unmarshal", "error", err.Error())
				return nil
			}

			var cacheDomains []string = domainInfo.CacheDomains
			if cacheDomains == nil || len(cacheDomains) == 0 {
				cacheDomains = []string{domains[index]}
			}

			for _, cacheDomain := range cacheDomains {
				domainTTLCache.Set(cacheDomain, &domainInfo, ttlcache.DefaultTTL)
			}

			return &domainInfo
		}
	}

	return nil
}

func UpdateDomainHitCount(domain string) {
	service.PostgresUpdateDomainHitCount(domain)
}

func init() {
	domainTTLCache = ttlcache.New[domainTTLCacheKeyType, domainTTLCacheValType](
		ttlcache.WithCapacity[domainTTLCacheKeyType, domainTTLCacheValType](65536),
		ttlcache.WithTTL[domainTTLCacheKeyType, domainTTLCacheValType](30*time.Minute),
		ttlcache.WithDisableTouchOnHit[domainTTLCacheKeyType, domainTTLCacheValType](),
	)

	go domainTTLCache.Start()

	go func() {
		for range time.NewTicker(time.Minute * 5).C {
			metrics := domainTTLCache.Metrics()
			slog.Info("domainTTLCache.Metrics",
				"Insertions", metrics.Insertions,
				"Hits", metrics.Hits,
				"Misses", metrics.Misses,
				"Evictions", metrics.Evictions,
			)
		}
	}()
}
