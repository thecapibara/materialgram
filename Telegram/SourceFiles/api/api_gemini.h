/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/basic_types.h"

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <rpl/producer.h>
#include <optional>
#include <vector>

namespace Api::Gemini {

inline constexpr auto kDefaultModel = "gemini-3.5-flash-lite";
inline constexpr auto kPrefApiKey = "gemini_api_key";
inline constexpr auto kPrefModel = "gemini_model";
inline constexpr auto kPrefSystemPrompt = "gemini_system_prompt";
inline constexpr auto kPrefTemperature = "gemini_temperature";
inline constexpr auto kPrefEnabled = "gemini_enabled";
inline constexpr auto kPrefTargetLanguage = "gemini_target_lang";

struct RequestOptions {
	QString prompt;
	QString systemInstruction;
	QString modelOverride;
	std::optional<float64> temperature;
	QByteArray imageBytes;
	QString imageMimeType;
	bool useCustomSystemPrompt = false;
};

struct Response {
	QString text;
	QString error;
	bool success = false;
};

struct ModelInfo {
	QString id;
	QString displayName;
	QString description;
};

[[nodiscard]] bool IsConfigured();
[[nodiscard]] bool Enabled();
void SetEnabled(bool enabled);
[[nodiscard]] rpl::producer<> Updated();

[[nodiscard]] QString ApiKey();
void SetApiKey(const QString &key);

[[nodiscard]] QString Model();
void SetModel(const QString &model);

[[nodiscard]] QString SystemPrompt();
void SetSystemPrompt(const QString &prompt);

[[nodiscard]] float64 Temperature();
void SetTemperature(float64 temperature);

[[nodiscard]] QString TargetLanguage();
void SetTargetLanguage(const QString &lang);

void GenerateContent(
	RequestOptions options,
	Fn<void(Response)> callback);

void FetchModels(
	Fn<void(std::vector<ModelInfo> models, QString error)> callback);

[[nodiscard]] QString SummarizePrompt(const QString &text);
[[nodiscard]] QString TranslatePrompt(
	const QString &text,
	const QString &targetLanguage);
[[nodiscard]] QString OcrPrompt();
[[nodiscard]] QString DescribeImagePrompt(const QString &language);

} // namespace Api::Gemini
