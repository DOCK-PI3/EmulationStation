#include "guis/GuiMenu.h"

#include "components/OptionListComponent.h"
#include "components/SliderComponent.h"
#include "components/SwitchComponent.h"
#include "guis/GuiCollectionSystemsOptions.h"
#include "guis/GuiDetectDevice.h"
#include "guis/GuiGeneralScreensaverOptions.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiScraperStart.h"
#include "guis/GuiSettings.h"
#include "views/UIModeController.h"
#include "views/ViewController.h"
#include "CollectionSystemManager.h"
#include "EmulationStation.h"
#include "SystemData.h"
#include "VolumeControl.h"
#include <SDL_events.h>

GuiMenu::GuiMenu(Window* window) : GuiComponent(window), mMenu(window, _("MasOS MENU PRINCIPAL").c_str()), mVersion(window)
{
	bool isFullUI = UIModeController::getInstance()->isUIModeFull();

	if (isFullUI)
		addEntry(_("SCRAPEAR").c_str(), 0x777777FF, true, [this] { openScraperSettings(); });

	addEntry(_("OPCIONES DE SONIDO").c_str(), 0x777777FF, true, [this] { openSoundSettings(); });


	if (isFullUI)
		addEntry(_("OPCIONES INTERFAZ USUARIO").c_str(), 0x777777FF, true, [this] { openUISettings(); });

	if (isFullUI)
		addEntry(_("OPCIONES COLECCION DE JUEGOS").c_str(), 0x777777FF, true, [this] { openCollectionSystemSettings(); });

	if (isFullUI)
		addEntry(_("OTRAS OPCIONES").c_str(), 0x777777FF, true, [this] { openOtherSettings(); });

	if (isFullUI)
		addEntry(_("CONFIGURE SUS BOTONES").c_str(), 0x777777FF, true, [this] { openConfigInput(); });

	addEntry(_("SALIR").c_str(), 0x777777FF, true, [this] {openQuitMenu(); });

	addChild(&mMenu);
	addVersionInfo();
	setSize(mMenu.getSize());
	setPosition((Renderer::getScreenWidth() - mSize.x()) / 2, Renderer::getScreenHeight() * 0.15f);
}

void GuiMenu::openScraperSettings()
{
	auto s = new GuiSettings(mWindow, _("SCRAPEAR").c_str());

	// scrape from
	auto scraper_list = std::make_shared< OptionListComponent< std::string > >(mWindow, _("SCRAPEAR DESDE"), false);
	std::vector<std::string> scrapers = getScraperList();
	for(auto it = scrapers.cbegin(); it != scrapers.cend(); it++)
		scraper_list->add(*it, *it, *it == Settings::getInstance()->getString("Scraper"));

	s->addWithLabel(_("SCRAPEAR DESDE"), scraper_list);
	s->addSaveFunc([scraper_list] { Settings::getInstance()->setString("Scraper", scraper_list->getSelected()); });

	// scrape ratings
	auto scrape_ratings = std::make_shared<SwitchComponent>(mWindow);
	scrape_ratings->setState(Settings::getInstance()->getBool("ScrapeRatings"));
	s->addWithLabel(_("SCRAPE RATINGS"), scrape_ratings);
	s->addSaveFunc([scrape_ratings] { Settings::getInstance()->setBool("ScrapeRatings", scrape_ratings->getState()); });

	// scrape now
	ComponentListRow row;
	auto openScrapeNow = [this] { mWindow->pushGui(new GuiScraperStart(mWindow)); };
	std::function<void()> openAndSave = openScrapeNow;
	openAndSave = [s, openAndSave] { s->save(); openAndSave(); };
	row.makeAcceptInputHandler(openAndSave);

	auto scrape_now = std::make_shared<TextComponent>(mWindow, _("SCRAPEAR AHORA"), Font::get(FONT_SIZE_MEDIUM), 0x777777FF);
	auto bracket = makeArrow(mWindow);
	row.addElement(scrape_now, true);
	row.addElement(bracket, false);
	s->addRow(row);

	mWindow->pushGui(s);
}

