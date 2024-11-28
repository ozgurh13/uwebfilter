package main

import (
	"fmt"
	"log/slog"

	"github.com/gocolly/colly/v2"
)

func ScrapeDomain(domain string) []string {
	results := make([]string, 0)

	c := colly.NewCollector()
	c.OnHTML("p, h1, h2, h3, h4, h5, h6", func(e *colly.HTMLElement) {
		domain := e.Request.URL.Host
		// TODO: this domain might be different from the one we have, add it to `cache_domains`
		fmt.Printf("domain: %s\n", domain)
		results = append(results, e.Text)
	})

	httpsDomain := fmt.Sprintf("https://%s", domain)
	err := c.Visit(httpsDomain)
	if err == nil {
		return results
	}
	slog.Error("c.Visit", "proto", "https", "domain", domain, "error", err.Error())

	httpDomain := fmt.Sprintf("http://%s", domain)
	err = c.Visit(httpDomain)
	if err == nil {
		return results
	}

	slog.Error("c.Visit", "proto", "http", "domain", domain, "error", err.Error())

	return nil
}
