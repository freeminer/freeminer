/*
Minetest
Copyright (C) 2010-2014 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "drawscene.h"
#include "log_types.h"
#include "minimap.h"
#include "settings.h"
#include "clouds.h"
#include "clientmap.h"
#include "util/timetaker.h"
#include "fontengine.h"
#include "guiscalingfilter.h"

#include "touchscreengui.h"

typedef enum {
	LEFT = -1,
	RIGHT = 1,
	EYECOUNT = 2
} paralax_sign;

void draw_anaglyph_3d_mode(Camera& camera, bool show_hud, Hud& hud,
		video::IVideoDriver* driver, scene::ISceneManager* smgr,
		bool draw_wield_tool, Client& client,
		gui::IGUIEnvironment* guienv )
{

	/* preserve old setup*/
	irr::core::vector3df oldPosition = camera.getCameraNode()->getPosition();
	irr::core::vector3df oldTarget   = camera.getCameraNode()->getTarget();

	irr::core::matrix4 startMatrix =
			camera.getCameraNode()->getAbsoluteTransformation();
	irr::core::vector3df focusPoint = (camera.getCameraNode()->getTarget()
			- camera.getCameraNode()->getAbsolutePosition()).setLength(1)
			+ camera.getCameraNode()->getAbsolutePosition();


	//Left eye...
	irr::core::vector3df leftEye;
	irr::core::matrix4 leftMove;
	leftMove.setTranslation(
			irr::core::vector3df(-g_settings->getFloat("3d_paralax_strength"),
					0.0f, 0.0f));
	leftEye = (startMatrix * leftMove).getTranslation();

	//clear the depth buffer, and color
	driver->beginScene( true, true, irr::video::SColor(0, 200, 200, 255));
	driver->getOverrideMaterial().Material.ColorMask = irr::video::ECP_RED;
	driver->getOverrideMaterial().EnableFlags = irr::video::EMF_COLOR_MASK;
	driver->getOverrideMaterial().EnablePasses = irr::scene::ESNRP_SKY_BOX
			+ irr::scene::ESNRP_SOLID + irr::scene::ESNRP_TRANSPARENT
			+ irr::scene::ESNRP_TRANSPARENT_EFFECT + irr::scene::ESNRP_SHADOW;
	camera.getCameraNode()->setPosition(leftEye);
	camera.getCameraNode()->setTarget(focusPoint);
	smgr->drawAll();
	driver->setTransform(video::ETS_WORLD, core::IdentityMatrix);
	if (show_hud) {
		hud.drawSelectionMesh();
		if (draw_wield_tool)
			camera.drawWieldedTool(&leftMove);
	}

	guienv->drawAll();

	//Right eye...
	irr::core::vector3df rightEye;
	irr::core::matrix4 rightMove;
	rightMove.setTranslation(
			irr::core::vector3df(g_settings->getFloat("3d_paralax_strength"),
					0.0f, 0.0f));
	rightEye = (startMatrix * rightMove).getTranslation();

	//clear the depth buffer
	driver->clearZBuffer();
	driver->getOverrideMaterial().Material.ColorMask = irr::video::ECP_GREEN
			+ irr::video::ECP_BLUE;
	driver->getOverrideMaterial().EnableFlags = irr::video::EMF_COLOR_MASK;
	driver->getOverrideMaterial().EnablePasses = irr::scene::ESNRP_SKY_BOX
			+ irr::scene::ESNRP_SOLID + irr::scene::ESNRP_TRANSPARENT
			+ irr::scene::ESNRP_TRANSPARENT_EFFECT + irr::scene::ESNRP_SHADOW;
	camera.getCameraNode()->setPosition(rightEye);
	camera.getCameraNode()->setTarget(focusPoint);
	smgr->drawAll();
	driver->setTransform(video::ETS_WORLD, core::IdentityMatrix);
	if (show_hud) {
		hud.drawSelectionMesh();
		if (draw_wield_tool)
			camera.drawWieldedTool(&rightMove);
	}

	guienv->drawAll();

	driver->getOverrideMaterial().Material.ColorMask = irr::video::ECP_ALL;
	driver->getOverrideMaterial().EnableFlags = 0;
	driver->getOverrideMaterial().EnablePasses = 0;
	camera.getCameraNode()->setPosition(oldPosition);
	camera.getCameraNode()->setTarget(oldTarget);
}

