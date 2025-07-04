#include <SQG/API/PackageUtils.h>
#include <SQG/API/DialogUtils.h>
#include <SQG/API/QuestUtils.h>
#include <SQG/API/ConditionUtils.h>
#include <SQG/API/ScriptUtils.h>
#include <SQG/API/Data.h>

namespace 
{
	// # Papyrus
	// =======================
	RE::TESObjectREFR* activator = nullptr;
	RE::TESObjectREFR* targetActivator = nullptr;
	size_t generatedQuestIndex = 0;

	std::vector<RE::TESQuest*> generatedQuests;
	std::vector<RE::TESObjectREFR*> targets;

	void FillQuestWithGeneratedData(RE::TESQuest* inQuest, RE::TESObjectREFR* inTarget)
	{
		//Parametrize quest
		//=======================
		auto questIndex = std::format("{:02}", generatedQuestIndex);
		inQuest->SetFormEditorID(("SQGDebug" + questIndex).c_str());
		inQuest->fullName = questIndex + "_SQG_POC";
		++generatedQuestIndex;

		//Add stages
		//=======================
		SQG::AddQuestStage(inQuest, 10, SQG::QuestStageType::Startup, "My boss told me to kill a man named \"Gibier\"", 0);
		SQG::AddQuestStage(inQuest, 11);
		SQG::AddQuestStage(inQuest, 45, SQG::QuestStageType::Shutdown, "I decided to spare Gibier.", 6);
		SQG::AddQuestStage(inQuest, 40, SQG::QuestStageType::Shutdown, "Gibier is dead.", 5);
		SQG::AddQuestStage(inQuest, 35, SQG::QuestStageType::Default, "When I told Gibier I was going to kill him he asked for a last will. I refused.", 4);
		SQG::AddQuestStage(inQuest, 32, SQG::QuestStageType::Default, "Gibier has done his last will. The time has came for him to die.", 3);
		SQG::AddQuestStage(inQuest, 30);
		SQG::AddQuestStage(inQuest, 20);
		SQG::AddQuestStage(inQuest, 15, SQG::QuestStageType::Default, "When I told Gibier I was going to kill him he asked for a last will. I let him do what he wanted but advised him to not do anything inconsiderate.", 2);
		SQG::AddQuestStage(inQuest, 12, SQG::QuestStageType::Default, "I spoke with Gibier, whom told me my boss was a liar and begged me to spare him. I need to decide what to do.", 1);

		//Add objectives
		//=======================
		SQG::AddObjective(inQuest, 10, "Kill Gibier", { 0 });
		SQG::AddObjective(inQuest, 12, "(Optional) Spare Gibier");
		SQG::AddObjective(inQuest, 15, "(Optional) Let Gibier do his last will");

		//Add aliases
		//=======================
		SQG::AddRefAlias(inQuest, 0, "SQGTestAliasTarget", inTarget);

		//Add script logic
		//===========================
		std::ifstream ifs;
		for (std::filesystem::path scriptPath = "./Data/SKSE/Plugins/SQGLibSample/"; const auto& file : std::filesystem::directory_iterator{ scriptPath })
		{
			ifs.open(file, std::ifstream::ate);
			auto size = ifs.tellg();
			ifs.close();
			char* buf = new char[size];
			std::memset(buf, 0, size);
			ifs.open(file);
			ifs.read(buf, size);
			ifs.close();

			for (auto i = 0; i < size; ++i)
			{
				if (buf[i] == 'Z')
				{
					buf[i] = questIndex[0];
					buf[i + 1] = questIndex[1];
					break;
				}
			}
			SQG::AddScript(file.path().stem().string() + questIndex, buf);
			delete[] buf;
		}

		auto* dataManager = SQG::DataManager::GetSingleton();

		auto* scriptMachine = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		auto* policy = scriptMachine->GetObjectHandlePolicy();

		const auto selectedQuestHandle = policy->GetHandleForObject(RE::FormType::Quest, inQuest);
		RE::BSTSmartPointer<RE::BSScript::Object> questCustomScriptObject;
		scriptMachine->CreateObjectWithProperties("SQGDebug" + questIndex, 1, questCustomScriptObject);
		scriptMachine->BindObject(questCustomScriptObject, selectedQuestHandle, false);
		dataManager->questsData[inQuest->formID].script = questCustomScriptObject.get();

		RE::BSTSmartPointer<RE::BSScript::Object> referenceAliasBaseScriptObject;
		scriptMachine->CreateObject("SQGQuestTargetScript" + questIndex, referenceAliasBaseScriptObject);
		const auto questAliasHandle = selectedQuestHandle & 0x0000FFFFFFFF;
		scriptMachine->BindObject(referenceAliasBaseScriptObject, questAliasHandle, false);

		RE::BSScript::Variable propertyValue;
		propertyValue.SetObject(referenceAliasBaseScriptObject);
		scriptMachine->SetPropertyValue(questCustomScriptObject, ("SQGTestAliasTarget" + questIndex).c_str(), propertyValue);
		questCustomScriptObject->initialized = true; //Required when creating object with properties

		RE::BSTSmartPointer<RE::BSScript::Object> dialogFragmentsCustomScriptObject;
		scriptMachine->CreateObject("TF_SQGDebug" + questIndex, dialogFragmentsCustomScriptObject);
		scriptMachine->BindObject(dialogFragmentsCustomScriptObject, selectedQuestHandle, false);

		//Add packages
		//=======================

		//ACQUIRE PACKAGE
		//=============================
		{
			std::unordered_map<std::string, SQG::PackageData> packageDataMap;
			RE::PackageTarget::Target targetData{};
			targetData.objType = RE::PACKAGE_OBJECT_TYPE::kWEAP;
			packageDataMap["Target Criteria"] = SQG::PackageData(RE::PackageTarget::Type::kObjectType, targetData);
			packageDataMap["Num to acquire"] = SQG::PackageData({ .i = 2 });
			packageDataMap["AllowStealing"] = SQG::PackageData({ .b = true });
			auto* customAcquirePackage = SQG::CreatePackageFromTemplate
			(
				dataManager->acquirePackage,
				inQuest,
				packageDataMap,
				{ SQG::CreateCondition(inQuest, RE::FUNCTION_DATA::FunctionID::kGetStage, RE::CONDITION_ITEM_DATA::OpCode::kEqualTo, {.f = 15.f}) });

			const auto packageHandle = policy->GetHandleForObject(RE::FormType::Package, customAcquirePackage);
			RE::BSTSmartPointer<RE::BSScript::Object> packageCustomScriptObject;
			scriptMachine->CreateObject("PF_SQGAcquirePackage" + questIndex, packageCustomScriptObject);
			scriptMachine->BindObject(packageCustomScriptObject, packageHandle, false);

			SQG::AddAliasPackage(inQuest, inTarget, customAcquirePackage, "PF_SQGAcquirePackage" + questIndex);
		}

		//ACTIVATE PACKAGE
		//=============================
		{
			std::unordered_map<std::string, SQG::PackageData> packageDataMap;
			RE::PackageTarget::Target targetData{};
			targetData.handle = targetActivator->CreateRefHandle();
			packageDataMap["Target"] = SQG::PackageData(RE::PackageTarget::Type::kNearReference, targetData);
			auto* customActivatePackage = SQG::CreatePackageFromTemplate
			(
				dataManager->activatePackage,
				inQuest,
				packageDataMap,
				{ SQG::CreateCondition(inQuest, RE::FUNCTION_DATA::FunctionID::kGetStage, RE::CONDITION_ITEM_DATA::OpCode::kEqualTo, {.f = 20.f}) }
			);

			const auto packageHandle = policy->GetHandleForObject(RE::FormType::Package, customActivatePackage);
			RE::BSTSmartPointer<RE::BSScript::Object> packageCustomScriptObject;
			scriptMachine->CreateObject("PF_SQGActivatePackage" + questIndex, packageCustomScriptObject);
			scriptMachine->BindObject(packageCustomScriptObject, packageHandle, false);

			SQG::AddAliasPackage(inQuest, inTarget, customActivatePackage, "PF_SQGActivatePackage" + questIndex);
		}

		//TRAVEL PACKAGE
		//=============================
		{
			std::unordered_map<std::string, SQG::PackageData> packageDataMap;
			packageDataMap["Place to Travel"] = SQG::PackageData(RE::PackageLocation::Type::kNearReference, { .refHandle = activator->CreateRefHandle() }, 0);
			auto* customTravelPackage = SQG::CreatePackageFromTemplate
			(
				dataManager->travelPackage,
				inQuest,
				packageDataMap,
				{ SQG::CreateCondition(inQuest, RE::FUNCTION_DATA::FunctionID::kGetStage, RE::CONDITION_ITEM_DATA::OpCode::kEqualTo, {.f = 30.f}) }
			);

			const auto packageHandle = policy->GetHandleForObject(RE::FormType::Package, customTravelPackage);
			RE::BSTSmartPointer<RE::BSScript::Object> packageCustomScriptObject;
			scriptMachine->CreateObject("PF_SQGTravelPackage" + questIndex, packageCustomScriptObject);
			scriptMachine->BindObject(packageCustomScriptObject, packageHandle, false);

			SQG::AddAliasPackage(inQuest, inTarget, customTravelPackage, "PF_SQGTravelPackage" + questIndex);
		}

		//Add dialogs
		//===========================

		auto* checkSpeakerCondition = SQG::CreateCondition(inTarget->data.objectReference, RE::FUNCTION_DATA::FunctionID::kGetIsID, RE::CONDITION_ITEM_DATA::OpCode::kEqualTo, { .f = 1.f });
		auto* aboveStage0Condition = SQG::CreateCondition(inQuest, RE::FUNCTION_DATA::FunctionID::kGetStage, RE::CONDITION_ITEM_DATA::OpCode::kGreaterThan, { .f = 0.f });
		auto* aboveStage10Condition = SQG::CreateCondition(inQuest, RE::FUNCTION_DATA::FunctionID::kGetStage, RE::CONDITION_ITEM_DATA::OpCode::kGreaterThan, { .f = 10.f });
		auto* underStage11Condition = SQG::CreateCondition(inQuest, RE::FUNCTION_DATA::FunctionID::kGetStage, RE::CONDITION_ITEM_DATA::OpCode::kLessThan, { .f = 11.f });
		auto* underStage12Condition = SQG::CreateCondition(inQuest, RE::FUNCTION_DATA::FunctionID::kGetStage, RE::CONDITION_ITEM_DATA::OpCode::kLessThan, { .f = 12.f });
		auto* aboveStage12Condition = SQG::CreateCondition(inQuest, RE::FUNCTION_DATA::FunctionID::kGetStage, RE::CONDITION_ITEM_DATA::OpCode::kGreaterThanOrEqualTo, { .f = 12.f });
		auto* underStage15Condition = SQG::CreateCondition(inQuest, RE::FUNCTION_DATA::FunctionID::kGetStage, RE::CONDITION_ITEM_DATA::OpCode::kLessThan, { .f = 15.f });

		auto* wonderEntry = SQG::AddDialogTopic(inQuest, inTarget, "I've not decided yet. I'd like to hear your side of the story.");
		wonderEntry->AddAnswer
		(
			"Thank you very much, you'll see that I don't deserve this cruelty. Your boss is a liar.",
			"",
			{ checkSpeakerCondition, aboveStage0Condition, underStage11Condition },
			11,
			4
		);
		wonderEntry->AddAnswer
		(
			"Your boss is a liar.",
			"Tell me again about the reasons of the contract",
			{ checkSpeakerCondition, aboveStage10Condition, underStage15Condition }
		);
		auto* moreWonderEntry = SQG::AddDialogTopic(inQuest, inTarget, "What did he do ?", wonderEntry);
		moreWonderEntry->AddAnswer
		(
			"He lied, I'm the good one in the story.",
			"",
			{},
			12,
			0
		);

		auto* attackEntry = SQG::AddDialogTopic(inQuest, inTarget, "As you guessed. Prepare to die !");
		attackEntry->AddAnswer
		(
			"Wait a minute ! Could I have a last will please ?",
			"",
			{ checkSpeakerCondition, aboveStage0Condition, underStage12Condition },
			11,
			4
		);
		attackEntry->AddAnswer
		(
			"Wait a minute ! Could I have a last will please ?",
			"I can't believe my boss would lie. Prepare to die !",
			{ checkSpeakerCondition, aboveStage0Condition, underStage15Condition }
		);
		auto* lastWillYesEntry = SQG::AddDialogTopic(inQuest, inTarget, "Yes, of course, proceed but don't do anything inconsiderate.", attackEntry);
		lastWillYesEntry->AddAnswer
		(
			"Thank you for your consideration",
			"",
			{},
			15,
			1
		);
		auto* lastWillNoEntry = SQG::AddDialogTopic(inQuest, inTarget, "No, I came here for business, not charity.", attackEntry);
		lastWillNoEntry->AddAnswer
		(
			"Your lack of dignity is a shame.",
			"",
			{},
			35,
			2
		);

		auto* spareEntry = SQG::AddDialogTopic(inQuest, inTarget, "Actually, I decided to spare you.");
		spareEntry->AddAnswer
		(
			"You're the kindest. I will make sure to hide myself from the eyes of your organization.",
			"",
			{ checkSpeakerCondition, aboveStage12Condition, underStage15Condition },
			45,
			3
		);

		SQG::AddForceGreet(inQuest, inTarget, "So you came here to kill me, right ?", { underStage11Condition });
		SQG::AddHelloTopic(inTarget, "What did you decide then ?");
	}

