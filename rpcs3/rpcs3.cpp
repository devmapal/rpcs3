#include "stdafx_gui.h"
#include "Emu/Memory/Memory.h"
#include "Emu/System.h"
#include "rpcs3.h"
#include "Ini.h"
#include "Gui/ConLogFrame.h"
#include "Emu/GameInfo.h"

#include "Emu/Io/Keyboard.h"
#include "Emu/Io/Null/NullKeyboardHandler.h"
#include "Emu/Io/Windows/WindowsKeyboardHandler.h"

#include "Emu/Io/Mouse.h"
#include "Emu/Io/Null/NullMouseHandler.h"
#include "Emu/Io/Windows/WindowsMouseHandler.h"

#include "Emu/Io/Pad.h"
#include "Emu/Io/Null/NullPadHandler.h"
#include "Emu/Io/Windows/WindowsPadHandler.h"
#if defined(_WIN32)
#include "Emu/Io/XInput/XInputPadHandler.h"
#endif

#include "Emu/SysCalls/Modules/cellMsgDialog.h"
#include "Gui/MsgDialog.h"

#include "Gui/GLGSFrame.h"
#include <wx/stdpaths.h>

#ifdef _WIN32
#include <wx/msw/wrapwin.h>
#endif

#ifdef __unix__
#include <X11/Xlib.h>
#endif

wxDEFINE_EVENT(wxEVT_DBG_COMMAND, wxCommandEvent);

IMPLEMENT_APP(Rpcs3App)
Rpcs3App* TheApp;

bool Rpcs3App::OnInit()
{
	static const wxCmdLineEntryDesc desc[]
	{
		{ wxCMD_LINE_SWITCH, "h", "help", "Show this help message", wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },
		{ wxCMD_LINE_SWITCH, "t", "test", "Run in test mode on (S)ELF", wxCMD_LINE_VAL_NONE },
		{ wxCMD_LINE_PARAM, NULL, NULL, "(S)ELF", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
		{ wxCMD_LINE_NONE }
	};

	parser.SetDesc(desc);
	parser.SetCmdLine(argc, argv);
	if(parser.Parse())
	{
		// help was given, terminating
		this->Exit();
	}

	SetSendDbgCommandCallback([](DbgCommand id, CPUThread* t)
	{
		wxGetApp().SendDbgCommand(id, t);
	});
	SetCallAfterCallback([](std::function<void()> func)
	{
		wxGetApp().CallAfter(func);
	});
	SetGetKeyboardHandlerCountCallback([]()
	{
		return 2;
	});
	SetGetKeyboardHandlerCallback([](int i) -> KeyboardHandlerBase*
	{
		switch (i)
		{
		case 0:
			return new NullKeyboardHandler();
			break;
		case 1:
			return new WindowsKeyboardHandler();
			break;
		default:
			return new NullKeyboardHandler();
		}
	});
	SetGetMouseHandlerCountCallback([]()
	{
		return 2;
	});
	SetGetMouseHandlerCallback([](int i) -> MouseHandlerBase*
	{
		switch (i)
		{
		case 0:
			return new NullMouseHandler();
			break;
		case 1:
			return new WindowsMouseHandler();
			break;
		default:
			return new NullMouseHandler();
		}
	});
	SetGetPadHandlerCountCallback([]()
	{
#if defined(_WIN32)
		return 3;
#else
		return 2;
#endif
	});
	SetGetPadHandlerCallback([](int i) -> PadHandlerBase*
	{
		switch (i)
		{
		case 0:
			return new NullPadHandler();
			break;
		case 1:
			return new WindowsPadHandler();
			break;
#if defined(_WIN32)
		case 2:
			return new XInputPadHandler();
			break;
#endif
		default:
			return new NullPadHandler();
		}
	});
	SetGetGSFrameCallback([]() -> GSFrameBase*
	{
		return new GLGSFrame();
	});
	SetMsgDialogCreateCallback(MsgDialogCreate);
	SetMsgDialogDestroyCallback(MsgDialogDestroy);
	SetMsgDialogProgressBarSetMsgCallback(MsgDialogProgressBarSetMsg);
	SetMsgDialogProgressBarResetCallback(MsgDialogProgressBarReset);
	SetMsgDialogProgressBarIncCallback(MsgDialogProgressBarInc);

	TheApp = this;
	SetAppName(_PRGNAME_);
	wxInitAllImageHandlers();

	// RPCS3 assumes the current working directory is the folder where it is contained, so we make sure this is true
	const wxString executablePath = wxStandardPaths::Get().GetExecutablePath();
	wxSetWorkingDirectory(wxPathOnly(executablePath));

	main_thread = std::this_thread::get_id();

	Ini.Load();
	m_MainFrame = new MainFrame();
	SetTopWindow(m_MainFrame);
	Emu.Init();

	m_MainFrame->Show();
	m_MainFrame->DoSettings(true);

	OnArguments(parser);

	return true;
}

void Rpcs3App::OnArguments(const wxCmdLineParser& parser)
{
	// Usage:
	//   rpcs3-*.exe               Initializes RPCS3
	//   rpcs3-*.exe [(S)ELF]      Initializes RPCS3, then loads and runs the specified (S)ELF file.

	if (parser.FoundSwitch("t")) {
		HLEExitOnStop = Ini.HLEExitOnStop.GetValue();
		Ini.HLEExitOnStop.SetValue(true);
		if (parser.GetParamCount() != 1)
		{
			wxLogDebug(wxT("A (S)ELF file needs to be given in test mode, exiting."));
			this->Exit();
		}
	}

	if (parser.GetParamCount() > 0) {
		Emu.SetPath(fmt::ToUTF8(parser.GetParam(0)));
		Emu.Load();
		Emu.Run();
	}
}

void Rpcs3App::Exit()
{
	if (parser.FoundSwitch("t")) {
		Ini.HLEExitOnStop.SetValue(HLEExitOnStop);
	}
	Emu.Stop();
	Ini.Save();

	wxApp::Exit();

#ifdef _WIN32
	timeEndPeriod(1);
#endif
}

void Rpcs3App::SendDbgCommand(DbgCommand id, CPUThread* thr)
{
	wxCommandEvent event(wxEVT_DBG_COMMAND, id);
	event.SetClientData(thr);
	AddPendingEvent(event);
}

Rpcs3App::Rpcs3App()
{
#ifdef _WIN32
	timeBeginPeriod(1);
#endif

	#if defined(__unix__) && !defined(__APPLE__)
	XInitThreads();
	#endif
}

GameInfo CurGameInfo;
