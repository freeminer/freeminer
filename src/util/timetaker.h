// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <string>
#include "irrlichttypes.h"

enum TimePrecision : s8
{
	PRECISION_SECONDS,
	PRECISION_MILLI,
	PRECISION_MICRO,
	PRECISION_NANO,
};

constexpr const char *TimePrecision_units[] = {
	"s"  /* PRECISION_SECONDS */,
	"ms" /* PRECISION_MILLI */,
	"us" /* PRECISION_MICRO */,
	"ns" /* PRECISION_NANO */,
};

/*
	TimeTaker
*/
extern unsigned int g_time_taker_enabled;

// Note: this class should be kept lightweight

class TimeTaker
{
public:
	// in freeminer timetaker by default disabled for release builds.
	// to count time should call start(): 

	TimeTaker(const std::string &name, u64 *result = nullptr,
		TimePrecision prec = PRECISION_MILLI)
	{
		if (result)
			m_result = result;
		else
			m_name = name;
		m_precision = prec;

		if (!g_time_taker_enabled) {
			m_running = false;
			return;
		}

		start();
	}

	~TimeTaker()
	{
		stop();
	}

	u64 stop(bool quiet=false);

	u64 getTimerTime();

	void start();
private:

	std::string m_name;
	u64 *m_result = nullptr;
	u64 m_time1{};
	bool m_running = true;
	TimePrecision m_precision;
};
