package main

import (
	"log"

	controller "uwebfilter.server/pkg/controller"

	"github.com/gofiber/fiber/v2"
)

func main() {
	app := fiber.New(fiber.Config{
		AppName: "uwebfilter.server",
		ErrorHandler: func(ctx *fiber.Ctx, err error) error {
			return ctx.Status(400).JSON(fiber.Map{
				"success": false,
			})
		},
	})

	app.Get("/health", controller.HealthCheck)
	app.Post("/query", controller.QueryPOST)

	log.Fatal(app.Listen(":3500"))
}
