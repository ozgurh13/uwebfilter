package utils

import (
	"strings"
)

func GetAllSubdomains(domain string) []string {
	subdomains := strings.Split(domain, ".")

	allSubdomains := make([]string, len(subdomains))
	for i, _ := range subdomains {
		allSubdomains[i] = strings.Join(subdomains[i:], ".")
	}

	return allSubdomains
}
