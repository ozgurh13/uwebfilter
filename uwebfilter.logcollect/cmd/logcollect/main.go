package main

import (
	"log"

	config "uwebfilter.logcollect/pkg/config"
	controller "uwebfilter.logcollect/pkg/controller"
	middleware "uwebfilter.logcollect/pkg/middleware"

	"github.com/gofiber/fiber/v2"
)

func main() {
	app := fiber.New(fiber.Config{
		AppName: "uwebfilter.logcollect",
		ErrorHandler: func(ctx *fiber.Ctx, err error) error {
			return ctx.Status(400).JSON(fiber.Map{
				"success": false,
			})
		},
	})

	setupRoutes(app)

	log.Fatal(app.ListenTLS(
		":3700",
		config.GetTlsCertPath(),
		config.GetTlsKeyPath(),
	))
}

func setupRoutes(app *fiber.App) {
	app.Post("/auth", controller.Auth)
	app.Post("/ingest", middleware.JWTAuth, controller.IngestPOST)
}
