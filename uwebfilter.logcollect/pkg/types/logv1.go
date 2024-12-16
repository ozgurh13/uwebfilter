package types

import (
	"net"
)

type LogV1 struct {
	SrcIP       net.IP `json:"srcip"`
	DstIP       net.IP `json:"dstip"`
	SrcPort     uint16 `json:"srcport"`
	DstPort     uint16 `json:"dstport"`
	Proto       string `json:"proto"`
	Domain      string `json:"domain"`
	Path        string `json:"path"`
	Action      string `json:"action"`
	Category    int    `json:"category"`
	Application int    `json:"application"`
	BlockType   int    `json:"blocktype"`
	LogTime     uint   `json:"logtime"`
}
