package controller

import (
	model "uwebfilter.server/pkg/model"
	utils "uwebfilter.server/pkg/utils"

	"github.com/gofiber/fiber/v2"
)

func QueryPOST(ctx *fiber.Ctx) error {
	var (
		queryRequest struct {
			Domain string `json:"domain"`
		}

		queryResponse struct {
			Category     int      `json:"category"`
			Application  int      `json:"application"`
			TTL          uint     `json:"ttl"`
			CacheDomains []string `json:"cache_domains"`
		}
	)

	if err := ctx.BodyParser(&queryRequest); err != nil {
		return ctx.Status(400).SendString("invalid query")
	}

	if queryRequest.Domain == "" {
		return ctx.Status(400).SendString("domain empty")
	}

	model.UpdateDomainHitCount(queryRequest.Domain)

	allSubdomains := utils.GetAllSubdomains(queryRequest.Domain)

	domainInfo := model.GetDomainInfo(allSubdomains)
	if domainInfo != nil {
		queryResponse.Category = domainInfo.Category
		queryResponse.Application = domainInfo.Application
		queryResponse.TTL = domainInfo.TTL
		queryResponse.CacheDomains = domainInfo.CacheDomains
	}

	return ctx.Status(200).JSON(queryResponse)
}
