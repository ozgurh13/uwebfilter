package main

import (
	"log"

	config "uwebfilter.logcollect/pkg/config"
	controller "uwebfilter.logcollect/pkg/controller"
	middleware "uwebfilter.logcollect/pkg/middleware"

	"github.com/gofiber/fiber/v2"
	"github.com/gofiber/fiber/v2/middleware/logger"
	"github.com/gofiber/fiber/v2/middleware/recover"
)

func main() {
	app := fiber.New(fiber.Config{
		AppName: "uwebfilter.logcollect",
		ErrorHandler: func(ctx *fiber.Ctx, err error) error {
			return ctx.SendStatus(fiber.StatusNotFound)
		},
	})

	setupApp(app)

	log.Fatal(app.ListenTLS(
		":3700",
		config.GetTlsCertPath(),
		config.GetTlsKeyPath(),
	))
}

func setupApp(app *fiber.App) {
	app.Use(recover.New())
	app.Use(logger.New())

	setupRoutesV1(app)
}

func setupRoutesV1(app *fiber.App) {
	v1 := app.Group("/v1")

	v1.Post("/auth", middleware.AuthRateLimit, controller.Auth)
	v1.Post("/ingest", middleware.JWTAuth, controller.IngestPOST)
}
