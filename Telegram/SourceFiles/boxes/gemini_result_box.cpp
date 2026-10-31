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
				st::boxTitle),
			st::boxRowPadding);
		AddSkip(content);
	}

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
		requestOptions.prompt = Api::Gemini::SummarizePrompt(args.inputText);
		break;
	case GeminiActionType::Translate: {
		const auto targetLang = args.targetLanguage.isEmpty()
			? Api::Gemini::TargetLanguage()
			: args.targetLanguage;
		requestOptions.prompt = Api::Gemini::TranslatePrompt(args.inputText, targetLang);
		break;
	}
	case GeminiActionType::Ocr:
		requestOptions.prompt = Api::Gemini::OcrPrompt();
		requestOptions.imageBytes = args.imageBytes;
		requestOptions.imageMimeType = args.imageMimeType;
		break;
	case GeminiActionType::DescribeImage: {
		const auto lang = args.targetLanguage.isEmpty()
			? Api::Gemini::TargetLanguage()
			: args.targetLanguage;
		requestOptions.prompt = Api::Gemini::DescribeImagePrompt(lang);
		requestOptions.imageBytes = args.imageBytes;
		requestOptions.imageMimeType = args.imageMimeType;
		break;
	}
	case GeminiActionType::CustomQuery:
	default:
		requestOptions.prompt = args.inputText;
		break;
	}

	// Trigger async call
	Api::Gemini::GenerateContent(std::move(requestOptions), crl::guard(box, [=](Api::Gemini::Response res) {
		loadingWrap->hide(anim::type::instant);

		if (!res.success) {
			state->currentResult.clear();
			resultLabelWrap->entity()->setText(
				u"Gemini Error: %1"_q.arg(res.error));
			resultLabelWrap->show(anim::type::instant);
			return;
		}

		state->currentResult = res.text;
		resultLabelWrap->entity()->setText(res.text);
		resultLabelWrap->show(anim::type::instant);
	}));
}

} // namespace Ui
