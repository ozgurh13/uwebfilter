package controller

import (
	"log/slog"
	"time"

	config "uwebfilter.logcollect/pkg/config"
	service "uwebfilter.logcollect/pkg/service"

	"github.com/gofiber/fiber/v2"
	"github.com/golang-jwt/jwt/v5"
)

func Auth(ctx *fiber.Ctx) error {
	var (
		authRequest = struct {
			User string `json:"username"`
			Pass string `json:"password"`
		}{}
	)

	// all requests should take 1 second to respond
	timeout := time.After(1 * time.Second)
	defer func() {
		<-timeout
	}()

	if err := ctx.BodyParser(&authRequest); err != nil {
		return ctx.SendStatus(fiber.StatusUnauthorized)
	}

	userID := service.PostgresSelectUserUUID(
		authRequest.User,
		authRequest.Pass,
	)
	if userID == "" {
		return ctx.SendStatus(fiber.StatusUnauthorized)
	}

	claims := jwt.MapClaims{
		"user_id": userID,
		"exp":     time.Now().Add(24 * time.Hour).Unix(),
	}

	token := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)

	jwtSecret := config.GetJWTSecret()
	jwtToken, err := token.SignedString(jwtSecret)
	if err != nil {
		slog.Error("token.SignedString", "error", err.Error())
		return ctx.SendStatus(fiber.StatusInternalServerError)
	}

	return ctx.JSON(fiber.Map{"token": jwtToken})
}

func jwtTokenGetUserID(ctx *fiber.Ctx) string {
	user := ctx.Locals("user").(*jwt.Token)
	claims := user.Claims.(jwt.MapClaims)
	name := claims["user_id"].(string)
	return name
}