	std::string GenerateQuest(RE::StaticFunctionTag*, RE::TESObjectREFR* inTarget)
	{
		targets.push_back(inTarget);

		if (generatedQuestIndex > 99)
		{
			return "Can't generate more than 100 quests";
		}

		const auto generatedQuest = SQG::CreateQuest();
		generatedQuests.push_back(generatedQuest);
		FillQuestWithGeneratedData(generatedQuest, inTarget);
		generatedQuest->Start();

		//Notify success
		//===========================
		std::ostringstream ss;
		ss << std::hex << generatedQuest->formID;
		const auto formId = ss.str();
		SKSE::log::debug("Generated quest with formID: {0}"sv, formId);
		return "Generated quest with formID: " + formId;
	}

	void PrintGeneratedQuests(RE::StaticFunctionTag*)
	{
		for(const auto& quest : generatedQuests)
		{
			std::ostringstream ss;
			ss << std::hex << quest->formID;
			SKSE::log::info("{0}:{1}"sv, quest->fullName.c_str(), ss.str());
		}
	}

	void DraftDebugFunction(RE::StaticFunctionTag*)
	{
		// ReSharper disable once CppDeclaratorNeverUsed
		[[maybe_unused]] int z = 42;
	}

	bool RegisterFunctions(RE::BSScript::IVirtualMachine* inScriptMachine)
	{
		inScriptMachine->RegisterFunction("GenerateQuest", "SQGLib", GenerateQuest);
		inScriptMachine->RegisterFunction("PrintGeneratedQuests", "SQGLib", PrintGeneratedQuests);
		inScriptMachine->RegisterFunction("DraftDebugFunction", "SQGLib", DraftDebugFunction);
		return true;
	}

