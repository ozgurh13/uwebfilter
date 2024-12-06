package middleware

import (
	service "uwebfilter.server/pkg/service"

	"github.com/gofiber/fiber/v2"
)

func PrometheusMiddleware(ctx *fiber.Ctx) error {
	ipaddr := ctx.IP()

	service.PrometheusIncIPAddrCount(ipaddr)

	return ctx.Next()
}
