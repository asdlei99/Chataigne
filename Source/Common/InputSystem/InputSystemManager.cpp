/*
  ==============================================================================

	InputSystemManager.cpp
	Created: 17 Jun 2018 5:57:00pm
	Author:  Ben

  ==============================================================================
*/

#include "InputSystemManager.h"
#include "InputDeviceHelpers.h"

juce_ImplementSingleton(InputSystemManager)

InputSystemManager::InputSystemManager() :
	Thread("ISM"),
	inputQueuedNotifier(10)
{

	isInit = SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) == 0;

	if (!isInit)
	{
		LOGERROR("Could not init Input System Manager : " << SDL_GetError());
		return;
	}

	lastCheckTime = 0;
	startThread();
}

InputSystemManager::~InputSystemManager()
{
	stopThread(1000);
	SDL_Quit();
}

Gamepad* InputSystemManager::addGamepad(Gamepad * g)
{
	gamepads.add(g);
	LOG("Gamepad added : " << g->getName());
	inputListeners.call(&InputManagerListener::gamepadAdded, g);
	inputQueuedNotifier.addMessage(new InputSystemEvent(InputSystemEvent::GAMEPAD_ADDED, g));
	return g;
}

void InputSystemManager::removeGamepad(Gamepad* g)
{
	if (!gamepads.contains(g)) return;
	gamepads.removeObject(g, false);
	
	LOG("Gamepad removed : " << g->getName());
	
	inputListeners.call(&InputManagerListener::gamepadRemoved, g);
	inputQueuedNotifier.addMessage(new InputSystemEvent(InputSystemEvent::GAMEPAD_REMOVED, g));
	SDL_GameControllerClose(g->gamepad);
	delete g;

}


Gamepad* InputSystemManager::getGamepadForSDL(SDL_Joystick* tj)
{
	for (auto& g : gamepads) if (g->joystick != nullptr && g->joystick == tj) return g;
	return nullptr;
}
Gamepad* InputSystemManager::getGamepadForSDL(SDL_GameController* tg)
{
	for (auto& g : gamepads) if (g->gamepad == tg) return g;
	return nullptr;
}

Gamepad* InputSystemManager::getGamepadForID(SDL_JoystickGUID id)
{
	for (auto& g : gamepads)
	{
		SDL_JoystickGUID guid = g->joystick != nullptr?SDL_JoystickGetGUID(g->joystick): SDL_JoystickGetGUID(SDL_GameControllerGetJoystick(g->gamepad));
		bool isTheSame = true;
		for (int i = 0; i < 16; ++i) if (guid.data[i] != id.data[i]) isTheSame = false;;
		if (isTheSame) return g;
	}
	return nullptr;
}

Gamepad* InputSystemManager::getGamepadForName(String name)
{
	for (auto& g : gamepads) if (String(SDL_GameControllerName(g->gamepad)) == name) return g;
	return nullptr;
}


void InputSystemManager::run()
{
	while (!threadShouldExit())
	{
		if (Engine::mainEngine->isClearing || Engine::mainEngine->isLoadingFile)
		{
			wait(20);
			continue;
		}

		if (Time::getApproximateMillisecondCounter() > lastCheckTime + checkDeviceTime)
		{
			int numDevices = SDL_NumJoysticks();

			for (int i = 0; i < numDevices; ++i)
			{
				if (SDL_IsGameController(i))
				{
					SDL_GameController* g = SDL_GameControllerOpen(i);
					if (g == NULL)
					{
						LOG("Unable to open gamepad : " << SDL_GetError());
						continue;
					}
					Gamepad* gp = getGamepadForSDL(g);
					if (gp == nullptr) addGamepad(new Gamepad(g));
				}
				else
				{
					SDL_Joystick* j = SDL_JoystickOpen(i);
					if (j == NULL)
					{
						LOG("Unable to open joystick : " << SDL_GetError());
						continue;
					}
					Gamepad* jj = getGamepadForSDL(j);
					if (jj == nullptr) addGamepad(new Gamepad(j));
				}
			}

			Array<Gamepad*> gamepadsToRemove;
			for (auto& g : gamepads)
			{
				//check removed devices
				if (g->joystick != nullptr)
				{
					if (!SDL_JoystickGetAttached(g->joystick)) gamepadsToRemove.add(g);
				}
				else
				{
					if (!SDL_GameControllerGetAttached(g->gamepad)) gamepadsToRemove.add(g);
				}
			}
			for (auto& g : gamepadsToRemove) removeGamepad(g);


			lastCheckTime = Time::getApproximateMillisecondCounter();
		}

		SDL_JoystickUpdate();
		SDL_GameControllerUpdate();

		gamepads.getLock().enter();
		for (auto& g : gamepads)
		{
			if (g != nullptr) g->update();
		}
		gamepads.getLock().exit();


		wait(20); //50fps
	}

}

