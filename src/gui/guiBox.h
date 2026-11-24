// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <array>
#include <IGUIElement.h>
#include <IVideoDriver.h>
#include <IGUIEnvironment.h>

using namespace irr;

struct BhBgState {
	bool valid = false;
	u64 stamp = 0;
	s32 frame = 0, fw = 0, fh = 0, dx = 0, dy = 0, dw = 0, dh = 0;
};
extern BhBgState g_bh_bg;
bool bhPaintGlass(video::IVideoDriver *driver, const core::rect<s32> &rect, const core::rect<s32> &clip);

class GUIBox : public gui::IGUIElement
{
public:
	GUIBox(gui::IGUIEnvironment *env, gui::IGUIElement *parent, s32 id,
		const core::rect<s32> &rectangle,
		const std::array<video::SColor, 4> &colors,
		const std::array<video::SColor, 4> &bordercolors,
		const std::array<s32, 4> &borderwidths);

	virtual void draw() override;

private:
	std::array<video::SColor, 4> m_colors;
	std::array<video::SColor, 4> m_bordercolors;
	std::array<s32, 4> m_borderwidths;
};
