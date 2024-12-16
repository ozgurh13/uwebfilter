package model

import (
	service "uwebfilter.logcollect/pkg/service"
	types "uwebfilter.logcollect/pkg/types"
)

func LogsSaveToDB(userID string, logs types.Logs) {
	switch logs.Version {
	case types.LogVersion1:
		service.PostgresInsertLogsV1(userID, logs.Logs.([]types.LogV1))
	}
}
