/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "boxes/gemini_settings_box.h"

#include "api/api_gemini.h"
#include "lang/lang_keys.h"
#include "ui/toast/toast.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/popup_menu.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"

namespace Ui {

void GeminiSettingsBox(not_null<GenericBox*> box) {
	box->setWidth(st::boxWideWidth);
	box->setTitle(rpl::single(u"Gemini AI Settings"_q));

	const auto content = box->verticalLayout();

	// Description label with link to Google AI Studio
	const auto description = box->addRow(
		object_ptr<FlatLabel>(
			box,
			u"Configure Google Gemini AI for smart replies, message summarization, translation, and image analysis.<br><br>You can get a free API key at <a href=\"https://aistudio.google.com/apikey\">Google AI Studio</a>."_q,
			st::aboutLabel),
		st::boxRowPadding);
	description->setOpenExternalLinks(true);

	AddSkip(content);

	// Enable Checkbox
	const auto enabledCheckbox = box->addRow(
		object_ptr<Checkbox>(
			box,
			u"Enable Gemini AI features"_q,
			Api::Gemini::Enabled(),
			st::defaultBoxCheckbox),
		st::boxRowPadding);

	AddSkip(content);

	// API Key Header & Input
	box->addRow(
		object_ptr<FlatLabel>(box, u"Gemini API Key:"_q, st::boxTitle),
		st::boxRowPadding);

	const auto apiKeyField = box->addRow(
		object_ptr<InputField>(
			box,
			st::defaultInputField,
			rpl::single(u"AIzaSy..."_q),
			Api::Gemini::ApiKey()),
		st::boxRowPadding);

	AddSkip(content);

	// Model Header & Input
	box->addRow(
		object_ptr<FlatLabel>(box, u"Model Name:"_q, st::boxTitle),
		st::boxRowPadding);

	const auto modelField = box->addRow(
		object_ptr<InputField>(
			box,
			st::defaultInputField,
			rpl::single(u"gemini-1.5-flash"_q),
			Api::Gemini::Model()),
		st::boxRowPadding);

	// Fetch models button
	const auto fetchModelsBtn = box->addRow(
		object_ptr<LinkButton>(
			box,
			u"Choose from available models online"_q,
			st::boxLinkButton),
		st::boxRowPadding);

	struct MenuHolder {
		base::unique_qptr<PopupMenu> menu;
	};
	const auto menuHolder = box->lifetime().make_state<MenuHolder>();

	fetchModelsBtn->setClickedCallback([=] {
		const auto key = apiKeyField->getLastText().trimmed();
		if (key.isEmpty()) {
			Toast::Show(box, u"Please enter an API Key first."_q);
			return;
		}

		Api::Gemini::SetApiKey(key);
		Toast::Show(box, u"Fetching model list..."_q);

		Api::Gemini::FetchModels([=](std::vector<Api::Gemini::ModelInfo> models, QString error) {
			if (!error.isEmpty()) {
				Toast::Show(box, u"Failed to load models: %1"_q.arg(error));
				return;
			}
			if (models.empty()) {
				Toast::Show(box, u"No supported models found."_q);
				return;
			}

			menuHolder->menu = base::make_unique_q<PopupMenu>(
				fetchModelsBtn,
				st::popupMenuWithIcons);

			for (const auto &model : models) {
				const auto id = model.id;
				const auto title = model.displayName.isEmpty()
					? model.id
					: model.displayName;
				menuHolder->menu->addAction(title, [=] {
					modelField->setText(id);
				});
			}

			menuHolder->menu->popup(
				fetchModelsBtn->mapToGlobal(QPoint(0, fetchModelsBtn->height())));
		});
	});

	AddSkip(content);

	// System Prompt Header & Input
	box->addRow(
		object_ptr<FlatLabel>(
			box,
			u"System Prompt (Custom Instructions):"_q,
			st::boxTitle),
		st::boxRowPadding);

	const auto systemPromptField = box->addRow(
		object_ptr<InputField>(
			box,
			st::defaultInputField,
			InputField::Mode::MultiLine,
			rpl::single(u"E.g.: You are a helpful assistant. Keep replies concise and polite."_q),
			Api::Gemini::SystemPrompt()),
		st::boxRowPadding);

	AddSkip(content);

	// Temperature / Creativity Header & Field
	box->addRow(
		object_ptr<FlatLabel>(
			box,
			u"Temperature (Creativity: 0.0 - 1.0, Default 0.5):"_q,
			st::boxTitle),
		st::boxRowPadding);

	const auto tempField = box->addRow(
		object_ptr<InputField>(
			box,
			st::defaultInputField,
			rpl::single(u"0.5"_q),
			QString::number(Api::Gemini::Temperature(), 'f', 1)),
		st::boxRowPadding);

	AddSkip(content);

	// Save button
	box->addButton(tr::lng_settings_save(), [=] {
		const auto key = apiKeyField->getLastText().trimmed();
		const auto model = modelField->getLastText().trimmed();
		const auto prompt = systemPromptField->getLastText();
		const auto tempVal = tempField->getLastText().toDouble();
		const auto enabled = enabledCheckbox->checked();

		Api::Gemini::SetApiKey(key);
		if (!model.isEmpty()) {
			Api::Gemini::SetModel(model);
		}
		Api::Gemini::SetSystemPrompt(prompt);
		Api::Gemini::SetTemperature(tempVal);
		Api::Gemini::SetEnabled(enabled);

		Toast::Show(u"Gemini AI settings saved."_q);
		box->closeBox();
	});

	box->addButton(tr::lng_cancel(), [=] {
		box->closeBox();
	});
}

} // namespace Ui
