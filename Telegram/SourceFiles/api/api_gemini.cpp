/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "api/api_gemini.h"

#include "core/application.h"
#include "core/core_settings.h"

#include <rpl/event_stream.h>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QUrl>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

namespace Api::Gemini {
namespace {

[[nodiscard]] QNetworkAccessManager *NetworkManager() {
	static const auto manager = new QNetworkAccessManager(&Core::App());
	return manager;
}

[[nodiscard]] QString CleanModelName(QString model) {
	if (model.startsWith(u"models/"_q)) {
		return model.mid(7);
	}
	return model;
}

[[nodiscard]] rpl::event_stream<> &UpdatedStream() {
	static rpl::event_stream<> stream;
	return stream;
}

} // namespace

rpl::producer<> Updated() {
	return UpdatedStream().events();
}

bool IsConfigured() {
	return !ApiKey().trimmed().isEmpty();
}

bool Enabled() {
	return IsConfigured()
		&& Core::App().settings().readPref<bool>(kPrefEnabled, true);
}

void SetEnabled(bool enabled) {
	Core::App().settings().writePref<bool>(kPrefEnabled, enabled);
	Core::App().saveSettingsDelayed();
	UpdatedStream().fire({});
}

QString ApiKey() {
	return Core::App().settings().readPref<QString>(kPrefApiKey).trimmed();
}

void SetApiKey(const QString &key) {
	Core::App().settings().writePref<QString>(kPrefApiKey, key.trimmed());
	Core::App().saveSettingsDelayed();
	UpdatedStream().fire({});
}

QString Model() {
	const auto saved = Core::App().settings().readPref<QString>(kPrefModel);
	return saved.isEmpty() ? QString::fromUtf8(kDefaultModel) : saved;
}

void SetModel(const QString &model) {
	Core::App().settings().writePref<QString>(kPrefModel, CleanModelName(model));
	Core::App().saveSettingsDelayed();
	UpdatedStream().fire({});
}

QString SystemPrompt() {
	return Core::App().settings().readPref<QString>(kPrefSystemPrompt);
}

void SetSystemPrompt(const QString &prompt) {
	Core::App().settings().writePref<QString>(kPrefSystemPrompt, prompt);
	Core::App().saveSettingsDelayed();
	UpdatedStream().fire({});
}

float64 Temperature() {
	const auto value = Core::App().settings().readPref<int>(kPrefTemperature, 5);
	return std::clamp(value, 0, 10) / 10.0;
}

void SetTemperature(float64 temperature) {
	const auto value = int(std::round(std::clamp(temperature, 0.0, 1.0) * 10));
	Core::App().settings().writePref<int>(kPrefTemperature, value);
	Core::App().saveSettingsDelayed();
	UpdatedStream().fire({});
}

QString TargetLanguage() {
	return Core::App().settings().readPref<QString>(kPrefTargetLanguage, u"en"_q);
}

void SetTargetLanguage(const QString &lang) {
	Core::App().settings().writePref<QString>(kPrefTargetLanguage, lang);
	Core::App().saveSettingsDelayed();
	UpdatedStream().fire({});
}

void GenerateContent(
		RequestOptions options,
		Fn<void(Response)> callback) {
	const auto key = ApiKey();
	if (key.isEmpty()) {
		if (callback) {
			callback({
				.error = u"Gemini API key is not configured."_q,
				.success = false,
			});
		}
		return;
	}

	const auto model = options.modelOverride.isEmpty()
		? Model()
		: CleanModelName(options.modelOverride);
	const auto urlString = u"https://generativelanguage.googleapis.com/v1beta/models/%1:generateContent?key=%2"_q
		.arg(model, key);

	auto request = QNetworkRequest(QUrl(urlString));
	request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/json"_q);

	QJsonObject root;

	// System instruction
	const auto systemPrompt = !options.systemInstruction.isEmpty()
		? options.systemInstruction
		: (options.useCustomSystemPrompt ? SystemPrompt() : QString());
	if (!systemPrompt.isEmpty()) {
		QJsonObject sysPart;
		sysPart[u"text"_q] = systemPrompt;
		QJsonArray sysParts;
		sysParts.append(sysPart);
		QJsonObject sysInstruction;
		sysInstruction[u"parts"_q] = sysParts;
		root[u"system_instruction"_q] = sysInstruction;
	}

	// Content parts
	QJsonArray parts;
	if (!options.imageBytes.isEmpty()) {
		QJsonObject inlineData;
		inlineData[u"mime_type"_q] = options.imageMimeType.isEmpty()
			? u"image/jpeg"_q
			: options.imageMimeType;
		inlineData[u"data"_q] = QString::fromUtf8(options.imageBytes.toBase64());
		QJsonObject imgPart;
		imgPart[u"inline_data"_q] = inlineData;
		parts.append(imgPart);
	}
	if (!options.prompt.isEmpty()) {
		QJsonObject textPart;
		textPart[u"text"_q] = options.prompt;
		parts.append(textPart);
	}

	QJsonObject contentObj;
	contentObj[u"role"_q] = u"user"_q;
	contentObj[u"parts"_q] = parts;

	QJsonArray contents;
	contents.append(contentObj);
	root[u"contents"_q] = contents;

	// Generation configuration
	QJsonObject genConfig;
	genConfig[u"temperature"_q] = options.temperature.value_or(Temperature());
	genConfig[u"maxOutputTokens"_q] = 4096;
	root[u"generationConfig"_q] = genConfig;

	// Safety settings
	static const QString categories[] = {
		u"HARM_CATEGORY_HARASSMENT"_q,
		u"HARM_CATEGORY_HATE_SPEECH"_q,
		u"HARM_CATEGORY_SEXUALLY_EXPLICIT"_q,
		u"HARM_CATEGORY_DANGEROUS_CONTENT"_q,
	};
	QJsonArray safety;
	for (const auto &category : categories) {
		QJsonObject rule;
		rule[u"category"_q] = category;
		rule[u"threshold"_q] = u"BLOCK_NONE"_q;
		safety.append(rule);
	}
	root[u"safetySettings"_q] = safety;

	const auto body = QJsonDocument(root).toJson(QJsonDocument::Compact);
	auto *reply = NetworkManager()->post(request, body);

	QObject::connect(reply, &QNetworkReply::finished, [reply, callback = std::move(callback)] {
		const auto status = reply->attribute(
			QNetworkRequest::HttpStatusCodeAttribute).toInt();
		const auto raw = reply->readAll();
		reply->deleteLater();

		if (!callback) {
			return;
		}

		auto errorDoc = QJsonDocument::fromJson(raw);
		if (status < 200 || status >= 300 || errorDoc.object().contains(u"error"_q)) {
			const auto errObj = errorDoc.object()[u"error"_q].toObject();
			const auto msg = errObj[u"message"_q].toString();
			callback({
				.error = msg.isEmpty()
					? u"HTTP Error %1"_q.arg(status)
					: msg,
				.success = false,
			});
			return;
		}

		const auto candidates = errorDoc.object()[u"candidates"_q].toArray();
		if (candidates.isEmpty()) {
			callback({
				.error = u"No response candidates returned."_q,
				.success = false,
			});
			return;
		}

		const auto first = candidates[0].toObject();
		const auto parts = first[u"content"_q].toObject()[u"parts"_q].toArray();
		if (parts.isEmpty()) {
			callback({
				.error = u"Empty response content."_q,
				.success = false,
			});
			return;
		}

		QString resultText;
		for (const auto &part : parts) {
			resultText += part.toObject()[u"text"_q].toString();
		}

		callback({
			.text = resultText.trimmed(),
			.success = true,
		});
	});
}

void FetchModels(
		Fn<void(std::vector<ModelInfo> models, QString error)> callback) {
	const auto key = ApiKey();
	if (key.isEmpty()) {
		if (callback) {
			callback({}, u"Gemini API key is not configured."_q);
		}
		return;
	}

	const auto urlString = u"https://generativelanguage.googleapis.com/v1beta/models?key=%1"_q
		.arg(key);
	auto request = QNetworkRequest(QUrl(urlString));
	auto *reply = NetworkManager()->get(request);

	QObject::connect(reply, &QNetworkReply::finished, [reply, callback = std::move(callback)] {
		const auto status = reply->attribute(
			QNetworkRequest::HttpStatusCodeAttribute).toInt();
		const auto raw = reply->readAll();
		reply->deleteLater();

		if (!callback) {
			return;
		}

		const auto doc = QJsonDocument::fromJson(raw);
		if (status < 200 || status >= 300 || doc.object().contains(u"error"_q)) {
			const auto errObj = doc.object()[u"error"_q].toObject();
			const auto msg = errObj[u"message"_q].toString();
			callback({}, msg.isEmpty() ? u"HTTP Error %1"_q.arg(status) : msg);
			return;
		}

		std::vector<ModelInfo> result;
		const auto models = doc.object()[u"models"_q].toArray();
		for (const auto &entry : models) {
			const auto obj = entry.toObject();
			const auto name = CleanModelName(obj[u"name"_q].toString());
			const auto displayName = obj[u"displayName"_q].toString();
			const auto description = obj[u"description"_q].toString();
			const auto methods = obj[u"supportedGenerationMethods"_q].toArray();

			bool canGenerate = false;
			for (const auto &method : methods) {
				if (method.toString() == u"generateContent"_q) {
					canGenerate = true;
					break;
				}
			}
			if (canGenerate && !name.isEmpty()) {
				result.push_back({
					.id = name,
					.displayName = displayName.isEmpty() ? name : displayName,
					.description = description,
				});
			}
		}

		callback(std::move(result), QString());
	});
}

QString SummarizePrompt(const QString &text) {
	return u"Summarize the following text briefly and clearly in the same language it is written in. Do not include any introductions, labels, or closing remarks. Return only the summary as plain text. The text to summarize:\n\n%1"_q
		.arg(text);
}

QString TranslatePrompt(
		const QString &text,
		const QString &targetLanguage) {
	return u"You are a professional translator. Translate all input text into %1 accurately and naturally, preserving the original meaning, tone, and context. Do not add explanations or comments. Just return the translated text without any introduction or closing phrases. Here is the text to translate into %1:\n\n%2"_q
		.arg(targetLanguage, text);
}

QString OcrPrompt() {
	return u"You are an OCR assistant. Extract and return only the exact text written in the image. Preserve the original language and formatting as much as possible. Do not translate, interpret, or explain the text. Output only the plain extracted text, without any introductions or extra words."_q;
}

QString DescribeImagePrompt(const QString &language) {
	return u"Describe the content of the image clearly and concisely in %1. Focus on the main objects, people, or scene. Do not add any explanations or introductions. Output only the description."_q
		.arg(language);
}

} // namespace Api::Gemini
