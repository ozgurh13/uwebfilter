package main

import (
	"bytes"
	"encoding/json"
	"io"
	"log/slog"
	"net/http"
	"strings"

	"github.com/jdkato/prose/v2"
)

type CategoryResponse struct {
	Prediction  string      `json:"Prediction"`
	Probability interface{} `json:"Probability"`
}

func CategoryToInt(category string) int {
	return 0 // TODO: lookup
}

func Categorize(cleanedText string) string {
	client := &http.Client{}
	url := "http://localhost:9005/inference"

	request := struct {
		Model string `json:"model"`
		Text  string `json:"text"`
	}{
		Model: "model-en",
		Text:  cleanedText,
	}

	jsonBytes, err := json.Marshal(request)
	if err != nil {
		slog.Error("json.Marshal", "error", err.Error())
		return ""
	}

	req, err := http.NewRequest("POST", url, bytes.NewBuffer(jsonBytes))
	if err != nil {
		slog.Error("http.NewRequest", "error", err.Error())
		return ""
	}

	resp, err := client.Do(req)
	if err != nil {
		slog.Error("client.Do", "error", err.Error())
		return ""
	}
	defer resp.Body.Close()

	body, _ := io.ReadAll(resp.Body)

	var pred CategoryResponse
	err = json.Unmarshal(body, &pred)
	if err != nil {
		slog.Error("json.Unmarshal", "error", err.Error())
		return ""
	}

	return pred.Prediction
}

func cleanText(text string) string {
	doc, _ := prose.NewDocument(text)

	keep := make([]string, 0)
	for _, tok := range doc.Tokens() {
		if keepTag(tok.Tag) {
			keep = append(keep, tok.Text)
		}
	}

	return strings.Join(keep, " ")
}

func keepTag(tag string) bool {
	unwantedTags := []string{"(", ")", ",", ":", ".", "'", "`", "#", "$"}

	for _, t := range unwantedTags {
		if tag == t {
			return false
		}
	}

	return true
}
