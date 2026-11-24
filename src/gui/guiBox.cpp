// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "guiBox.h"
#include "porting.h"
#include "filesys.h"
#include <algorithm>
#include <string>

BhBgState g_bh_bg;

bool bhPaintGlass(video::IVideoDriver *driver, const core::rect<s32> &rect, const core::rect<s32> &clip)
{
	if (!g_bh_bg.valid || g_bh_bg.dw <= 0 || porting::getTimeMs() - g_bh_bg.stamp >= 250)
		return false;
	static video::ITexture *blur_tex = nullptr;
	static bool blur_missing = false;
	if (!blur_tex && !blur_missing) {
		std::string p = porting::path_share + DIR_DELIM + "textures" + DIR_DELIM +
				"base" + DIR_DELIM + "pack" + DIR_DELIM + "blackhole_anim_blur.png";
		blur_tex = driver->getTexture(p.c_str());
		if (!blur_tex)
			blur_missing = true;
	}
	if (!blur_tex)
		return false;
	core::rect<s32> d = rect;
	d.clipAgainst(clip);
	d.clipAgainst(core::rect<s32>(g_bh_bg.dx, g_bh_bg.dy,
			g_bh_bg.dx + g_bh_bg.dw, g_bh_bg.dy + g_bh_bg.dh));
	if (d.getWidth() <= 0 || d.getHeight() <= 0)
		return false;
	f32 sc = (f32)g_bh_bg.fw / (f32)g_bh_bg.dw;
	s32 top = g_bh_bg.frame * g_bh_bg.fh;
	core::rect<s32> sr(
		(s32)((d.UpperLeftCorner.X - g_bh_bg.dx) * sc),
		(s32)((d.UpperLeftCorner.Y - g_bh_bg.dy) * sc) + top,
		(s32)((d.LowerRightCorner.X - g_bh_bg.dx) * sc),
		(s32)((d.LowerRightCorner.Y - g_bh_bg.dy) * sc) + top);
	sr.LowerRightCorner.X = std::min(sr.LowerRightCorner.X, g_bh_bg.fw);
	sr.LowerRightCorner.Y = std::min(sr.LowerRightCorner.Y, top + g_bh_bg.fh);
	driver->draw2DImage(blur_tex, d, sr);
	driver->draw2DRectangle(video::SColor(150, 12, 10, 8), d);
	driver->draw2DRectangleOutline(d, video::SColor(60, 255, 255, 255));
	return true;
}
#include <IVideoDriver.h>
#include "irr_v2d.h"

GUIBox::GUIBox(gui::IGUIEnvironment *env, gui::IGUIElement *parent, s32 id,
	const core::rect<s32> &rectangle,
	const std::array<video::SColor, 4> &colors,
	const std::array<video::SColor, 4> &bordercolors,
	const std::array<s32, 4> &borderwidths) :
	gui::IGUIElement(gui::EGUIET_ELEMENT, env, parent, id, rectangle),
	m_colors(colors),
	m_bordercolors(bordercolors),
	m_borderwidths(borderwidths)
{
}

