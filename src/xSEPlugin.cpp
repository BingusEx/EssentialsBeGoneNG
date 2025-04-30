
#include "SimpleIni.h"
namespace {

	//vars
	enum NPC_STATE : bool {
		kProtected = false,
		kVunerable = true
	};

	NPC_STATE generalNPCState;
	NPC_STATE sideQuestNPCState;

	bool enableMessageBoxVIP = true;
	bool enableMessageBoxSideQuest = true;

	bool enableCameraShake = true;

	std::string messageVIP;
	std::string messageSideQuest;

	std::string notificationVIP;
	std::string notificationSideQuest;

	std::unordered_map<RE::FormID, RE::QUEST_DATA::Type> questNPCs;
	std::vector<std::string> npcExclusions;

	//
	void DisableEssentialStatus(RE::Actor* a_actor, RE::TESNPC* a_npc) {
		if (!a_npc) {
			return;
		}

		bool essential = a_actor->IsEssential();

		if (essential || a_actor->IsProtected()) {
			if (generalNPCState == NPC_STATE::kProtected) {
				a_actor->GetActorRuntimeData().boolFlags.reset(RE::Actor::BOOL_FLAGS::kEssential);
				a_actor->GetActorRuntimeData().boolFlags.set(RE::Actor::BOOL_FLAGS::kProtected);
			}
			else {
				a_actor->GetActorRuntimeData().boolFlags.reset(RE::Actor::BOOL_FLAGS::kEssential);
				a_actor->GetActorRuntimeData().boolFlags.reset(RE::Actor::BOOL_FLAGS::kProtected);
			}
		}

		bool baseEssential = a_npc->IsEssential();

		if (baseEssential || a_npc->IsProtected()) {
			if (generalNPCState == NPC_STATE::kProtected) {
				a_npc->actorData.actorBaseFlags.set(RE::ACTOR_BASE_DATA::Flag::kProtected);
				a_npc->actorData.actorBaseFlags.reset(RE::ACTOR_BASE_DATA::Flag::kEssential);
			}
			else {
				a_npc->actorData.actorBaseFlags.reset(RE::ACTOR_BASE_DATA::Flag::kProtected);
				a_npc->actorData.actorBaseFlags.reset(RE::ACTOR_BASE_DATA::Flag::kEssential);
			}
		}

		auto xAliases = a_actor->extraList.GetByType<RE::ExtraAliasInstanceArray>();
		if (xAliases) {
			RE::BSReadLockGuard locker(xAliases->lock);
			for (auto& aliasData : xAliases->aliases) {
				if (aliasData) {
					auto quest = aliasData->quest;
					auto alias = const_cast<RE::BGSBaseAlias*>(aliasData->alias);
					if (quest && alias && quest->GetType() != RE::QUEST_DATA::Type::kNone && (alias->IsEssential() || essential || baseEssential)) {
						alias->SetEssential(false);
						alias->SetProtected(true);
						if (generalNPCState == NPC_STATE::kVunerable && sideQuestNPCState == NPC_STATE::kVunerable) {
							switch (quest->GetType()) {
								case RE::QUEST_DATA::Type::kMiscellaneous:
								case RE::QUEST_DATA::Type::kSideQuest:
									alias->SetProtected(false);
									break;
								default:
									break;
							}
						}

						questNPCs.insert({ a_actor->GetFormID(), quest->GetType() });
					}
				}
			}
		}
	}


	std::string ReplaceName(std::string& a_subject, const std::string& a_search, const std::string& a_replace) {
		size_t pos = 0;
		while ((pos = a_subject.find(a_search, pos)) != std::string::npos) {
			a_subject.replace(pos, a_search.length(), a_replace);
			pos += a_replace.length();
		}
		return a_subject;
	}


	void PlayTESSound(RE::FormID a_formID) {
		auto audioManager = RE::BSAudioManager::GetSingleton();
		if (audioManager) {
			audioManager->Play(a_formID);
		}
	}

	// not very reliable
	class TESObjectLoadedEventHandler : public RE::BSTEventSink<RE::TESObjectLoadedEvent> {
		public:
		static TESObjectLoadedEventHandler* GetSingleton() {
			static TESObjectLoadedEventHandler singleton;
			return &singleton;
		}

