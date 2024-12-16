package types

import (
	"encoding/json"
	"fmt"
)

const (
	LogVersion1 = 1
)

type Logs struct {
	LogBase
}

type LogBase struct {
	Version uint `json:"version"`
	Logs    any  `json:"logs"`
}

func (logBase *LogBase) UnmarshalJSON(data []byte) error {
	type Alias LogBase
	aux := &struct {
		Logs json.RawMessage `json:"logs"`
		*Alias
	}{
		Alias: (*Alias)(logBase),
	}

	if err := json.Unmarshal(data, aux); err != nil {
		return err
	}

	logBase.Version = aux.Version

	return logBase.unmarshalJSON(aux.Logs)
}

func (logBase *LogBase) unmarshalJSON(auxLogs json.RawMessage) error {
	switch logBase.Version {
	case LogVersion1:
		var logs []LogV1
		if err := json.Unmarshal(auxLogs, &logs); err != nil {
			return err
		}
		logBase.Logs = logs

	default:
		return fmt.Errorf("unsupported version: %d", logBase.Version)
	}

	return nil
}