	// # SKSE
	// =======================

	void InitializeLog()
	{
	#ifndef NDEBUG
		auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
	#else
		auto path = SKSE::log::log_directory();
		if (!path)
		{
			util::report_and_fail("Failed to find standard logging directory"sv);
		}

		*path /= fmt::format("{}.log"sv, Plugin::NAME);
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
	#endif

	#ifndef NDEBUG
		constexpr auto level = spdlog::level::trace;
	#else
		constexpr auto level = spdlog::level::info;
	#endif

		auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
		log->set_level(level);
		log->flush_on(level);

		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("%g(%#): [%^%l%$] %v"s);
	}
}

// ReSharper disable once CppInconsistentNaming
extern "C" DLLEXPORT bool SKSEPlugin_Query(const SKSE::QueryInterface* inQueryInterface, SKSE::PluginInfo* outPluginInfo)
{
	if(inQueryInterface->IsEditor())
	{
		SKSE::log::critical("This plugin is not designed to run in Editor\n"sv);
		return false;
	}

	if(const auto runtimeVersion = inQueryInterface->RuntimeVersion(); runtimeVersion < SKSE::RUNTIME_1_5_97)
	{
		SKSE::log::critical("Unsupported runtime version %s!\n"sv, runtimeVersion.string());
		return false;
	}

	outPluginInfo->infoVersion = SKSE::PluginInfo::kVersion;
	outPluginInfo->name = Plugin::NAME.data();
	outPluginInfo->version = Plugin::VERSION[0];

	return true;
}

