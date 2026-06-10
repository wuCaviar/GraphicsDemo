#pragma once

#include <string>
#include <windows.h>

#ifdef LAYOUT_LIBRARY
#define LAYOUT_API __declspec(dllexport)
#else
#define LAYOUT_API __declspec(dllimport)
#endif

struct LayoutInfo
{
	double element_interval;//毫米
	double double_progress_real;
	double paper_width;
	double total_height;
	double total_end;
	double save_height;
	bool bool_run;
	bool bool_RemoveFile;
	int loop_count;//线程数
	int operate_count;//排列数量
	double run_time;
	int resolution;//解决时间
	bool bool_dll_run;
	const char* intput_path;
	int intput_length;
	const char* output_path;
	int output_length;
	std::string error_info;

	LayoutInfo() {
		element_interval = 2.5;
		double_progress_real = 0;
		paper_width = 0;
		total_height = 0;
		total_end = 0;
		save_height = 0;
		bool_run = 0;
		bool_RemoveFile = 0;
		loop_count = 0;
		operate_count = 0;
		run_time = 0;
		resolution = 0;
		bool_dll_run = 0;
		intput_length = 0;
		output_length = 0;
	}
};

extern "C" LAYOUT_API void LayoutDAO(LayoutInfo * layoutinfo);
