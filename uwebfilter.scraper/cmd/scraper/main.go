package main

import (
	"fmt"
	"strings"
)

func main() {
	domains := PostgresSelectDomainsWithoutCategories(10)
	fmt.Printf("domains: %v\n", domains)

	for _, domain := range domains {
		text := ScrapeDomain(domain)
		if text == nil {
			PostgresInsertDomainFailed(domain)
			continue
		}

		allText := strings.Join(text, " ")
		cleanedText := cleanText(allText)
		category := Categorize(cleanedText)
		categoryInt := CategoryToInt(category)

		fmt.Printf("domain(%s)\n  raw: %v\n  cleaned: %s\n  category: %s\n\n", domain, allText, cleanedText, category)
		PostgresInsertDomain(domain, categoryInt)
	}
}
