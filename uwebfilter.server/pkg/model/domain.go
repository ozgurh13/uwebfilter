package model

import (
	"encoding/json"
	"log/slog"

	service "uwebfilter.server/pkg/service"
)

type DomainInfo struct {
	Category     int      `json:"category"`
	Application  int      `json:"application"`
	TTL          uint     `json:"ttl"`
	CacheDomains []string `json:"cache_domains"`
}

func GetDomainInfo(domains []string) *DomainInfo {
	domainInfoStrings := service.RedisGetDomainInfos(domains)

	for _, domainInfoString := range domainInfoStrings {
		if domainInfoString != nil {
			var domainInfo DomainInfo

			err := json.Unmarshal([]byte(domainInfoString.(string)), &domainInfo)
			if err != nil {
				slog.Error("json.Unmarshal", "error", err.Error())
				return nil
			}

			return &domainInfo
		}
	}

	return nil
}

func UpdateDomainHitCount(domain string) {
	service.PostgresUpdateDomainHitCount(domain)
}
