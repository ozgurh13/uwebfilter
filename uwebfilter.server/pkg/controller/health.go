package controller

import (
	"github.com/gofiber/fiber/v2"
)

func HealthCheck(ctx *fiber.Ctx) error {
	return ctx.Status(200).JSON(fiber.Map{
		"success": true,
	})
}
