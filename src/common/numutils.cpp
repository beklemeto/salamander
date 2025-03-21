// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

double clampUp(double value, double up)
{
	return value>up? up: value;
}

double clampDown(double value, double down)
{
    return value < down ? down : value;
}

double clamp(double value, double down,double up)
{
    return value > up ? up : value < down ? down : value;
}

int clampUp(int value, int up)
{
    return value > up ? up : value;
}

int clampDown(int value, int down)
{
    return value < down ? down : value;
}

int clamp(int value, int down, int up)
{
    return value > up ? up : value < down ? down : value;
}