void GuiMenu::openSoundSettings()
{
	auto s = new GuiSettings(mWindow, _("OPCIONES DE SONIDO").c_str());

	// volume
	auto volume = std::make_shared<SliderComponent>(mWindow, 0.f, 100.f, 1.f, "%");
	volume->setValue((float)VolumeControl::getInstance()->getVolume());
	s->addWithLabel(_("VOLUMEN DEL SISTEMA"), volume);
	s->addSaveFunc([volume] { VolumeControl::getInstance()->setVolume((int)Math::round(volume->getValue())); });

	if (UIModeController::getInstance()->isUIModeFull())
	{
#ifdef _RPI_
		// volume control device
		auto vol_dev = std::make_shared< OptionListComponent<std::string> >(mWindow, _("AUDIO DEVICE"), false);
		std::vector<std::string> transitions;
		transitions.push_back(N_("PCM"));
		transitions.push_back(N_("Speaker"));
		transitions.push_back(N_("Master"));
		for(auto it = transitions.cbegin(); it != transitions.cend(); it++)
			vol_dev->add(_(it->c_str()), *it, Settings::getInstance()->getString("AudioDevice") == *it);
		s->addWithLabel(_("AUDIO DEVICE"), vol_dev);
		s->addSaveFunc([vol_dev] {
			Settings::getInstance()->setString("AudioDevice", vol_dev->getSelected());
			VolumeControl::getInstance()->deinit();
			VolumeControl::getInstance()->init();
		});
#endif

		// disable sounds
		auto sounds_enabled = std::make_shared<SwitchComponent>(mWindow);
		sounds_enabled->setState(Settings::getInstance()->getBool("EnableSounds"));
		s->addWithLabel(_("ENABLE NAVIGATION SOUNDS"), sounds_enabled);
		s->addSaveFunc([sounds_enabled] {
			if (sounds_enabled->getState()
				&& !Settings::getInstance()->getBool("EnableSounds")
				&& PowerSaver::getMode() == PowerSaver::INSTANT)
			{
				Settings::getInstance()->setString("PowerSaverMode", "default");
				PowerSaver::init();
			}
			Settings::getInstance()->setBool("EnableSounds", sounds_enabled->getState());
		});

		auto video_audio = std::make_shared<SwitchComponent>(mWindow);
		video_audio->setState(Settings::getInstance()->getBool("VideoAudio"));
		s->addWithLabel(_("ENABLE VIDEO AUDIO"), video_audio);
		s->addSaveFunc([video_audio] { Settings::getInstance()->setBool("VideoAudio", video_audio->getState()); });

#ifdef _RPI_
		// OMX player Audio Device
		auto omx_audio_dev = std::make_shared< OptionListComponent<std::string> >(mWindow, _("OMX PLAYER AUDIO DEVICE"), false);
		std::vector<std::string> devices;
		devices.push_back(N_("local"));
		devices.push_back(N_("hdmi"));
		devices.push_back(N_("both"));
		// USB audio
		devices.push_back("alsa:hw:0,0");
		devices.push_back("alsa:hw:1,0");
		for (auto it = devices.cbegin(); it != devices.cend(); it++)
			omx_audio_dev->add(_(it->c_str()), *it, Settings::getInstance()->getString("OMXAudioDev") == *it);
		s->addWithLabel(_("OMX PLAYER AUDIO DEVICE"), omx_audio_dev);
		s->addSaveFunc([omx_audio_dev] {
			if (Settings::getInstance()->getString("OMXAudioDev") != omx_audio_dev->getSelected())
				Settings::getInstance()->setString("OMXAudioDev", omx_audio_dev->getSelected());
		});
#endif
	}

	mWindow->pushGui(s);

}

