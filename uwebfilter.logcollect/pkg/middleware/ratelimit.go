package middleware

import (
	"time"

	"github.com/gofiber/fiber/v2/middleware/limiter"
)

var (
	AuthRateLimit = limiter.New(limiter.Config{
		Max:               5,
		Expiration:        1 * time.Minute,
		LimiterMiddleware: limiter.SlidingWindow{},
	})
)
