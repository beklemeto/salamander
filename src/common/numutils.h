// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//template<typename T>
//T clampUp(T value, T up);
//
//template <typename T>
//T clampDown(T value, T down);
//
//template <typename T>
//T clamp(T value, T up,T down);


double clampUp(double value, double up);
double clampDown(double value, double down);
double clamp(double value, double down, double up);

int clampUp(int value, int up);
int clampDown(int value, int down);
int clamp(int value, int down, int up);