void GuiMenu::openUISettings()
{
	auto s = new GuiSettings(mWindow, _("OPCIONES INTERFAZ DE USUARIO").c_str());

	//UI mode
	auto UImodeSelection = std::make_shared< OptionListComponent<std::string> >(mWindow, _("INTERFAZ DE USUARIO MODO"), false);
	std::vector<std::string> UImodes = UIModeController::getInstance()->getUIModes();
	for (auto it = UImodes.cbegin(); it != UImodes.cend(); it++)
		UImodeSelection->add(_(it->c_str()), *it, Settings::getInstance()->getString("UIMode") == *it);
	s->addWithLabel(_("UI MODE"), UImodeSelection);
	Window* window = mWindow;
	s->addSaveFunc([ UImodeSelection, window]
	{
		std::string selectedMode = UImodeSelection->getSelected();
		if (selectedMode != "Full")
		{
			std::stringstream message;
			message << boost::locale::format(_("Está cambiando la interfaz de usuario a un modo restringido:\n{1}\n"	\
				"Esto ocultará la mayoría de las opciones de menú para evitar cambios en el sistema.\n"	\
				"Para desbloquear y regresar a la IU completa, ingrese este código: \n"		\
				"\"{2}\"\n\n"								\
				"Quieres proceder?"))
			  % _(selectedMode.c_str()) % UIModeController::getInstance()->getFormattedPassKeyStr();
			window->pushGui(new GuiMsgBox(window, message.str(), 
				_("YES"), [selectedMode] {
					LOG(LogDebug) << "Configurando el modo UI a " << selectedMode;
					Settings::getInstance()->setString("UIMode", selectedMode);
					Settings::getInstance()->saveFile();
			}, _("NO"),nullptr));
		}
	});

	// screensaver
	ComponentListRow screensaver_row;
	screensaver_row.elements.clear();
	screensaver_row.addElement(std::make_shared<TextComponent>(mWindow, _("AJUSTES PROTECCIÓN DE PANTALLA"), Font::get(FONT_SIZE_MEDIUM), 0x777777FF), true);
	screensaver_row.addElement(makeArrow(mWindow), false);
	screensaver_row.makeAcceptInputHandler(std::bind(&GuiMenu::openScreensaverOptions, this));
	s->addRow(screensaver_row);

	// quick system select (left/right in game list view)
	auto quick_sys_select = std::make_shared<SwitchComponent>(mWindow);
	quick_sys_select->setState(Settings::getInstance()->getBool("QuickSystemSelect"));
	s->addWithLabel(_("SELECCIÓN RÁPIDA DEL SISTEMA"), quick_sys_select);
	s->addSaveFunc([quick_sys_select] { Settings::getInstance()->setBool("QuickSystemSelect", quick_sys_select->getState()); });

	// carousel transition option
	auto move_carousel = std::make_shared<SwitchComponent>(mWindow);
	move_carousel->setState(Settings::getInstance()->getBool("MoveCarousel"));
	s->addWithLabel(_("TRANSICIONES DE CARRUSEL"), move_carousel);
	s->addSaveFunc([move_carousel] {
		if (move_carousel->getState()
			&& !Settings::getInstance()->getBool("MoveCarousel")
			&& PowerSaver::getMode() == PowerSaver::INSTANT)
		{
			Settings::getInstance()->setString("PowerSaverMode", "default");
			PowerSaver::init();
		}
		Settings::getInstance()->setBool("MoveCarousel", move_carousel->getState());
	});

	// transition style
	auto transition_style = std::make_shared< OptionListComponent<std::string> >(mWindow, _("ESTILO DE TRANSICIÓN"), false);
	std::vector<std::string> transitions;
	transitions.push_back(N_("fade"));
	transitions.push_back(N_("slide"));
	transitions.push_back(N_("instant"));
	for(auto it = transitions.cbegin(); it != transitions.cend(); it++)
	  transition_style->add(_(it->c_str()), *it, Settings::getInstance()->getString("TransitionStyle") == *it);
	s->addWithLabel(_("ESTILO DE TRANSICIÓN"), transition_style);
	s->addSaveFunc([transition_style] {
		if (Settings::getInstance()->getString("TransitionStyle") == "instant"
			&& transition_style->getSelected() != "instant"
			&& PowerSaver::getMode() == PowerSaver::INSTANT)
		{
			Settings::getInstance()->setString("PowerSaverMode", "default");
			PowerSaver::init();
		}
		Settings::getInstance()->setString("TransitionStyle", transition_style->getSelected());
	});

	// theme set
	auto themeSets = ThemeData::getThemeSets();

	if(!themeSets.empty())
	{
		std::map<std::string, ThemeSet>::const_iterator selectedSet = themeSets.find(Settings::getInstance()->getString("ThemeSet"));
		if(selectedSet == themeSets.cend())
			selectedSet = themeSets.cbegin();

		auto theme_set = std::make_shared< OptionListComponent<std::string> >(mWindow, _("CONJUNTO DE TEMA"), false);
		for(auto it = themeSets.cbegin(); it != themeSets.cend(); it++)
			theme_set->add(it->first, it->first, it == selectedSet);
		s->addWithLabel(_("CONJUNTO DE TEMA"), theme_set);

		Window* window = mWindow;
		s->addSaveFunc([window, theme_set]
		{
			bool needReload = false;
			if(Settings::getInstance()->getString("ThemeSet") != theme_set->getSelected())
				needReload = true;

			Settings::getInstance()->setString("ThemeSet", theme_set->getSelected());

			if(needReload)
			{
				CollectionSystemManager::get()->updateSystemsList();
				ViewController::get()->goToStart();
				ViewController::get()->reloadAll(); // TODO - replace this with some sort of signal-based implementation
			}
		});
	}

	// GameList view style
	auto gamelist_style = std::make_shared< OptionListComponent<std::string> >(mWindow, _("ESTILO LISTA DE JUEGOS"), false);
	std::vector<std::string> styles;
	styles.push_back(N_("automatic"));
	styles.push_back(N_("basic"));
	styles.push_back(N_("detailed"));
	styles.push_back(N_("video"));
	styles.push_back(N_("grid"));

	for (auto it = styles.cbegin(); it != styles.cend(); it++)
	  gamelist_style->add(_(it->c_str()), *it, Settings::getInstance()->getString("GamelistViewStyle") == *it);
	s->addWithLabel(_("ESTILO LISTA DE JUEGOS"), gamelist_style);
	s->addSaveFunc([gamelist_style] {
		bool needReload = false;
		if (Settings::getInstance()->getString("GamelistViewStyle") != gamelist_style->getSelected())
			needReload = true;
		Settings::getInstance()->setString("GamelistViewStyle", gamelist_style->getSelected());
		if (needReload)
			ViewController::get()->reloadAll();
	});

	// Optionally start in selected system
	auto systemfocus_list = std::make_shared< OptionListComponent<std::string> >(mWindow, _("INICIAR EN SISTEMA"), false);
	systemfocus_list->add(_("NONE"), "", Settings::getInstance()->getString("StartupSystem") == "");
	for (auto it = SystemData::sSystemVector.cbegin(); it != SystemData::sSystemVector.cend(); it++)
	{
		if ("retropie" != (*it)->getName())
		{
			std::string label(_((*it)->getName().c_str()));
			systemfocus_list->add(label, (*it)->getName(), Settings::getInstance()->getString("StartupSystem") == (*it)->getName());
		}
	}
	s->addWithLabel(_("INICIAR EN SISTEMA"), systemfocus_list);
	s->addSaveFunc([systemfocus_list] {
		Settings::getInstance()->setString("StartupSystem", systemfocus_list->getSelected());
	});

	// show help
	auto show_help = std::make_shared<SwitchComponent>(mWindow);
	show_help->setState(Settings::getInstance()->getBool("ShowHelpPrompts"));
	s->addWithLabel(_("AYUDA EN PANTALLA"), show_help);
	s->addSaveFunc([show_help] { Settings::getInstance()->setBool("ShowHelpPrompts", show_help->getState()); });

	// enable filters (ForceDisableFilters)
	auto enable_filter = std::make_shared<SwitchComponent>(mWindow);
	enable_filter->setState(!Settings::getInstance()->getBool("ForceDisableFilters"));
	s->addWithLabel(_("ACTIVAR FILTROS"), enable_filter);
	s->addSaveFunc([enable_filter] { 
		bool filter_is_enabled = !Settings::getInstance()->getBool("ForceDisableFilters");
		Settings::getInstance()->setBool("ForceDisableFilters", !enable_filter->getState()); 
		if (enable_filter->getState() != filter_is_enabled) ViewController::get()->ReloadAndGoToStart();
	});

	mWindow->pushGui(s);

}