void init_texture(video::IVideoDriver* driver, const v2u32& screensize,
		video::ITexture** texture, const char* name)
{
	if (*texture != NULL)
	{
		driver->removeTexture(*texture);
	}
	*texture = driver->addRenderTargetTexture(
			core::dimension2d<u32>(screensize.X, screensize.Y), name,
			irr::video::ECF_A8R8G8B8);
}

video::ITexture* draw_image(const v2u32 &screensize,
		paralax_sign psign, const irr::core::matrix4 &startMatrix,
		const irr::core::vector3df &focusPoint, bool show_hud,
		video::IVideoDriver *driver, Camera &camera, scene::ISceneManager *smgr,
		Hud &hud, bool draw_wield_tool, Client &client,
		gui::IGUIEnvironment *guienv, const video::SColor &skycolor)
{
	static video::ITexture* images[2] = { NULL, NULL };
	static v2u32 last_screensize = v2u32(0, 0);

	video::ITexture* image = NULL;

	if (screensize != last_screensize) {
		init_texture(driver, screensize, &images[1], "mt_drawimage_img1");
		init_texture(driver, screensize, &images[0], "mt_drawimage_img2");
		last_screensize = screensize;
	}

	if (psign == RIGHT)
		image = images[1];
	else
		image = images[0];

	driver->setRenderTarget(image, true, true,
			irr::video::SColor(255,
					skycolor.getRed(), skycolor.getGreen(), skycolor.getBlue()));

	irr::core::vector3df eye_pos;
	irr::core::matrix4 movement;
	movement.setTranslation(
			irr::core::vector3df((int) psign *
					g_settings->getFloat("3d_paralax_strength"), 0.0f, 0.0f));
	eye_pos = (startMatrix * movement).getTranslation();

	//clear the depth buffer
	driver->clearZBuffer();
	camera.getCameraNode()->setPosition(eye_pos);
	camera.getCameraNode()->setTarget(focusPoint);
	smgr->drawAll();

	driver->setTransform(video::ETS_WORLD, core::IdentityMatrix);

	if (show_hud) {
		hud.drawSelectionMesh();
		if (draw_wield_tool)
			camera.drawWieldedTool(&movement);
	}

	guienv->drawAll();

	/* switch back to real renderer */
	driver->setRenderTarget(0, true, true,
			irr::video::SColor(0,
					skycolor.getRed(), skycolor.getGreen(), skycolor.getBlue()));

	return image;
}

video::ITexture*  draw_hud(video::IVideoDriver* driver, const v2u32& screensize,
		bool show_hud, Hud& hud, Client& client, bool draw_crosshair,
		video::SColor skycolor, gui::IGUIEnvironment* guienv, Camera& camera )
{
	static video::ITexture* image = NULL;
	init_texture(driver, screensize, &image, "mt_drawimage_hud");
	driver->setRenderTarget(image, true, true,
			irr::video::SColor(255,0,0,0));

	if (show_hud)
	{
		if (draw_crosshair)
			hud.drawCrosshair();
		hud.drawHotbar(client.getPlayerItem());
		hud.drawLuaElements(camera.getOffset());
		camera.drawNametags();
		guienv->drawAll();
	}

	driver->setRenderTarget(0, true, true,
			irr::video::SColor(0,
					skycolor.getRed(), skycolor.getGreen(), skycolor.getBlue()));

	return image;
}

