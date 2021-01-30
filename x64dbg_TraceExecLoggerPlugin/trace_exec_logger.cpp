#include "trace_exec_logger.h"


static duint skip_breakpoints_log_addr = 0;


void log_proc_info(const char* msg)
{
	if (msg == NULL || !get_proc_enabled())
	{
		return;
	}

	json entry = json::object();
	entry["type"] = "proc log";
	entry["module"] = log_module();
	entry["thread"] = log_thread();
	entry["memory"] = log_memory();
	entry["handle"] = log_handle();
	entry["network"] = log_network();
	entry["message"] = msg;

	add_log(DbgGetThreadId(), &entry);
}


void log_exec(const char* msg, duint cip)
{
	// 353 micro seconds
	if (msg == NULL || !get_telogger_enabled() || !should_log(cip))
	{
		return;
	}

	json entry = json::object();
	entry["type"] = "log";

	REGDUMP reg_dump;
	if (!DbgGetRegDumpEx(&reg_dump, sizeof(reg_dump)))
	{
		add_log(DbgGetThreadId(), &entry);
		return;
	}

	entry["inst"] = log_instruction(&reg_dump);
	// 34 micro seconds
	entry["reg"] = log_register(&reg_dump);
	// 175 micro seconds
	entry["stack"] = log_stack(&reg_dump);
	entry["message"] = msg;

	add_log(DbgGetThreadId(), &entry);

	flush_changed_memory();
}


bool command_callback(int argc, char* argv[])
{
	if (argc < 1)
	{
		return false;
	}

	if (strstr(argv[0], "help"))
	{
		telogger_logputs("Log: Help\n"
			"Command:\n"
			"    TElogger.help\n"
			"    TElogger.enable\n"
			"    TElogger.disable\n"
			"    TElogger.setdir dirname\n"
			"    TElogger.inst.help\n"
			"    TElogger.reg.help\n"
			"    TElogger.stack.help\n"
			"    TElogger.proc.help\n"
			"    TElogger.filt.help\n"
			"    TElogger.auto.help");
	}
	else if (strstr(argv[0], "enable"))
	{
		set_telogger_enabled(true);
		telogger_logputs("Log: Enabled");
	}
	else if (strstr(argv[0], "disable"))
	{
		set_telogger_enabled(false);
		telogger_logputs("Log: Disabled");
	}
	else if (strstr(argv[0], "setdir"))
	{
		if (argc < 2)
		{
			telogger_logputs("Log: Failed to set dir\n"
				"Command:\n"
				"    TElogger.setdir dirname");
			return false;
		}
		set_save_dir(argv[1]);
		telogger_logputs("Log: Setdir");
	}
	else if (strstr(argv[0], "save"))
	{
		save_all_thread_log();
		telogger_logputs("Log: Save");
	}

	return true;
}


extern "C" __declspec(dllexport) void CBMENUENTRY(CBTYPE, PLUG_CB_MENUENTRY* info)
{
	menu_callback(info);
}


extern "C" __declspec(dllexport) void CBTRACEEXECUTE(CBTYPE, PLUG_CB_TRACEEXECUTE* info)
{
	log_exec("Trace Execute Log", info->cip);
	skip_breakpoints_log_addr = info->cip;
}


extern "C" __declspec(dllexport) void CBSTEPPED(CBTYPE, PLUG_CB_STEPPED* info)
{
	bool result = false;
	duint cip = DbgEval("cip", &result);
	log_exec("Stepped Log", cip);
	if (result)
	{
		skip_breakpoints_log_addr = cip;
	}
}


extern "C" __declspec(dllexport) void CBBREAKPOINT(CBTYPE, PLUG_CB_BREAKPOINT* info)
{
	if (skip_breakpoints_log_addr == 0 || skip_breakpoints_log_addr != info->breakpoint->addr)
	{
		log_exec("Breakpoint Log", info->breakpoint->addr);
	}
	run_debug();
	skip_breakpoints_log_addr = 0;
}


extern "C" __declspec(dllexport) void CBPAUSEDEBUG(CBTYPE, PLUG_CB_PAUSEDEBUG* info)
{
	run_debug();
}


extern "C" __declspec(dllexport) void CBSYSTEMBREAKPOINT(CBTYPE, PLUG_CB_SYSTEMBREAKPOINT* info)
{
	bool result = false;
	duint cip = DbgEval("cip", &result);
	if (result) {
		skip_system_break_point(cip);
	}
}


extern "C" __declspec(dllexport) void CBINITDEBUG(CBTYPE, PLUG_CB_INITDEBUG* info)
{
	set_auto_run_enabled(false);
	set_file_name(info->szFileName);
	clear_cache();
}


extern "C" __declspec(dllexport) void CBSTOPDEBUG(CBTYPE, PLUG_CB_STOPDEBUG* info)
{
	save_all_thread_log();
	delete_all_log();
}


extern "C" __declspec(dllexport) void CBCREATEPROCESS(CBTYPE, PLUG_CB_CREATEPROCESS* info)
{
	telogger_logprintf("CREATEPROCESS ID = %d\n", info->fdProcessInfo->dwProcessId);
}


extern "C" __declspec(dllexport) void CBCREATETHREAD(CBTYPE, PLUG_CB_CREATETHREAD* info)
{
	telogger_logprintf("CREATETHREAD ID = %d\n", info->dwThreadId);
	log_proc_info("Create Thread Log");
	char cmd[DEFAULT_BUF_SIZE] = { 0 };
	_snprintf_s(cmd, sizeof(cmd), _TRUNCATE, "SetBPX %p, TEloggerStartThread, ss", info->CreateThread->lpStartAddress);
	DbgCmdExecDirect(cmd);
}


extern "C" __declspec(dllexport) void CBLOADDLL(CBTYPE, PLUG_CB_LOADDLL* info)
{
	telogger_logprintf("LOADDLL %s\n", info->modname);
	log_proc_info("Load Dll Log");
}


bool init_logger_plugin(PLUG_INITSTRUCT* init_struct)
{
	init_menu();

	init_instruction_log(init_struct);
	init_register_log(init_struct);
	init_stack_log(init_struct);
	init_proc_info_log(init_struct);
	init_filter_log(init_struct);
	init_save(init_struct);
	init_auto_run(init_struct);

	_plugin_registercommand(pluginHandle, "TElogger.help", command_callback, false);
	_plugin_registercommand(pluginHandle, "TElogger.enable", command_callback, false);
	_plugin_registercommand(pluginHandle, "TElogger.disable", command_callback, false);
	_plugin_registercommand(pluginHandle, "TElogger.setdir", command_callback, false);
	_plugin_registercommand(pluginHandle, "TElogger.save", command_callback, false);

	return true;
}


bool stop_logger_plugin()
{
	stop_menu();

	stop_instruction_log();
	stop_register_log();
	stop_stack_log();
	stop_proc_info_log();
	stop_filter_log();
	stop_save();
	stop_auto_run();

	_plugin_unregistercommand(pluginHandle, "TElogger.help");
	_plugin_unregistercommand(pluginHandle, "TElogger.enable");
	_plugin_unregistercommand(pluginHandle, "TElogger.disable");
	_plugin_unregistercommand(pluginHandle, "TElogger.setdir");
	_plugin_unregistercommand(pluginHandle, "TElogger.save");

	return true;
}


void setup_logger_plugin()
{
	setup_menu();

	setup_instruction_log();
	setup_register_log();
	setup_stack_log();
	setup_proc_info_log();
	setup_filter_log();
	setup_save();
	setup_auto_run();
}