void GuiMenu::openOtherSettings()
{
	auto s = new GuiSettings(mWindow, _("OTRAS OPCIONES").c_str());

	// maximum vram
	auto max_vram = std::make_shared<SliderComponent>(mWindow, 0.f, 1000.f, 10.f, _("Mb"));
	max_vram->setValue((float)(Settings::getInstance()->getInt("MaxVRAM")));
	s->addWithLabel(_("VRAM LIMITE"), max_vram);
	s->addSaveFunc([max_vram] { Settings::getInstance()->setInt("MaxVRAM", (int)Math::round(max_vram->getValue())); });

	// power saver
	auto power_saver = std::make_shared< OptionListComponent<std::string> >(mWindow, _("MODOS AHORRO DE ENERGÍA"), false);
	std::vector<std::string> modes;
	modes.push_back(N_("disabled"));
	modes.push_back(N_("default"));
	modes.push_back(N_("enhanced"));
	modes.push_back(N_("instant"));
	for (auto it = modes.cbegin(); it != modes.cend(); it++)
	  power_saver->add(_(it->c_str()), *it, Settings::getInstance()->getString("PowerSaverMode") == *it);
	s->addWithLabel(_("POWER SAVER MODES"), power_saver);
	s->addSaveFunc([this, power_saver] {
		if (Settings::getInstance()->getString("PowerSaverMode") != "instant" && power_saver->getSelected() == "instant") {
			Settings::getInstance()->setString("TransitionStyle", "instant");
			Settings::getInstance()->setBool("MoveCarousel", false);
			Settings::getInstance()->setBool("EnableSounds", false);
		}
		Settings::getInstance()->setString("PowerSaverMode", power_saver->getSelected());
		PowerSaver::init();
	});

	// gamelists
	auto save_gamelists = std::make_shared<SwitchComponent>(mWindow);
	save_gamelists->setState(Settings::getInstance()->getBool("SaveGamelistsOnExit"));
	s->addWithLabel(_("GUARDAR METADATOS AL SALIR"), save_gamelists);
	s->addSaveFunc([save_gamelists] { Settings::getInstance()->setBool("SaveGamelistsOnExit", save_gamelists->getState()); });

	auto parse_gamelists = std::make_shared<SwitchComponent>(mWindow);
	parse_gamelists->setState(Settings::getInstance()->getBool("ParseGamelistOnly"));
	s->addWithLabel(_("ANALIZAR SOLO LISTA DE JUEGOS"), parse_gamelists);
	s->addSaveFunc([parse_gamelists] { Settings::getInstance()->setBool("ParseGamelistOnly", parse_gamelists->getState()); });

#ifndef WIN32
	// hidden files
	auto hidden_files = std::make_shared<SwitchComponent>(mWindow);
	hidden_files->setState(Settings::getInstance()->getBool("ShowHiddenFiles"));
	s->addWithLabel(_("MOSTRAR FICHEROS OCULTOS"), hidden_files);
	s->addSaveFunc([hidden_files] { Settings::getInstance()->setBool("ShowHiddenFiles", hidden_files->getState()); });
#endif

#ifdef _RPI_
	// Video Player - VideoOmxPlayer
	auto omx_player = std::make_shared<SwitchComponent>(mWindow);
	omx_player->setState(Settings::getInstance()->getBool("VideoOmxPlayer"));
	s->addWithLabel(_("UTILICE EL REPRODUCTOR OMX (HW ACCELERATED)"), omx_player);
	s->addSaveFunc([omx_player]
	{
		// need to reload all views to re-create the right video components
		bool needReload = false;
		if(Settings::getInstance()->getBool("VideoOmxPlayer") != omx_player->getState())
			needReload = true;

		Settings::getInstance()->setBool("VideoOmxPlayer", omx_player->getState());

		if(needReload)
			ViewController::get()->reloadAll();
	});

#endif

	// framerate
	auto framerate = std::make_shared<SwitchComponent>(mWindow);
	framerate->setState(Settings::getInstance()->getBool("DrawFramerate"));
	s->addWithLabel(_("MOSTRAR FRAMERATE"), framerate);
	s->addSaveFunc([framerate] { Settings::getInstance()->setBool("DrawFramerate", framerate->getState()); });


	mWindow->pushGui(s);

}

