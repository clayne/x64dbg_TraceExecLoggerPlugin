#pragma once

#include "x64dbg_TraceExecLoggerPlugin.h"

#define HEX_SIZE 0x30
#define PLUGIN_NAME_LOG_HEADER "[" PLUGIN_NAME "] "
#define telogger_logprintf(format, ...) _plugin_logprintf(PLUGIN_NAME_LOG_HEADER format, __VA_ARGS__)
#define telogger_logputs(text) _plugin_logputs(PLUGIN_NAME_LOG_HEADER text)
#define logprintf(format, ...) _plugin_logprintf(format, __VA_ARGS__)
#define logputs(text) _plugin_logputs(text)


void save_json_file(const char* file_name, SYSTEMTIME* system_time, int number, const char* buffer);
void make_address_label_string(duint addr, char* text, size_t text_size);
void make_hex_string(char* data, size_t data_size, char* text, size_t text_size);
void make_address_json(LOG_ADDRESS& address_json, duint addr);
