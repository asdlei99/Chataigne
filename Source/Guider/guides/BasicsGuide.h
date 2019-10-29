/*
  ==============================================================================

    BasicsGuide.h
    Created: 7 Nov 2018 2:57:23pm
    Author:  Ben

  ==============================================================================
*/

#pragma once

#include "../BaseGuide.h"
#include "Module/ModuleManager.h"
#include "Module/modules/controller/keyboard/KeyboardModule.h"
#include "StateMachine/ui/StateMachineView.h"
#include "Common/Processor/Action/Condition/conditions/StandardCondition/StandardCondition.h"
#include "Common/Processor/Action/Condition/conditions/StandardCondition/ui/StandardConditionEditor.h"
#include "Common/Command/ui/BaseCommandHandlerManagerEditor.h"
#include "Common/Command/ui/BaseCommandHandlerEditor.h"
#include "Module/ui/ModuleManagerUI.h"
#include "Module/modules/audio/AudioModule.h"
#include "Module/modules/audio/commands/PlayAudioFileCommand.h"

class BasicsGuide :
	public BaseGuide,
	public ModuleManager::AsyncListener,
	public StateMachineView::ManagerUIListener,
	public StandardCondition::AsyncListener,
	public Inspector::InspectorListener,
	public GenericControllableContainerEditor::ContainerEditorListener,
	public Parameter::AsyncListener
{
public:
	BasicsGuide();
	~BasicsGuide();

	enum Steps { 
		INTRO, 
		OBSERVE_MODULES, 
		ADD_KBD_MOD, 
		OBSERVE_INSPECTOR,
		ADD_AUDIO_MOD,
		OBSERVE_INSPECTOR_2,
		ADD_STATE,
		ADD_ACTION,
		ADD_CONDITION,
		SELECT_CONDITION_SOURCE,
		EDIT_CONDITION,
		OBSERVE_CONDITION,
		ADD_CONSEQUENCE,
		EDIT_CONSEQUENCE,
		END,
		STEPS_MAX };

	KeyboardModule * keyboardModule;
	AudioModule * audioModule;
	State * state;
	StandardCondition * condition;
	Parameter * conditionReference;
	Parameter * audioFile;

	Inspector * inspector;
	StateMachineView * smui;
	ModuleManagerUI * mmui; 
	StateViewUI * svui;
	GenericManagerEditor<Condition> * cme;
	BaseCommandHandlerManagerEditor<Consequence> * csme;
	StandardConditionEditor * ce;
	BaseCommandHandlerEditor * cse;

	void initInternal() override;
	void clear() override;
	void handleStep(int step) override;

	void newMessage(const ModuleManager::ManagerEvent &e) override;
	void newMessage(const StandardCondition::ConditionEvent &e) override;
	void newMessage(const Parameter::ParameterEvent &e) override;

	void currentInspectableChanged(Inspector *) override;

	void itemUIAdded(StateViewUI * svui) override;

	void containerRebuilt(GenericControllableContainerEditor * editor) override;

	static BasicsGuide * create(var) { return new BasicsGuide(); }
};
