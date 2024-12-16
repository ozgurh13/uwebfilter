package middleware

import (
	config "uwebfilter.logcollect/pkg/config"

	"github.com/gofiber/fiber/v2"

	jwtware "github.com/gofiber/contrib/jwt"
)

var (
	JWTAuth = jwtware.New(jwtware.Config{
		SigningKey: jwtware.SigningKey{Key: config.GetJWTSecret()},
		ErrorHandler: func(ctx *fiber.Ctx, _ error) error {
			return ctx.SendStatus(401)
		},
	})
)