//Joystick::Joystick(SDL_Joystick * joystick) :
//	joystick(joystick),
//	axesCC("Axes"),
//	buttonsCC("Buttons")
//{
//	int numAxes = SDL_JoystickNumAxes(joystick);
//	for (int i = 0; i <  numAxes; ++i)
//	{
//		FloatParameter * f = axesCC.addFloatParameter("Axis " + String(i + 1), "", 0, -1, 1);
//		axisOffset[i] = 0;
//		axisDeadZone[i] = 0;
//		f->isControllableFeedbackOnly = true;
//	}
//
//	int numButtons = SDL_JoystickNumButtons(joystick);
//	for (int i = 0; i < numButtons; ++i)
//	{
//		BoolParameter * b = buttonsCC.addBoolParameter("Button " + String(i + 1), "", false);
//		b->isControllableFeedbackOnly = true;
//	}
//}
//
//Joystick::~Joystick()
//{
//	masterReference.clear();
//}
//
//void Joystick::update()
//{
//	int numAxes = SDL_JoystickNumAxes(joystick);
//	if (axesCC.controllables.size() == numAxes)
//	{
//		GenericScopedLock lock(axesCC.controllables.getLock());
//		for (int i = 0; i < numAxes; ++i)
//		{
//			float axisValue = jmap<float>((float)SDL_JoystickGetAxis(joystick, i), INT16_MIN, INT16_MAX, -1, 1) + axisOffset[i];
//			if (fabs(axisValue) < axisDeadZone[i]) axisValue = 0;
//			else
//			{
//				if (axisValue > 0) axisValue = jmap<float>(axisValue, axisDeadZone[i], 1 + axisOffset[i], 0, 1);
//				else axisValue = jmap<float>(axisValue, -1 + axisOffset[i], -axisDeadZone[i], -1, 0);
//			}
//			((FloatParameter*)axesCC.controllables[i])->setValue(axisValue);
//		}
//	}
//
//	int numButtons = SDL_JoystickNumButtons(joystick);
//	if (buttonsCC.controllables.size() == numButtons)
//	{
//		GenericScopedLock lock(buttonsCC.controllables.getLock());
//		for (int i = 0; i < numButtons; ++i)
//		{
//			((BoolParameter*)buttonsCC.controllables[i])->setValue(SDL_JoystickGetButton(joystick, i) > 0);
//		}
//	}
//}

Gamepad::Gamepad(SDL_GameController* gamepad) :
	gamepad(gamepad),
	joystick(nullptr)
	//axesCC("Axes"),
	//buttonsCC("Buttons")
{

	//for (int i = 0; i < SDL_CONTROLLER_AXIS_MAX; ++i)
	//{
	//	SDL_GameControllerAxis a = (SDL_GameControllerAxis)i;

	//	FloatParameter * f = axesCC.addFloatParameter(SDL_GameControllerGetStringForAxis(a), "", 0, -1, 1);
	//	f->isControllableFeedbackOnly = true;
	//	axisOffset[i] = 0;
	//	axisDeadZone[i] = 0;
	//}

	//for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; ++i)
	//{
	//	SDL_GameControllerButton b = (SDL_GameControllerButton)i;
	//	BoolParameter * bp = buttonsCC.addBoolParameter(SDL_GameControllerGetStringForButton(b), "", false);
	//	bp->isControllableFeedbackOnly = true;
	//}
}

Gamepad::Gamepad(SDL_Joystick* joystick) :
	gamepad(nullptr),
	joystick(joystick)
{

}

Gamepad::~Gamepad()
{
	masterReference.clear();
}

