package utils

import (
	"reflect"
	"testing"
)

type testIO struct {
	input  string
	output []string
}

func TestGetAllSubdomains(t *testing.T) {
	domainsIO := []testIO{
		{"rr5---sn-4g5ednsy.googlevideo.com", []string{
			"rr5---sn-4g5ednsy.googlevideo.com",
			"googlevideo.com",
			"com",
		}},
		{"facebook.com", []string{
			"facebook.com",
			"com",
		}},
		{"xyz", []string{
			"xyz",
		}},
		{"", []string{
			"",
		}},
	}

	for _, domainIO := range domainsIO {
		result := GetAllSubdomains(domainIO.input)
		if !reflect.DeepEqual(result, domainIO.output) {
			t.Fatalf(`GetAllSubdomains(%s) = %v, want %v`,
				domainIO.input, result, domainIO.output,
			)
		}
	}
}