void draw_interlaced_3d_mode(Camera& camera, bool show_hud,
		Hud& hud, video::IVideoDriver* driver,
		scene::ISceneManager* smgr, const v2u32& screensize,
		bool draw_wield_tool, Client& client, gui::IGUIEnvironment* guienv,
		video::SColor skycolor )
{
	/* save current info */
	irr::core::vector3df oldPosition = camera.getCameraNode()->getPosition();
	irr::core::vector3df oldTarget = camera.getCameraNode()->getTarget();
	irr::core::matrix4 startMatrix =
			camera.getCameraNode()->getAbsoluteTransformation();
	irr::core::vector3df focusPoint = (camera.getCameraNode()->getTarget()
			- camera.getCameraNode()->getAbsolutePosition()).setLength(1)
			+ camera.getCameraNode()->getAbsolutePosition();

	/* create left view */
	video::ITexture* left_image = draw_image(screensize, LEFT, startMatrix,
			focusPoint, show_hud, driver, camera, smgr, hud,
			draw_wield_tool, client, guienv, skycolor);

	//Right eye...
	irr::core::vector3df rightEye;
	irr::core::matrix4 rightMove;
	rightMove.setTranslation(
			irr::core::vector3df(g_settings->getFloat("3d_paralax_strength"),
					0.0f, 0.0f));
	rightEye = (startMatrix * rightMove).getTranslation();

	//clear the depth buffer
	driver->clearZBuffer();
	camera.getCameraNode()->setPosition(rightEye);
	camera.getCameraNode()->setTarget(focusPoint);
	smgr->drawAll();

	driver->setTransform(video::ETS_WORLD, core::IdentityMatrix);

	if (show_hud) {
		hud.drawSelectionMesh();
		if(draw_wield_tool)
			camera.drawWieldedTool(&rightMove);
	}
	guienv->drawAll();

	for (unsigned int i = 0; i < screensize.Y; i+=2 ) {
#if (IRRLICHT_VERSION_MAJOR >= 1) && (IRRLICHT_VERSION_MINOR >= 8)
		driver->draw2DImage(left_image, irr::core::position2d<s32>(0, i),
#else
		driver->draw2DImage(left_image, irr::core::position2d<s32>(0, screensize.Y-i),
#endif
				irr::core::rect<s32>(0, i,screensize.X, i+1), 0,
				irr::video::SColor(255, 255, 255, 255),
				false);
	}

	/* cleanup */
	camera.getCameraNode()->setPosition(oldPosition);
	camera.getCameraNode()->setTarget(oldTarget);
}

void draw_sidebyside_3d_mode(Camera& camera, bool show_hud,
		Hud& hud, video::IVideoDriver* driver,
		scene::ISceneManager* smgr, const v2u32& screensize,
		bool draw_wield_tool, Client& client, gui::IGUIEnvironment* guienv,
		video::SColor skycolor )
{
	/* save current info */
	irr::core::vector3df oldPosition = camera.getCameraNode()->getPosition();
	irr::core::vector3df oldTarget = camera.getCameraNode()->getTarget();
	irr::core::matrix4 startMatrix =
			camera.getCameraNode()->getAbsoluteTransformation();
	irr::core::vector3df focusPoint = (camera.getCameraNode()->getTarget()
			- camera.getCameraNode()->getAbsolutePosition()).setLength(1)
			+ camera.getCameraNode()->getAbsolutePosition();

	/* create left view */
	video::ITexture* left_image = draw_image(screensize, LEFT, startMatrix,
			focusPoint, show_hud, driver, camera, smgr, hud,
			draw_wield_tool, client, guienv, skycolor);

	/* create right view */
	video::ITexture* right_image = draw_image(screensize, RIGHT, startMatrix,
			focusPoint, show_hud, driver, camera, smgr, hud,
			draw_wield_tool, client, guienv, skycolor);

	/* create hud overlay */
	video::ITexture* hudtexture = draw_hud(driver, screensize, show_hud, hud, client,
			false, skycolor, guienv, camera );
	driver->makeColorKeyTexture(hudtexture, irr::video::SColor(255, 0, 0, 0));
	//makeColorKeyTexture mirrors texture so we do it twice to get it right again
	driver->makeColorKeyTexture(hudtexture, irr::video::SColor(255, 0, 0, 0));

	draw2DImageFilterScaled(driver, left_image,
			irr::core::rect<s32>(0, 0, screensize.X/2, screensize.Y),
			irr::core::rect<s32>(0, 0, screensize.X, screensize.Y), 0, 0, false);

	draw2DImageFilterScaled(driver, hudtexture,
			irr::core::rect<s32>(0, 0, screensize.X/2, screensize.Y),
			irr::core::rect<s32>(0, 0, screensize.X, screensize.Y), 0, 0, true);

	draw2DImageFilterScaled(driver, right_image,
			irr::core::rect<s32>(screensize.X/2, 0, screensize.X, screensize.Y),
			irr::core::rect<s32>(0, 0, screensize.X, screensize.Y), 0, 0, false);

	draw2DImageFilterScaled(driver, hudtexture,
			irr::core::rect<s32>(screensize.X/2, 0, screensize.X, screensize.Y),
			irr::core::rect<s32>(0, 0, screensize.X, screensize.Y), 0, 0, true);

	left_image = NULL;
	right_image = NULL;

	/* cleanup */
	camera.getCameraNode()->setPosition(oldPosition);
	camera.getCameraNode()->setTarget(oldTarget);
}

void draw_top_bottom_3d_mode(Camera& camera, bool show_hud,
		Hud& hud, video::IVideoDriver* driver,
		scene::ISceneManager* smgr, const v2u32& screensize,
		bool draw_wield_tool, Client& client, gui::IGUIEnvironment* guienv,
		video::SColor skycolor )
{
	/* save current info */
	irr::core::vector3df oldPosition = camera.getCameraNode()->getPosition();
	irr::core::vector3df oldTarget = camera.getCameraNode()->getTarget();
	irr::core::matrix4 startMatrix =
			camera.getCameraNode()->getAbsoluteTransformation();
	irr::core::vector3df focusPoint = (camera.getCameraNode()->getTarget()
			- camera.getCameraNode()->getAbsolutePosition()).setLength(1)
			+ camera.getCameraNode()->getAbsolutePosition();

	/* create left view */
	video::ITexture* left_image = draw_image(screensize, LEFT, startMatrix,
			focusPoint, show_hud, driver, camera, smgr, hud,
			draw_wield_tool, client, guienv, skycolor);

	/* create right view */
	video::ITexture* right_image = draw_image(screensize, RIGHT, startMatrix,
			focusPoint, show_hud, driver, camera, smgr, hud,
			draw_wield_tool, client, guienv, skycolor);

	/* create hud overlay */
	video::ITexture* hudtexture = draw_hud(driver, screensize, show_hud, hud, client,
			false, skycolor, guienv, camera );
	driver->makeColorKeyTexture(hudtexture, irr::video::SColor(255, 0, 0, 0));
	//makeColorKeyTexture mirrors texture so we do it twice to get it right again
	driver->makeColorKeyTexture(hudtexture, irr::video::SColor(255, 0, 0, 0));

	draw2DImageFilterScaled(driver, left_image,
			irr::core::rect<s32>(0, 0, screensize.X, screensize.Y/2),
			irr::core::rect<s32>(0, 0, screensize.X, screensize.Y), 0, 0, false);

	draw2DImageFilterScaled(driver, hudtexture,
			irr::core::rect<s32>(0, 0, screensize.X, screensize.Y/2),
			irr::core::rect<s32>(0, 0, screensize.X, screensize.Y), 0, 0, true);

	draw2DImageFilterScaled(driver, right_image,
			irr::core::rect<s32>(0, screensize.Y/2, screensize.X, screensize.Y),
			irr::core::rect<s32>(0, 0, screensize.X, screensize.Y), 0, 0, false);

	draw2DImageFilterScaled(driver, hudtexture,
			irr::core::rect<s32>(0, screensize.Y/2, screensize.X, screensize.Y),
			irr::core::rect<s32>(0, 0, screensize.X, screensize.Y), 0, 0, true);

	left_image = NULL;
	right_image = NULL;

	/* cleanup */
	camera.getCameraNode()->setPosition(oldPosition);
	camera.getCameraNode()->setTarget(oldTarget);
}

void draw_pageflip_3d_mode(Camera& camera, bool show_hud,
		Hud& hud, video::IVideoDriver* driver,
		scene::ISceneManager* smgr, const v2u32& screensize,
		bool draw_wield_tool, Client& client, gui::IGUIEnvironment* guienv,
		video::SColor skycolor)
{
	/* preserve old setup*/
	irr::core::vector3df oldPosition = camera.getCameraNode()->getPosition();
	irr::core::vector3df oldTarget   = camera.getCameraNode()->getTarget();

	irr::core::matrix4 startMatrix =
			camera.getCameraNode()->getAbsoluteTransformation();
	irr::core::vector3df focusPoint = (camera.getCameraNode()->getTarget()
			- camera.getCameraNode()->getAbsolutePosition()).setLength(1)
			+ camera.getCameraNode()->getAbsolutePosition();

	//Left eye...
// fmTODO: fixme:
#if IRRLICHT_VERSION_10000 < 10900
	driver->setRenderTarget(irr::video::ERT_STEREO_LEFT_BUFFER);
#endif

	irr::core::vector3df leftEye;
	irr::core::matrix4 leftMove;
	leftMove.setTranslation(
			irr::core::vector3df(-g_settings->getFloat("3d_paralax_strength"),
					0.0f, 0.0f));
	leftEye = (startMatrix * leftMove).getTranslation();

	//clear the depth buffer, and color
	driver->beginScene(true, true, irr::video::SColor(200, 200, 200, 255));
	camera.getCameraNode()->setPosition(leftEye);
	camera.getCameraNode()->setTarget(focusPoint);
	smgr->drawAll();
	driver->setTransform(video::ETS_WORLD, core::IdentityMatrix);

	if (show_hud) {
		hud.drawSelectionMesh();
		if (draw_wield_tool)
			camera.drawWieldedTool(&leftMove);
		hud.drawHotbar(client.getPlayerItem());
		hud.drawLuaElements(camera.getOffset());
		camera.drawNametags();
	}

	guienv->drawAll();

	//Right eye...
// fmTODO: fixme:
#if IRRLICHT_VERSION_10000 < 10900
	driver->setRenderTarget(irr::video::ERT_STEREO_RIGHT_BUFFER);
#endif

	irr::core::vector3df rightEye;
	irr::core::matrix4 rightMove;
	rightMove.setTranslation(
			irr::core::vector3df(g_settings->getFloat("3d_paralax_strength"),
					0.0f, 0.0f));
	rightEye = (startMatrix * rightMove).getTranslation();

	//clear the depth buffer, and color
	driver->beginScene(true, true, irr::video::SColor(200, 200, 200, 255));
	camera.getCameraNode()->setPosition(rightEye);
	camera.getCameraNode()->setTarget(focusPoint);
	smgr->drawAll();
	driver->setTransform(video::ETS_WORLD, core::IdentityMatrix);

	if (show_hud) {
		hud.drawSelectionMesh();
		if (draw_wield_tool)
			camera.drawWieldedTool(&rightMove);
		hud.drawHotbar(client.getPlayerItem());
		hud.drawLuaElements(camera.getOffset());
		camera.drawNametags();
	}

	guienv->drawAll();

	camera.getCameraNode()->setPosition(oldPosition);
	camera.getCameraNode()->setTarget(oldTarget);
}

void draw_plain(Camera &camera, bool show_hud, Hud &hud,
		video::IVideoDriver *driver, bool draw_wield_tool,
		Client &client, gui::IGUIEnvironment *guienv)
{
	driver->setTransform(video::ETS_WORLD, core::IdentityMatrix);

/*
#if !defined(NDEBUG)
	client.getEnv().getClientMap().renderBlockBoundaries(client.getEnv().getClientMap().m_block_boundary);
#endif
*/

	if (show_hud) {
		hud.drawSelectionMesh();
		if (draw_wield_tool) {
			camera.drawWieldedTool();
		}
	}
}

void draw_scene(video::IVideoDriver *driver, scene::ISceneManager *smgr,
		Camera &camera, Client& client, LocalPlayer *player, Hud &hud,
		Mapper &mapper, gui::IGUIEnvironment *guienv,
		const v2u32 &screensize, const video::SColor &skycolor,
		bool show_hud, bool show_minimap)
{
	TimeTaker timer("smgr");

	bool draw_wield_tool = (show_hud &&
			(player->hud_flags & HUD_FLAG_WIELDITEM_VISIBLE) &&
			camera.getCameraMode() < CAMERA_MODE_THIRD );

	bool draw_crosshair = ((player->hud_flags & HUD_FLAG_CROSSHAIR_VISIBLE) &&
			(camera.getCameraMode() != CAMERA_MODE_THIRD_FRONT));

#ifdef HAVE_TOUCHSCREENGUI
	if (g_touchscreengui) {
	static const auto touchtarget = g_settings->getBool("touchtarget");
	draw_crosshair = !touchtarget;
	}
#endif

	std::string draw_mode = g_settings->get("3d_mode");

	smgr->drawAll();

	if (draw_mode == "anaglyph")
	{
		draw_anaglyph_3d_mode(camera, show_hud, hud, driver,
				smgr, draw_wield_tool, client, guienv);
		draw_crosshair = false;
	}
	else if (draw_mode == "interlaced")
	{
		draw_interlaced_3d_mode(camera, show_hud, hud, driver,
				smgr, screensize, draw_wield_tool, client, guienv, skycolor);
		draw_crosshair = false;
	}
	else if (draw_mode == "sidebyside")
	{
		draw_sidebyside_3d_mode(camera, show_hud, hud, driver,
				smgr, screensize, draw_wield_tool, client, guienv, skycolor);
		show_hud = false;
	}
	else if (draw_mode == "topbottom")
	{
		draw_top_bottom_3d_mode(camera, show_hud, hud, driver,
				smgr, screensize, draw_wield_tool, client, guienv, skycolor);
		show_hud = false;
	}
	else if (draw_mode == "pageflip")
	{
		draw_pageflip_3d_mode(camera, show_hud, hud, driver,
				smgr, screensize, draw_wield_tool, client, guienv, skycolor);
		draw_crosshair = false;
		show_hud = false;
	}
	else {
		draw_plain(camera, show_hud, hud, driver,
				draw_wield_tool, client, guienv);
	}

	/*
		Post effects
	*/
	{
		client.getEnv().getClientMap().renderPostFx(camera.getCameraMode());
	}

	//TODO how to make those 3d too
	if (show_hud)
	{
		if (draw_crosshair)
			hud.drawCrosshair();

		hud.drawHotbar(client.getPlayerItem());
		hud.drawLuaElements(camera.getOffset());
		camera.drawNametags();

		if (show_minimap)
			mapper.drawMinimap();
	}

	guienv->drawAll();

	timer.stop(true);
}

static std::wstring text_old;
/*
	Draws a screen with a single text on it.
	Text will be removed when the screen is drawn the next time.
	Additionally, a progressbar can be drawn when percent is set between 0 and 100.
*/
void draw_load_screen(const std::wstring &text, IrrlichtDevice* device,
		gui::IGUIEnvironment* guienv, float dtime, int percent, bool clouds )
{
	video::IVideoDriver* driver    = device->getVideoDriver();

	if (device->getVideoDriver()->getDriverType() == video::EDT_NULL) {
		if (text != text_old) {
			actionstream<<" ... "<< text << std::endl;
			text_old = text;
		}
		return;
	}

	v2u32 screensize               = porting::getWindowSize();

	v2s32 textsize(g_fontengine->getTextWidth(text), g_fontengine->getLineHeight());
	v2s32 center(screensize.X / 2, screensize.Y / 2);
	core::rect<s32> textrect(center - textsize / 2, center + textsize / 2);

	gui::IGUIStaticText *guitext = guienv->addStaticText(
			text.c_str(), textrect, false, false);
	guitext->setTextAlignment(gui::EGUIA_CENTER, gui::EGUIA_UPPERLEFT);

	bool cloud_menu_background = clouds && g_settings->getBool("menu_clouds");
	if (cloud_menu_background)
	{
		g_menuclouds->step(dtime*3);
		g_menuclouds->render();
		driver->beginScene(true, true, video::SColor(255, 140, 186, 250));
		g_menucloudsmgr->drawAll();
	}
	else
		driver->beginScene(true, true, video::SColor(255, 0, 0, 0));

	// draw progress bar
	if ((percent >= 0) && (percent <= 100))
	{
		v2s32 barsize(
				// 342 is (approximately) 256/0.75 to keep bar on same size as
				// before with default settings
				342 * porting::getDisplayDensity() *
				g_settings->getFloat("gui_scaling"),
				g_fontengine->getTextHeight() * 2);

		core::rect<s32> barrect(center - barsize / 2, center + barsize / 2);
		driver->draw2DRectangle(video::SColor(255, 255, 255, 255),barrect, NULL); // border
		driver->draw2DRectangle(video::SColor(255, 64, 64, 64), core::rect<s32> (
				barrect.UpperLeftCorner + 1,
				barrect.LowerRightCorner-1), NULL); // black inside the bar
		driver->draw2DRectangle(video::SColor(255, 128, 128, 128), core::rect<s32> (
				barrect.UpperLeftCorner + 1,
				core::vector2d<s32>(
						barrect.LowerRightCorner.X -
						(barsize.X - 1) + percent * (barsize.X - 2) / 100,
						barrect.LowerRightCorner.Y - 1)), NULL); // the actual progress
	}
	guienv->drawAll();
	driver->endScene();

	guitext->remove();

	//return guitext;
}