void Gamepad::update()
{
	//if (axesCC.controllables.size() == SDL_CONTROLLER_AXIS_MAX)
	//{
	//	ScopedLock(axesCC.controllables.getLock());

	int numAxes = joystick != nullptr ? SDL_JoystickNumAxes(joystick) : SDL_CONTROLLER_AXIS_MAX;
	int numButtons = joystick != nullptr ? SDL_JoystickNumButtons(joystick) : SDL_CONTROLLER_BUTTON_MAX;

	Array<float> axes;
	Array<bool> buttons;

	for (int i = 0; i < numAxes; ++i)
	{
		float val = joystick != nullptr ? (float)SDL_JoystickGetAxis(joystick, i) : (float)SDL_GameControllerGetAxis(gamepad, (SDL_GameControllerAxis)i);
		axes.add(val);
	}

	for (int i = 0; i < numButtons; ++i)
	{
		bool val = joystick != nullptr ? (SDL_JoystickGetButton(joystick, i) > 0) : (SDL_GameControllerGetButton(gamepad, (SDL_GameControllerButton)i) > 0);
		buttons.add(val);
	}

	gamepadListeners.call(&GamepadListener::gamepadValuesUpdated, axes, buttons);
}

SDL_JoystickID Gamepad::getDevID()
{
	return joystick != nullptr ? SDL_JoystickInstanceID(joystick) : SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gamepad));
}

String Gamepad::getName()
{
	return joystick != nullptr ? SDL_JoystickName(joystick) : SDL_GameControllerName(gamepad);
}

String Gamepad::getAxisName(int index)
{
	return "Axis " + String(index + 1);// : SDL_GameControllerGetStringForAxis((SDL_GameControllerAxis)index);
}

String Gamepad::getButtonName(int index)
{
	return "Button " + String(index + 1);//joystick != nullptr ? SDL_GameControllerGetStringForButton((SDL_GameControllerButton)index);
}

//GAMEPAD PARAMETER

GamepadParameter::GamepadParameter(const String& name, const String& description) :
	Parameter(Controllable::CUSTOM, name, description, var(), var(), var()),
	gamepad(nullptr)
{
	for (int i = 0; i < 16; i++)
	{
		value.append(0);
		defaultValue.append(0);
	}
}

GamepadParameter::~GamepadParameter()
{
}

void GamepadParameter::setGamepad(Gamepad* j)
{
	if (j == gamepad) return;
	if (gamepad != nullptr && !gamepad.wasObjectDeleted())
	{
		//gamepad->removeGamepadListener(this);
	}

	gamepad = j;

	if (gamepad != nullptr && !gamepad.wasObjectDeleted())
	{

		//gamepad->addGamepadListener(this);
		var val;
		ghostID = SDL_JoystickGetGUID(SDL_GameControllerGetJoystick(gamepad->gamepad));
		ghostName = SDL_GameControllerName(gamepad->gamepad);
		for (int i = 0; i < 16; ++i) val.append((int)ghostID.data[i]);
		setValue(val, false, true);

	}
	else
	{
		var val;
		for (int i = 0; i < 16; ++i) val.append(0);
		setValue(val, false, true);
	}
}

var GamepadParameter::getJSONDataInternal()
{
	var data = Parameter::getJSONDataInternal();
	data.getDynamicObject()->setProperty("deviceName", ghostName);
	return data;
}

void GamepadParameter::loadJSONDataInternal(var data)
{
	if (data.isArray())
	{
		for (int i = 0; i < 16; ++i) ghostID.data[i] = (uint8_t)(int)data[i];
		setGamepad(InputSystemManager::getInstance()->getGamepadForID(ghostID));
	}

	if (gamepad == nullptr)
	{
		ghostName = data.getProperty("deviceName", "");
		setGamepad(InputSystemManager::getInstance()->getGamepadForName(ghostName));
	}
}

ControllableUI* GamepadParameter::createDefaultUI(Array<Controllable *> controllables)
{
	Array<GamepadParameter*> parameters = Inspectable::getArrayAs<Controllable, GamepadParameter>(controllables);
	if (parameters.size() == 0) parameters.add(this);
	return new GamepadParameterUI(parameters);
}
