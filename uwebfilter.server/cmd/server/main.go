package main

import (
	"log"
	"net/http"

	controller "uwebfilter.server/pkg/controller"
	middleware "uwebfilter.server/pkg/middleware"

	"github.com/gofiber/fiber/v2"

	"github.com/prometheus/client_golang/prometheus/promhttp"
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

	app.Use(middleware.PrometheusMiddleware)

	app.Get("/health", controller.HealthCheck)
	app.Post("/query", controller.QueryPOST)

	go func() {
		http.Handle("/metrics", promhttp.Handler())
		log.Fatal(http.ListenAndServe(":8090", nil))
	}()

	log.Fatal(app.Listen(":3500"))
}