void GUIBox::draw()
{
	if (!IsVisible)
		return;

	std::array<s32, 4> negative_borders = {0, 0, 0, 0};
	std::array<s32, 4> positive_borders = {0, 0, 0, 0};

	for (size_t i = 0; i <= 3; i++) {
		if (m_borderwidths[i] > 0)
			positive_borders[i] = m_borderwidths[i];
		else
			negative_borders[i] = m_borderwidths[i];
	}

	v2s32 upperleft = AbsoluteRect.UpperLeftCorner;
	v2s32 lowerright = AbsoluteRect.LowerRightCorner;

	v2s32 topleft_border = {
		upperleft.X - positive_borders[3],
		upperleft.Y - positive_borders[0]
	};
	v2s32 topleft_rect = {
		upperleft.X - negative_borders[3],
		upperleft.Y - negative_borders[0]
	};

	v2s32 lowerright_border = {
		lowerright.X + positive_borders[1],
		lowerright.Y + positive_borders[2]
	};
	v2s32 lowerright_rect = {
		lowerright.X + negative_borders[1],
		lowerright.Y + negative_borders[2]
	};

	core::rect<s32> main_rect(
		topleft_rect.X,
		topleft_rect.Y,
		lowerright_rect.X,
		lowerright_rect.Y
	);

	std::array<core::rect<s32>, 4> border_rects;

	border_rects[0] = core::rect<s32>(
		topleft_border.X,
		topleft_border.Y,
		lowerright_border.X,
		topleft_rect.Y
	);

	border_rects[1] = core::rect<s32>(
		lowerright_rect.X,
		topleft_rect.Y,
		lowerright_border.X,
		lowerright_rect.Y
	);

	border_rects[2] = core::rect<s32>(
		topleft_border.X,
		lowerright_rect.Y,
		lowerright_border.X,
		lowerright_border.Y
	);

	border_rects[3] = core::rect<s32>(
		topleft_border.X,
		topleft_rect.Y,
		topleft_rect.X,
		lowerright_rect.Y
	);

	video::IVideoDriver *driver = Environment->getVideoDriver();

	std::array<video::SColor, 4> fill = m_colors;
	bool glass = false;
	if (g_bh_bg.valid && g_bh_bg.dw > 0 && porting::getTimeMs() - g_bh_bg.stamp < 250) {
		static video::ITexture *blur_tex = nullptr;
		static bool blur_missing = false;
		if (!blur_tex && !blur_missing) {
			std::string p = porting::path_share + DIR_DELIM + "textures" + DIR_DELIM +
					"base" + DIR_DELIM + "pack" + DIR_DELIM + "blackhole_anim_blur.png";
			blur_tex = driver->getTexture(p.c_str());
			if (!blur_tex)
				blur_missing = true;
		}
		if (blur_tex) {
			core::rect<s32> d = main_rect;
			d.clipAgainst(AbsoluteClippingRect);
			d.clipAgainst(core::rect<s32>(g_bh_bg.dx, g_bh_bg.dy,
					g_bh_bg.dx + g_bh_bg.dw, g_bh_bg.dy + g_bh_bg.dh));
			if (d.getWidth() > 0 && d.getHeight() > 0) {
				f32 sc = (f32)g_bh_bg.fw / (f32)g_bh_bg.dw;
				s32 top = g_bh_bg.frame * g_bh_bg.fh;
				core::rect<s32> sr(
					(s32)((d.UpperLeftCorner.X - g_bh_bg.dx) * sc),
					(s32)((d.UpperLeftCorner.Y - g_bh_bg.dy) * sc) + top,
					(s32)((d.LowerRightCorner.X - g_bh_bg.dx) * sc),
					(s32)((d.LowerRightCorner.Y - g_bh_bg.dy) * sc) + top);
				sr.LowerRightCorner.X = std::min(sr.LowerRightCorner.X, g_bh_bg.fw);
				sr.LowerRightCorner.Y = std::min(sr.LowerRightCorner.Y, top + g_bh_bg.fh);
				{
					s32 mx = sr.getWidth() / 40, my = sr.getHeight() / 40;
					sr.UpperLeftCorner.X += mx;
					sr.UpperLeftCorner.Y += my;
					sr.LowerRightCorner.X -= mx;
					sr.LowerRightCorner.Y -= my;
				}
				driver->draw2DImage(blur_tex, d, sr);
				{
					s32 t = std::min<s32>(16, std::min(d.getWidth(), d.getHeight()) / 6);
					if (t > 3 && d.getWidth() > 3 * t && d.getHeight() > 3 * t) {
						f32 fx = (f32)sr.getWidth() / (f32)d.getWidth();
						f32 fy = (f32)sr.getHeight() / (f32)d.getHeight();
						s32 shx = (s32)(t * fx * 0.9f), shy = (s32)(t * fy * 0.9f);
						s32 lx = (s32)(t * fx * 0.45f), ly = (s32)(t * fy * 0.45f);
						core::rect<s32> UL = d;
						driver->draw2DImage(blur_tex,
							core::rect<s32>(d.UpperLeftCorner.X, d.UpperLeftCorner.Y, d.LowerRightCorner.X, d.UpperLeftCorner.Y + t),
							core::rect<s32>(sr.UpperLeftCorner.X, sr.UpperLeftCorner.Y + shy, sr.LowerRightCorner.X, sr.UpperLeftCorner.Y + shy + ly));
						driver->draw2DImage(blur_tex,
							core::rect<s32>(d.UpperLeftCorner.X, d.LowerRightCorner.Y - t, d.LowerRightCorner.X, d.LowerRightCorner.Y),
							core::rect<s32>(sr.UpperLeftCorner.X, sr.LowerRightCorner.Y - shy - ly, sr.LowerRightCorner.X, sr.LowerRightCorner.Y - shy));
						driver->draw2DImage(blur_tex,
							core::rect<s32>(d.UpperLeftCorner.X, d.UpperLeftCorner.Y, d.UpperLeftCorner.X + t, d.LowerRightCorner.Y),
							core::rect<s32>(sr.UpperLeftCorner.X + shx, sr.UpperLeftCorner.Y, sr.UpperLeftCorner.X + shx + lx, sr.LowerRightCorner.Y));
						driver->draw2DImage(blur_tex,
							core::rect<s32>(d.LowerRightCorner.X - t, d.UpperLeftCorner.Y, d.LowerRightCorner.X, d.LowerRightCorner.Y),
							core::rect<s32>(sr.LowerRightCorner.X - shx - lx, sr.UpperLeftCorner.Y, sr.LowerRightCorner.X - shx, sr.LowerRightCorner.Y));
					}
				}
				glass = true;
			}
		}
	}
	if (glass) {
		for (auto &c : fill)
			if (c.getAlpha() > 150)
				c.setAlpha(150);
	}
	driver->draw2DRectangle(main_rect, fill[0], fill[1], fill[3],
			fill[2], &AbsoluteClippingRect);
	if (glass && main_rect.getWidth() > 24 && main_rect.getHeight() > 24)
		{
		driver->draw2DRectangleOutline(main_rect, video::SColor(70, 255, 255, 255));
		s32 band = std::min<s32>(18, main_rect.getHeight() / 5);
		core::rect<s32> top_band(main_rect.UpperLeftCorner.X + 1, main_rect.UpperLeftCorner.Y + 1,
				main_rect.LowerRightCorner.X - 1, main_rect.UpperLeftCorner.Y + 1 + band);
		driver->draw2DRectangle(top_band, video::SColor(55, 255, 255, 255),
				video::SColor(55, 255, 255, 255), video::SColor(0, 255, 255, 255),
				video::SColor(0, 255, 255, 255), &AbsoluteClippingRect);
		core::rect<s32> bot_band(main_rect.UpperLeftCorner.X + 1, main_rect.LowerRightCorner.Y - 1 - band,
				main_rect.LowerRightCorner.X - 1, main_rect.LowerRightCorner.Y - 1);
		driver->draw2DRectangle(bot_band, video::SColor(0, 0, 0, 0),
				video::SColor(0, 0, 0, 0), video::SColor(60, 0, 0, 0),
				video::SColor(60, 0, 0, 0), &AbsoluteClippingRect);
		{
			s32 gw = std::min<s32>(main_rect.getWidth() / 3, 140);
			if (gw > 20) {
				core::rect<s32> clip = main_rect;
				clip.clipAgainst(AbsoluteClippingRect);
				s32 span = main_rect.getWidth() + gw;
				s32 p = (s32)(((u32)porting::getTimeMs() / 10) % (u32)span) - gw;
				s32 x0 = main_rect.UpperLeftCorner.X + p;
				s32 y0 = main_rect.UpperLeftCorner.Y + 1;
				driver->draw2DRectangle(core::rect<s32>(x0, y0, x0 + gw / 2, y0 + 2),
					video::SColor(0, 255, 240, 220), video::SColor(150, 255, 240, 220),
					video::SColor(0, 255, 240, 220), video::SColor(150, 255, 240, 220), &clip);
				driver->draw2DRectangle(core::rect<s32>(x0 + gw / 2, y0, x0 + gw, y0 + 2),
					video::SColor(150, 255, 240, 220), video::SColor(0, 255, 240, 220),
					video::SColor(150, 255, 240, 220), video::SColor(0, 255, 240, 220), &clip);
			}
		}
	}

	for (size_t i = 0; i <= 3; i++)
		driver->draw2DRectangle(m_bordercolors[i], border_rects[i],
				&AbsoluteClippingRect);

	IGUIElement::draw();
}