		auto ProcessEvent(const RE::TESObjectLoadedEvent* evn, RE::BSTEventSource<RE::TESObjectLoadedEvent>* a_eventSource) -> RE::BSEventNotifyControl override {
			if (!evn) {
				return RE::BSEventNotifyControl::kContinue;
			}

			auto actor = RE::TESForm::LookupByID<RE::Actor>(evn->formID);
			if (!actor || actor->IsPlayerRef() || actor->IsPlayerTeammate()) {
				return RE::BSEventNotifyControl::kContinue;
			}

			if (std::ranges::find(npcExclusions, actor->GetName()) != npcExclusions.end()) {
				return RE::BSEventNotifyControl::kContinue;
			}

			DisableEssentialStatus(actor, actor->GetActorBase());

			return RE::BSEventNotifyControl::kContinue;
		}

		protected:
		TESObjectLoadedEventHandler() = default;
		TESObjectLoadedEventHandler(const TESObjectLoadedEventHandler&) = delete;
		TESObjectLoadedEventHandler(TESObjectLoadedEventHandler&&) = delete;
		virtual ~TESObjectLoadedEventHandler() = default;

		auto operator=(const TESObjectLoadedEventHandler&) -> TESObjectLoadedEventHandler & = delete;
		auto operator=(TESObjectLoadedEventHandler&&)->TESObjectLoadedEventHandler & = delete;
	};

	std::vector<std::string> split(const std::string& input, const std::string& delimiter) {
		std::vector<std::string> tokens;
		size_t start = 0, end;

		while ((end = input.find(delimiter, start)) != std::string::npos) {
			std::string token = input.substr(start, end - start);
			if (!token.empty()) {
				tokens.push_back(token);
			}
			start = end + delimiter.length();
		}

		std::string lastToken = input.substr(start);
		if (!lastToken.empty()) {
			tokens.push_back(lastToken);
		}

		return tokens;
	}

	//
	class TESDeathEventHandler : public RE::BSTEventSink<RE::TESDeathEvent> {
		public:
		static TESDeathEventHandler* GetSingleton() {
			static TESDeathEventHandler singleton;
			return &singleton;
		}

		auto ProcessEvent(const RE::TESDeathEvent* evn, RE::BSTEventSource<RE::TESDeathEvent>* a_eventSource) -> RE::BSEventNotifyControl override {
			if (!evn) {
				return RE::BSEventNotifyControl::kContinue;
			}

			auto actor = evn->actorDying.get();
			if (!actor) {
				return RE::BSEventNotifyControl::kContinue;
			}

			auto result = questNPCs.find(actor->GetFormID());
			if (result != questNPCs.end()) {
				if (!evn->dead) {
					PlayTESSound(greyBeardRumble);	//greybeard rumble
					if (enableCameraShake) {
						switch (result->second) {
							case RE::QUEST_DATA::Type::kMiscellaneous:
							case RE::QUEST_DATA::Type::kSideQuest:
								RE::ShakeCamera(0.125f, actor->GetPosition(), 2.0);
								break;
							default:
								RE::ShakeCamera(0.25f, actor->GetPosition(), 2.0);
								break;
						}
					}
				}
				else {
					std::string name{ actor->GetName() };

					switch (result->second) {
						case RE::QUEST_DATA::Type::kMiscellaneous:
						case RE::QUEST_DATA::Type::kSideQuest:
							if (enableMessageBoxSideQuest) {
								auto str = ReplaceName(messageSideQuest, "[npc]", name);
								RE::DebugMessageBox(str.c_str());
							}
							else {
								auto str = ReplaceName(notificationSideQuest, "[npc]", name);
								RE::DebugNotification(str.c_str());
							}
							break;
						default:
							if (enableMessageBoxVIP) {
								auto str = ReplaceName(messageVIP, "[npc]", name);
								logger::info("{}", str);
								RE::DebugMessageBox(str.c_str());
							}
							else {
								auto str = ReplaceName(notificationVIP, "[npc]", name);
								RE::DebugNotification(str.c_str());
							}
							break;
					}
				}
			}

			return RE::BSEventNotifyControl::kContinue;
		}

		protected:
		static inline RE::FormID greyBeardRumble{ 0x00000E05 };

		TESDeathEventHandler() = default;
		TESDeathEventHandler(const TESDeathEventHandler&) = delete;
		TESDeathEventHandler(TESDeathEventHandler&&) = delete;
		virtual ~TESDeathEventHandler() = default;

		auto operator=(const TESDeathEventHandler&) -> TESDeathEventHandler & = delete;
		auto operator=(TESDeathEventHandler&&)->TESDeathEventHandler & = delete;
	};

