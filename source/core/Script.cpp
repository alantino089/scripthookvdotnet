/**
 * Copyright (C) 2015 crosire
 *
 * This software is  provided 'as-is', without any express  or implied  warranty. In no event will the
 * authors be held liable for any damages arising from the use of this software.
 * Permission  is granted  to anyone  to use  this software  for  any  purpose,  including  commercial
 * applications, and to alter it and redistribute it freely, subject to the following restrictions:
 *
 *   1. The origin of this software must not be misrepresented; you must not claim that you  wrote the
 *      original  software. If you use this  software  in a product, an  acknowledgment in the product
 *      documentation would be appreciated but is not required.
 *   2. Altered source versions must  be plainly  marked as such, and  must not be  misrepresented  as
 *      being the original software.
 *   3. This notice may not be removed or altered from any source distribution.
 */

#include "ScriptDomain.hpp"
#include "Menu.hpp"
#include "Settings.hpp"

namespace GTA
{
	using namespace System;
	using namespace System::Threading;
	using namespace System::Windows::Forms;
	using namespace System::Collections::Concurrent;

	extern void HandleUnhandledException(Object ^sender, UnhandledExceptionEventArgs ^args);

	Script::Script() : _interval(0), _running(false), _filename(ScriptDomain::CurrentDomain->LookupScriptFilename(this)), _scriptdomain(ScriptDomain::CurrentDomain), _waitEvent(gcnew AutoResetEvent(false)), _continueEvent(gcnew AutoResetEvent(false)), _keyboardEvents(gcnew ConcurrentQueue<Tuple<bool, KeyEventArgs ^> ^>())
	{
	}
	Script::~Script()
	{
	}

	Viewport ^Script::View::get()
	{
		if (Object::ReferenceEquals(_viewport, nullptr))
		{
			_viewport = gcnew GTA::Viewport();

			Tick += gcnew EventHandler(this, &Script::HandleViewportDraw);
			KeyUp += gcnew KeyEventHandler(this, &Script::HandleViewportInput);
		}

		return _viewport;
	}
	ScriptSettings ^Script::Settings::get()
	{
		if (Object::ReferenceEquals(_settings, nullptr))
		{
			String ^path = IO::Path::ChangeExtension(_filename, ".ini");

			if (IO::File::Exists(path))
			{
				_settings = ScriptSettings::Load(path);
			}
			else
			{
				_settings = gcnew ScriptSettings(path);
			}
		}

		return _settings;
	}
	int Script::Interval::get()
	{
		return _interval;
	}
	void Script::Interval::set(int value)
	{
		if (value < 0)
		{
			value = 0;
		}

		_interval = value;
	}

	void Script::Abort()
	{
		_waitEvent->Set();

		_scriptdomain->AbortScript(this);
	}
	void Script::Wait(int ms)
	{
		Script ^script = ScriptDomain::ExecutingScript;

		if (Object::ReferenceEquals(script, nullptr) || !script->_running)
		{
			throw gcnew InvalidOperationException("Illegal call to 'Script.Wait()' outside main loop!");
		}

		const DateTime resume = DateTime::Now + TimeSpan::FromMilliseconds(ms);

		do
		{
			script->_waitEvent->Set();
			script->_continueEvent->WaitOne();
		}
		while (DateTime::Now < resume);
	}
	void Script::Yield()
	{
		Wait(0);
	}

	void Script::MainLoop()
	{
		// Wait for domain to run scripts
		_continueEvent->WaitOne();

		// Run main loop
		while (_running)
		{
			Tuple<bool, KeyEventArgs ^> ^keyevent = nullptr;

			// Process events
			while (_keyboardEvents->TryDequeue(keyevent))
			{
				try
				{
					if (keyevent->Item1)
					{
						KeyDown(this, keyevent->Item2);
					}
					else
					{
						KeyUp(this, keyevent->Item2);
					}
				}
				catch (Exception ^ex)
				{
					HandleUnhandledException(this, gcnew UnhandledExceptionEventArgs(ex, false));
					break;
				}
			}

			try
			{
				Tick(this, EventArgs::Empty);
			}
			catch (Exception ^ex)
			{
				HandleUnhandledException(this, gcnew UnhandledExceptionEventArgs(ex, true));

				Abort();
				break;
			}

			// Yield execution to next tick
			Wait(_interval);
		}
	}
	void Script::HandleViewportDraw(Object ^sender, EventArgs ^e)
	{
		_viewport->Draw();
	}
	void Script::HandleViewportInput(Object ^sender, KeyEventArgs ^e)
	{
		if (e->KeyCode == ActivateKey) _viewport->HandleActivate();
		else if (e->KeyCode == BackKey) _viewport->HandleBack();
		else if (e->KeyCode == LeftKey) _viewport->HandleChangeItem(false);
		else if (e->KeyCode == RightKey) _viewport->HandleChangeItem(true);
		else if (e->KeyCode == UpKey) _viewport->HandleChangeSelection(false);
		else if (e->KeyCode == DownKey) _viewport->HandleChangeSelection(true);
	}
}