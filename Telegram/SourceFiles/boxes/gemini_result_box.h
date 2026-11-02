/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/layers/generic_box.h"
#include "api/api_gemini.h"

namespace Ui {

enum class GeminiActionType {
	Summarize,
	Translate,
	Ocr,
	DescribeImage,
	TranscribeAudio,
	CustomQuery,
};

struct GeminiResultBoxArgs {
	GeminiActionType type = GeminiActionType::CustomQuery;
	QString title;
	QString inputText;
	QByteArray imageBytes;
	QString imageMimeType;
	QString targetLanguage;
	Fn<void(QString)> applyToDraft;
};

void GeminiResultBox(not_null<GenericBox*> box, GeminiResultBoxArgs &&args);

} // namespace Ui