	// ini
	void LoadSettings() {
		constexpr auto path = L"Data/SKSE/Plugins/EssentialsBeGone.ini";

		CSimpleIniA ini;
		ini.SetUnicode();
		ini.SetMultiKey();

		ini.LoadFile(path);

		generalNPCState = static_cast<NPC_STATE>(ini.GetBoolValue("Settings", "General NPC state", false));
		ini.SetBoolValue("Settings", "General NPC state", std::to_underlying(generalNPCState), ";Generic NPC state.\n;0 - Protected, 1 - Vunerable", true);

		sideQuestNPCState = static_cast<NPC_STATE>(ini.GetBoolValue("Settings", "Sidequest NPC state", false));
		ini.SetBoolValue("Settings", "Sidequest NPC state", std::to_underlying(sideQuestNPCState), ";Sidequest NPC state (important quest NPCs will always be protected). \n;0 - Protected, 1 - Vunerable", true);

		enableMessageBoxVIP = ini.GetBoolValue("Settings", "Enable messagebox for important quest NPCs", true);
		ini.SetBoolValue("Settings", "Enable messagebox for important quest NPCs", enableMessageBoxVIP, ";Enables a Morrowind style message box that pops up when important quest NPCs are killed. If false, a notification will show up instead.", true);

		enableMessageBoxSideQuest = ini.GetBoolValue("Settings", "Enable messagebox for sidequest NPCs", true);
		ini.SetBoolValue("Settings", "Enable messagebox for sidequest NPCs", enableMessageBoxSideQuest, ";Enables a Morrowind style message box that pops up when important quest NPCs are killed. If false, a notification will show up instead.", true);

		enableCameraShake = ini.GetBoolValue("Settings", "Enable camera shake", true);
		ini.SetBoolValue("Settings", "Enable camera shake", enableCameraShake, ";Camera shakes when important quest NPCs are killed", true);

		std::string exclusions = ini.GetValue("Settings", "Exclusions", "Dexion Evicus , General Tullius , Ulfric Stormcloak");
		ini.SetValue("Settings", "Exclusions", exclusions.c_str(), ";NPCs that should be excluded by this plugin.", true);
		npcExclusions = split(exclusions, " , ");

		messageVIP = ini.GetValue("Message Settings", "MessageBox (important NPCs)", "With [npc]'s death, the thread of prophecy is severed. Restore a saved game to restore the weave of fate, or persist in the doomed world you have created.");
		ini.SetValue("Message Settings", "MessageBox (important NPCs)", messageVIP.c_str(), ";Message that triggers for important quest NPCs.", true);

		messageSideQuest = ini.GetValue("Message Settings", "MessageBox (sidequest NPCs)", "With [npc]'s death, the thread of prophecy is damaged. Restore a saved game to repair the weave of fate, or persist in the doomed world you have created.");
		ini.SetValue("Message Settings", "MessageBox (sidequest NPCs)", messageSideQuest.c_str(), ";Message that triggers for side quest NPCs.", true);

		notificationVIP = ini.GetValue("Message Settings", "Notification (important NPCs)", "Your actions have broken the thread of prophecy...");
		ini.SetValue("Message Settings", "Notification (important NPCs)", notificationVIP.c_str(), ";Alternate notification that triggers for important quest NPCs.", true);

		notificationSideQuest = ini.GetValue("Settings", "Notification (sidequest NPCs)", "Your actions have caused a shift throughout the weave of fate...");
		ini.SetValue("Message Settings", "Notification (sidequest NPCs)", notificationSideQuest.c_str(), ";Alternate notification that triggers for side quest NPCs.", true);

		ini.SaveFile(path);
	}


	void OnInit(SKSE::MessagingInterface::Message* a_msg) {
		switch (a_msg->type) {
			case SKSE::MessagingInterface::kDataLoaded:
			{
				auto sourceHolder = RE::ScriptEventSourceHolder::GetSingleton();
				if (sourceHolder) {
					sourceHolder->AddEventSink(TESObjectLoadedEventHandler::GetSingleton());
					logger::info("Registered object loaded event handler");
					sourceHolder->AddEventSink(TESDeathEventHandler::GetSingleton());
					logger::info("Registered death event handler");
				}
			}
			break;
			default:
				break;
		}
	}
}

SKSEPluginLoad(const LoadInterface* a_skse) {
	Init(a_skse);

	LoadSettings();

	auto messaging = SKSE::GetMessagingInterface();
	if (!messaging->RegisterListener("SKSE", OnInit)) {
		return false;
	}

	logger::info("SKSEPluginLoad OK");

	return true;
}

SKSEPluginInfo(
	.Version = REL::Version{ 2, 0, 0, 0 },
	.Name = "EssentialsBegone",
	.Author = "PowerofThree",
	.StructCompatibility = SKSE::StructCompatibility::Independent,
	.RuntimeCompatibility = SKSE::VersionIndependence::AddressLibrary
);