void GuiMenu::openConfigInput()
{
	Window* window = mWindow;
	window->pushGui(new GuiMsgBox(window, _("¿ESTÁS SEGURO DE QUE QUIERES CONFIGURAR LA ENTRADA?"), _("YES"),
		[window] {
		window->pushGui(new GuiDetectDevice(window, false, nullptr));
	}, _("NO"), nullptr)
	);

}

void GuiMenu::openQuitMenu()
{
	auto s = new GuiSettings(mWindow, _("SALIR").c_str());

	Window* window = mWindow;

	ComponentListRow row;
	if (UIModeController::getInstance()->isUIModeFull())
	{
		row.makeAcceptInputHandler([window] {
			window->pushGui(new GuiMsgBox(window, _("REALMENTE QUIERES REINICIAR?"), _("SI"),
				[] {
				if(quitES("/tmp/es-restart") != 0)
					LOG(LogWarning) << "Restart terminated with non-zero result!";
			}, _("NO"), nullptr));
		});
		row.addElement(std::make_shared<TextComponent>(window, _("REINICIAR EMULATIONSTATION"), Font::get(FONT_SIZE_MEDIUM), 0x777777FF), true);
		s->addRow(row);



		if(Settings::getInstance()->getBool("ShowExit"))
		{
			row.elements.clear();
			row.makeAcceptInputHandler([window] {
				window->pushGui(new GuiMsgBox(window, _("SEGURO QUE QUIERE SALIR?"), _("SI"),
					[] {
					SDL_Event ev;
					ev.type = SDL_QUIT;
					SDL_PushEvent(&ev);
				}, _("NO"), nullptr));
			});
			row.addElement(std::make_shared<TextComponent>(window, _("SALIR DE EMULATIONSTATION"), Font::get(FONT_SIZE_MEDIUM), 0x777777FF), true);
			s->addRow(row);
		}
	}
	row.elements.clear();
	row.makeAcceptInputHandler([window] {
		window->pushGui(new GuiMsgBox(window, _("REALMENTE QUIERE REINICIAR?"), _("SI"),
			[] {
			if (quitES("/tmp/es-sysrestart") != 0)
				LOG(LogWarning) << "Restart terminated with non-zero result!";
		}, _("NO"), nullptr));
	});
	row.addElement(std::make_shared<TextComponent>(window, _("REINICIAR SISTEMA"), Font::get(FONT_SIZE_MEDIUM), 0x777777FF), true);
	s->addRow(row);

	row.elements.clear();
	row.makeAcceptInputHandler([window] {
		window->pushGui(new GuiMsgBox(window, _("¿REALMENTE QUIERE APAGAR?"), _("SI"),
			[] {
			if (quitES("/tmp/es-shutdown") != 0)
				LOG(LogWarning) << "Shutdown terminated with non-zero result!";
		}, _("NO"), nullptr));
	});
	row.addElement(std::make_shared<TextComponent>(window, _("APAGAR SISTEMA"), Font::get(FONT_SIZE_MEDIUM), 0x777777FF), true);
	s->addRow(row);

	mWindow->pushGui(s);
}