// ReSharper disable once CppInconsistentNaming
extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* inLoadInterface)
{
#ifndef NDEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	InitializeLog();
	SKSE::log::info("{} v{}"sv, Plugin::NAME, Plugin::VERSION.string());

	Init(inLoadInterface);

	if(const auto papyrusInterface = SKSE::GetPapyrusInterface(); !papyrusInterface || !papyrusInterface->Register(RegisterFunctions))
	{
		return false;
	}

	if(const auto messaging = SKSE::GetMessagingInterface(); !messaging
		|| !messaging->RegisterListener([](SKSE::MessagingInterface::Message* message)
			{
				if(auto* dataHandler = RE::TESDataHandler::GetSingleton(); message->type == SKSE::MessagingInterface::kDataLoaded)
				{
					activator = reinterpret_cast<RE::TESObjectREFR*>(dataHandler->LookupForm(RE::FormID{ 0x001885 }, "SQGLibSample.esp"));
					targetActivator = reinterpret_cast<RE::TESObjectREFR*>(dataHandler->LookupForm(RE::FormID{ 0x008438 }, "SQGLibSample.esp"));
				}
				else if(message->type == SKSE::MessagingInterface::kPostLoadGame)
				{
					generatedQuests.clear();
					targets.clear();

					auto* dataManager = SQG::DataManager::GetSingleton();
					generatedQuestIndex = dataManager->questsData.size();
					for(auto& questData : dataManager->questsData | std::views::values)
					{
						generatedQuests.push_back(questData.quest);
						targets.push_back(reinterpret_cast<RE::BGSRefAlias*>(questData.quest->aliases[0])->GetReference());
					}
				}
			})
		)
	{
		return false;
	}

	return true;
}
