package controller

import (
	model "uwebfilter.logcollect/pkg/model"
	types "uwebfilter.logcollect/pkg/types"

	"github.com/gofiber/fiber/v2"
)

func IngestPOST(ctx *fiber.Ctx) error {
	var (
		logs types.Logs
	)

	if err := ctx.BodyParser(&logs); err != nil {
		return ctx.SendStatus(400)
	}

	userID := jwtTokenGetUserID(ctx)
	model.LogsSaveToDB(userID, logs)

	return ctx.SendStatus(fiber.StatusCreated)
}