void GuiMenu::addVersionInfo()
{
	std::string  buildDate = (Settings::getInstance()->getBool("Debug") ? std::string( "   (" + Utils::String::toUpper(PROGRAM_BUILT_STRING) + ")") : (""));
	mVersion.setFont(Font::get(FONT_SIZE_SMALL));
	mVersion.setColor(0x5E5E5EFF);
	mVersion.setText("EMULATIONSTATION V" + Utils::String::toUpper(PROGRAM_VERSION_STRING) + buildDate);
	mVersion.setHorizontalAlignment(ALIGN_CENTER);
	addChild(&mVersion);
}

void GuiMenu::openScreensaverOptions() {
	mWindow->pushGui(new GuiGeneralScreensaverOptions(mWindow, _("AJUSTES PROTECCIÓN DE PANTALLA").c_str()));
}

void GuiMenu::openCollectionSystemSettings() {
	mWindow->pushGui(new GuiCollectionSystemsOptions(mWindow));
}

void GuiMenu::onSizeChanged()
{
	mVersion.setSize(mSize.x(), 0);
	mVersion.setPosition(0, mSize.y() - mVersion.getSize().y());
}

void GuiMenu::addEntry(const char* name, unsigned int color, bool add_arrow, const std::function<void()>& func)
{
	std::shared_ptr<Font> font = Font::get(FONT_SIZE_MEDIUM);

	// populate the list
	ComponentListRow row;
	row.addElement(std::make_shared<TextComponent>(mWindow, name, font, color), true);

	if(add_arrow)
	{
		std::shared_ptr<ImageComponent> bracket = makeArrow(mWindow);
		row.addElement(bracket, false);
	}

	row.makeAcceptInputHandler(func);

	mMenu.addRow(row);
}

bool GuiMenu::input(InputConfig* config, Input input)
{
	if(GuiComponent::input(config, input))
		return true;

	if((config->isMappedTo("b", input) || config->isMappedTo("start", input)) && input.value != 0)
	{
		delete this;
		return true;
	}

	return false;
}

HelpStyle GuiMenu::getHelpStyle()
{
	HelpStyle style = HelpStyle();
	style.applyTheme(ViewController::get()->getState().getSystem()->getTheme(), "system");
	return style;
}

std::vector<HelpPrompt> GuiMenu::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	prompts.push_back(HelpPrompt("Arriba/Abajo", _("CHOOSE")));
	prompts.push_back(HelpPrompt("A", _("SELECIONAR")));
	prompts.push_back(HelpPrompt("START", _("CERRAR")));
	return prompts;
}
