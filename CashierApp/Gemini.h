// Gemini.h - sends SALES information to Gemini AI and gets a text recommendation.
#ifndef GEMINI_H
#define GEMINI_H

#include <string>

// Builds the question (prompt) from the sales data in MySQL. Returns "" if there are no sales.
std::string BuildSalesPrompt();

// Sends the prompt to Gemini. true = success (answer filled), false = failed (error filled).
bool AskGemini(const std::string& prompt, std::wstring& answer, std::string& error);

// Converts UTF-8 text to wide text (used by the GUI for Gemini text and labels).
std::wstring Utf8ToWide(const std::string& text);

#endif
