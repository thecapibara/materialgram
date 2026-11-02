/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "boxes/gemini_result_box.h"

#include "api/api_gemini.h"
#include "lang/lang_keys.h"
#include "ui/effects/loading_element.h"
#include "ui/text/text_utilities.h"
#include "ui/toast/toast.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/slide_wrap.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"

namespace Ui {
namespace {

[[nodiscard]] QString ResolveTitle(GeminiActionType type, const QString &custom) {
	if (!custom.isEmpty()) {
		return custom;
	}
	switch (type) {
	case GeminiActionType::Summarize:
		return u"Gemini AI: Summary"_q;
	case GeminiActionType::Translate:
		return u"Gemini AI: Translation"_q;
	case GeminiActionType::Ocr:
		return u"Gemini AI: Extracted Text (OCR)"_q;
	case GeminiActionType::DescribeImage:
		return u"Gemini AI: Image Description"_q;
	case GeminiActionType::TranscribeAudio:
		return u"Gemini AI: Audio Transcription"_q;
	case GeminiActionType::ImproveText:
		return u"Gemini AI: Improved Text"_q;
	case GeminiActionType::CustomQuery:
	default:
		return u"Gemini AI"_q;
	}
}

} // namespace

void GeminiResultBox(not_null<GenericBox*> box, GeminiResultBoxArgs &&args) {
	box->setWidth(st::boxWideWidth);
	box->setTitle(rpl::single(ResolveTitle(args.type, args.title)));

	const auto content = box->verticalLayout();

	// If there's an original text snippet, show a subtle header
	if (!args.inputText.isEmpty() && args.type != GeminiActionType::CustomQuery) {
		const auto snippet = args.inputText.length() > 140
			? (args.inputText.left(140) + u"..."_q)
			: args.inputText;
		box->addRow(
			object_ptr<FlatLabel>(
				box,
				u"Original: %1"_q.arg(snippet),
				st::aboutLabel),
			st::boxRowPadding);
		AddSkip(content);
	}

	// Loading status label
	const auto statusText = (args.type == GeminiActionType::Translate)
		? u"Translating with Gemini AI, please wait..."_q
		: (args.type == GeminiActionType::Summarize)
		? u"Summarizing with Gemini AI, please wait..."_q
		: (args.type == GeminiActionType::Ocr)
		? u"Extracting text with Gemini AI, please wait..."_q
		: (args.type == GeminiActionType::DescribeImage)
		? u"Analyzing image with Gemini AI, please wait..."_q
		: (args.type == GeminiActionType::TranscribeAudio)
		? u"Transcribing audio with Gemini AI, please wait..."_q
		: (args.type == GeminiActionType::ImproveText)
		? u"Improving text with Gemini AI, please wait..."_q
		: u"Generating with Gemini AI, please wait..."_q;

	const auto statusLabel = box->addRow(
		object_ptr<FlatLabel>(
			box,
			statusText,
			st::boxLabel),
		st::boxRowPadding);

	// Loading animation wrap
	const auto loadingWrap = box->addRow(
		object_ptr<SlideWrap<RpWidget>>(
			box,
			CreateLoadingTextWidget(
				box,
				st::aboutLabel.style,
				3,
				rpl::single(false))),
		st::boxRowPadding);

	// Result text label
	const auto resultLabelWrap = box->addRow(
		object_ptr<SlideWrap<FlatLabel>>(
			box,
			object_ptr<FlatLabel>(
				box,
				QString(),
				st::aboutLabel)),
		st::boxRowPadding);
	resultLabelWrap->entity()->setSelectable(true);
	resultLabelWrap->hide(anim::type::instant);

	struct State {
		QString currentResult;
	};
	const auto state = box->lifetime().make_state<State>();

	// Bottom action buttons
	const auto applyDraft = args.applyToDraft;
	if (applyDraft) {
		box->addButton(rpl::single(u"Insert into draft"_q), [=] {
			if (!state->currentResult.isEmpty()) {
				applyDraft(state->currentResult);
				box->closeBox();
			}
		});
	}

	box->addButton(tr::lng_context_copy_text(), [=] {
		if (state->currentResult.isEmpty()) {
			return;
		}
		TextUtilities::SetClipboardText(
			TextForMimeData::Simple(state->currentResult));
		Toast::Show(box, tr::lng_text_copied(tr::now));
	});

	box->addButton(tr::lng_box_ok(), [=] {
		box->closeBox();
	});

	// Build request
	Api::Gemini::RequestOptions requestOptions;
	switch (args.type) {
	case GeminiActionType::Summarize:
		requestOptions.systemInstruction = u"You are a concise summarizer. Summarize the following text clearly and concisely in the same language as the text. Output ONLY the summary without introductory or concluding remarks."_q;
		requestOptions.prompt = args.inputText;
		break;
	case GeminiActionType::Translate: {
		const auto targetLang = args.targetLanguage.isEmpty()
			? Api::Gemini::TargetLanguage()
			: args.targetLanguage;
		requestOptions.systemInstruction = u"You are an expert translator. Translate the following text into %1. Preserve the meaning, formatting, and tone. Output ONLY the raw translated text, without explanations or markdown formatting like quotes or backticks."_q.arg(targetLang);
		requestOptions.prompt = args.inputText;
		break;
	}
	case GeminiActionType::Ocr:
		requestOptions.systemInstruction = u"You are an expert OCR tool. Extract all visible text from the image accurately. Output ONLY the extracted text with no introductory text or commentary."_q;
		requestOptions.prompt = u"Extract all visible text from this image."_q;
		requestOptions.imageBytes = args.imageBytes;
		requestOptions.imageMimeType = args.imageMimeType;
		break;
	case GeminiActionType::DescribeImage: {
		const auto lang = args.targetLanguage.isEmpty()
			? Api::Gemini::TargetLanguage()
			: args.targetLanguage;
		requestOptions.systemInstruction = u"Describe the provided image clearly and in detail in %1."_q.arg(lang);
		requestOptions.prompt = u"Describe this image."_q;
		requestOptions.imageBytes = args.imageBytes;
		requestOptions.imageMimeType = args.imageMimeType;
		break;
	}
	case GeminiActionType::TranscribeAudio:
		requestOptions.systemInstruction = u"You are an expert speech-to-text transcriber. Transcribe the spoken audio verbatim in its original language. Output ONLY the transcription text without commentary, timestamps, or quotation marks."_q;
		requestOptions.prompt = u"Transcribe this audio verbatim in its original language."_q;
		requestOptions.imageBytes = args.imageBytes;
		requestOptions.imageMimeType = args.imageMimeType;
		break;
	case GeminiActionType::ImproveText:
		requestOptions.systemInstruction = u"You are an expert editor. Improve the following text by fixing spelling, grammar, punctuation, and awkward phrasing in its original language. Preserve the original language, tone, and intended meaning. Output ONLY the improved text without quotation marks, introductions, or explanations."_q;
		requestOptions.prompt = args.inputText;
		break;
	case GeminiActionType::CustomQuery:
	default:
		requestOptions.useCustomSystemPrompt = true;
		requestOptions.prompt = args.inputText;
		break;
	}

	// Trigger async call
	requestOptions.streamCallback = crl::guard(box, [=](const QString &text) {
		statusLabel->hide();
		loadingWrap->hide(anim::type::instant);
		state->currentResult = text;
		resultLabelWrap->entity()->setText(text);
		resultLabelWrap->show(anim::type::instant);
	});

	Api::Gemini::GenerateContent(std::move(requestOptions), crl::guard(box, [=](Api::Gemini::Response res) {
		statusLabel->hide();
		loadingWrap->hide(anim::type::instant);

		if (!res.success) {
			if (state->currentResult.isEmpty()) {
				resultLabelWrap->entity()->setText(
					u"Gemini Error: %1"_q.arg(res.error));
				resultLabelWrap->show(anim::type::instant);
			}
			return;
		}

		state->currentResult = res.text;
		resultLabelWrap->entity()->setText(res.text);
		resultLabelWrap->show(anim::type::instant);
	}));
}

} // namespace Ui
