/*
content_cao.cpp
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
*/

/*
This file is part of Freeminer.

Freeminer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Freeminer  is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Freeminer.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "content_cao.h"
#include "tile.h"
#include "environment.h"
#include "collision.h"
#include "settings.h"
#include <ICameraSceneNode.h>
#include <ITextSceneNode.h>
#include <IBillboardSceneNode.h>
#include "serialization.h" // For decompressZlib
#include "gamedef.h"
#include "clientobject.h"
#include "content_object.h"
#include "mesh.h"
#include "itemdef.h"
#include "tool.h"
#include "content_cso.h"
#include "sound.h"
#include "nodedef.h"
#include "localplayer.h"
#include "util/numeric.h" // For IntervalLimiter
#include "util/serialize.h"
#include "util/mathconstants.h"
#include "map.h"
#include "main.h" // g_settings
#include "camera.h" // CameraModes
#include <IMeshManipulator.h>
#include <IAnimatedMeshSceneNode.h>
#include <IBoneSceneNode.h>

class Settings;
struct ToolCapabilities;

#define PP(x) "("<<(x).X<<","<<(x).Y<<","<<(x).Z<<")"

std::map<u16, ClientActiveObject::Factory> ClientActiveObject::m_types;

/*
	SmoothTranslator
*/

struct SmoothTranslator
{
	v3f vect_old;
	v3f vect_show;
	v3f vect_aim;
	f32 anim_counter;
	f32 anim_time;
	f32 anim_time_counter;
	bool aim_is_end;

	SmoothTranslator():
		vect_old(0,0,0),
		vect_show(0,0,0),
		vect_aim(0,0,0),
		anim_counter(0),
		anim_time(0),
		anim_time_counter(0),
		aim_is_end(true)
	{}

	void init(v3f vect)
	{
		vect_old = vect;
		vect_show = vect;
		vect_aim = vect;
		anim_counter = 0;
		anim_time = 0;
		anim_time_counter = 0;
		aim_is_end = true;
	}

	void sharpen()
	{
		init(vect_show);
	}

	void update(v3f vect_new, bool is_end_position=false, float update_interval=-1)
	{
		aim_is_end = is_end_position;
		vect_old = vect_show;
		vect_aim = vect_new;
		if(update_interval > 0){
			anim_time = update_interval;
		} else {
			if(anim_time < 0.001 || anim_time > 1.0)
				anim_time = anim_time_counter;
			else
				anim_time = anim_time * 0.9 + anim_time_counter * 0.1;
		}
		anim_time_counter = 0;
		anim_counter = 0;
	}

	void translate(f32 dtime)
	{
		anim_time_counter = anim_time_counter + dtime;
		anim_counter = anim_counter + dtime;
		v3f vect_move = vect_aim - vect_old;
		f32 moveratio = 1.0;
		if(anim_time > 0.001)
			moveratio = anim_time_counter / anim_time;
		// Move a bit less than should, to avoid oscillation
		moveratio = moveratio * 0.8;
		float move_end = 1.5;
		if(aim_is_end)
			move_end = 1.0;
		if(moveratio > move_end)
			moveratio = move_end;
		vect_show = vect_old + vect_move * moveratio;
	}

	bool is_moving()
	{
		return ((anim_time_counter / anim_time) < 1.4);
	}
};

/*
	Other stuff
*/

static void setBillboardTextureMatrix(scene::IBillboardSceneNode *bill,
		float txs, float tys, int col, int row)
{
	video::SMaterial& material = bill->getMaterial(0);
	core::matrix4& matrix = material.getTextureMatrix(0);
	matrix.setTextureTranslate(txs*col, tys*row);
	matrix.setTextureScale(txs, tys);
}

/*
	TestCAO
*/

class TestCAO : public ClientActiveObject
{
public:
	TestCAO(IGameDef *gamedef, ClientEnvironment *env);
	virtual ~TestCAO();
	
	u8 getType() const
	{
		return ACTIVEOBJECT_TYPE_TEST;
	}
	
	static ClientActiveObject* create(IGameDef *gamedef, ClientEnvironment *env);

	void addToScene(scene::ISceneManager *smgr, ITextureSource *tsrc,
			IrrlichtDevice *irr);
	void removeFromScene(bool permanent);
	void updateLight(u8 light_at_pos);
	v3s16 getLightPosition();
	void updateNodePos();

	void step(float dtime, ClientEnvironment *env);

	void processMessage(const std::string &data);

	bool getCollisionBox(aabb3f *toset) { return false; }
private:
	scene::IMeshSceneNode *m_node;
	v3f m_position;
};

// Prototype
TestCAO proto_TestCAO(NULL, NULL);

TestCAO::TestCAO(IGameDef *gamedef, ClientEnvironment *env):
	ClientActiveObject(0, gamedef, env),
	m_node(NULL),
	m_position(v3f(0,10*BS,0))
{
	ClientActiveObject::registerType(getType(), create);
}

TestCAO::~TestCAO()
{
}

ClientActiveObject* TestCAO::create(IGameDef *gamedef, ClientEnvironment *env)
{
	return new TestCAO(gamedef, env);
}

void TestCAO::addToScene(scene::ISceneManager *smgr, ITextureSource *tsrc,
			IrrlichtDevice *irr)
{
	if(m_node != NULL)
		return;
	
	//video::IVideoDriver* driver = smgr->getVideoDriver();
	
	scene::SMesh *mesh = new scene::SMesh();
	scene::IMeshBuffer *buf = new scene::SMeshBuffer();
	video::SColor c(255,255,255,255);
	video::S3DVertex vertices[4] =
	{
		video::S3DVertex(-BS/2,-BS/4,0, 0,0,0, c, 0,1),
		video::S3DVertex(BS/2,-BS/4,0, 0,0,0, c, 1,1),
		video::S3DVertex(BS/2,BS/4,0, 0,0,0, c, 1,0),
		video::S3DVertex(-BS/2,BS/4,0, 0,0,0, c, 0,0),
	};
	u16 indices[] = {0,1,2,2,3,0};
	buf->append(vertices, 4, indices, 6);
	// Set material
	buf->getMaterial().setFlag(video::EMF_LIGHTING, false);
	buf->getMaterial().setFlag(video::EMF_BACK_FACE_CULLING, false);
	buf->getMaterial().setTexture(0, tsrc->getTexture("rat.png"));
	buf->getMaterial().setFlag(video::EMF_BILINEAR_FILTER, false);
	buf->getMaterial().setFlag(video::EMF_FOG_ENABLE, true);
	buf->getMaterial().MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;
	// Add to mesh
	mesh->addMeshBuffer(buf);
	buf->drop();
	m_node = smgr->addMeshSceneNode(mesh, NULL);
	mesh->drop();
	updateNodePos();
}

void TestCAO::removeFromScene(bool permanent)
{
	if(m_node == NULL)
		return;

	m_node->remove();
	m_node = NULL;
}

void TestCAO::updateLight(u8 light_at_pos)
{
}

v3s16 TestCAO::getLightPosition()
{
	return floatToInt(m_position, BS);
}

void TestCAO::updateNodePos()
{
	if(m_node == NULL)
		return;

	m_node->setPosition(m_position);
	//m_node->setRotation(v3f(0, 45, 0));
}

void TestCAO::step(float dtime, ClientEnvironment *env)
{
	if(m_node)
	{
		v3f rot = m_node->getRotation();
		//infostream<<"dtime="<<dtime<<", rot.Y="<<rot.Y<<std::endl;
		rot.Y += dtime * 180;
		m_node->setRotation(rot);
	}
}

void TestCAO::processMessage(const std::string &data)
{
	infostream<<"TestCAO: Got data: "<<data<<std::endl;
	std::istringstream is(data, std::ios::binary);
	u16 cmd;
	is>>cmd;
	if(cmd == 0)
	{
		v3f newpos;
		is>>newpos.X;
		is>>newpos.Y;
		is>>newpos.Z;
		m_position = newpos;
		updateNodePos();
	}
}

/*
	ItemCAO
*/

class ItemCAO : public ClientActiveObject
{
public:
	ItemCAO(IGameDef *gamedef, ClientEnvironment *env);
	virtual ~ItemCAO();
	
	u8 getType() const
	{
		return ACTIVEOBJECT_TYPE_ITEM;
	}
	
	static ClientActiveObject* create(IGameDef *gamedef, ClientEnvironment *env);

	void addToScene(scene::ISceneManager *smgr, ITextureSource *tsrc,
			IrrlichtDevice *irr);
	void removeFromScene(bool permanent);
	void updateLight(u8 light_at_pos);
	v3s16 getLightPosition();
	void updateNodePos();
	void updateInfoText();
	void updateTexture();

	void step(float dtime, ClientEnvironment *env);

	void processMessage(const std::string &data);

	void initialize(const std::string &data);
	
	core::aabbox3d<f32>* getSelectionBox()
		{return &m_selection_box;}
	v3f getPosition()
		{return m_position;}
	
	std::string infoText()
		{return m_infotext;}

	bool getCollisionBox(aabb3f *toset) { return false; }
private:
	core::aabbox3d<f32> m_selection_box;
	scene::IMeshSceneNode *m_node;
	v3f m_position;
	std::string m_itemstring;
	std::string m_infotext;
};

#include "inventory.h"

// Prototype
ItemCAO proto_ItemCAO(NULL, NULL);

ItemCAO::ItemCAO(IGameDef *gamedef, ClientEnvironment *env):
	ClientActiveObject(0, gamedef, env),
	m_selection_box(-BS/3.,0.0,-BS/3., BS/3.,BS*2./3.,BS/3.),
	m_node(NULL),
	m_position(v3f(0,10*BS,0))
{
	if(!gamedef && !env)
	{
		ClientActiveObject::registerType(getType(), create);
	}
}

ItemCAO::~ItemCAO()
{
}

ClientActiveObject* ItemCAO::create(IGameDef *gamedef, ClientEnvironment *env)
{
	return new ItemCAO(gamedef, env);
}

void ItemCAO::addToScene(scene::ISceneManager *smgr, ITextureSource *tsrc,
			IrrlichtDevice *irr)
{
	if(m_node != NULL)
		return;
	
	//video::IVideoDriver* driver = smgr->getVideoDriver();
	
	scene::SMesh *mesh = new scene::SMesh();
	scene::IMeshBuffer *buf = new scene::SMeshBuffer();
	video::SColor c(255,255,255,255);
	video::S3DVertex vertices[4] =
	{
		/*video::S3DVertex(-BS/2,-BS/4,0, 0,0,0, c, 0,1),
		video::S3DVertex(BS/2,-BS/4,0, 0,0,0, c, 1,1),
		video::S3DVertex(BS/2,BS/4,0, 0,0,0, c, 1,0),
		video::S3DVertex(-BS/2,BS/4,0, 0,0,0, c, 0,0),*/
		video::S3DVertex(BS/3.,0,0, 0,0,0, c, 0,1),
		video::S3DVertex(-BS/3.,0,0, 0,0,0, c, 1,1),
		video::S3DVertex(-BS/3.,0+BS*2./3.,0, 0,0,0, c, 1,0),
		video::S3DVertex(BS/3.,0+BS*2./3.,0, 0,0,0, c, 0,0),
	};
	u16 indices[] = {0,1,2,2,3,0};
	buf->append(vertices, 4, indices, 6);
	// Set material
	buf->getMaterial().setFlag(video::EMF_LIGHTING, false);
	buf->getMaterial().setFlag(video::EMF_BACK_FACE_CULLING, false);
	// Initialize with a generated placeholder texture
	buf->getMaterial().setTexture(0, tsrc->getTexture(""));
	buf->getMaterial().setFlag(video::EMF_BILINEAR_FILTER, false);
	buf->getMaterial().setFlag(video::EMF_FOG_ENABLE, true);
	buf->getMaterial().MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;
	// Add to mesh
	mesh->addMeshBuffer(buf);
	buf->drop();
	m_node = smgr->addMeshSceneNode(mesh, NULL);
	mesh->drop();
	updateNodePos();

	/*
		Update image of node
	*/

	updateTexture();
}

void ItemCAO::removeFromScene(bool permanent)
{
	if(m_node == NULL)
		return;

	m_node->remove();
	m_node = NULL;
}

void ItemCAO::updateLight(u8 light_at_pos)
{
	if(m_node == NULL)
		return;

	u8 li = decode_light(light_at_pos);
	video::SColor color(255,li,li,li);
	setMeshColor(m_node->getMesh(), color);
}

v3s16 ItemCAO::getLightPosition()
{
	return floatToInt(m_position + v3f(0,0.5*BS,0), BS);
}

void ItemCAO::updateNodePos()
{
	if(m_node == NULL)
		return;

	m_node->setPosition(m_position);
}

void ItemCAO::updateInfoText()
{
	try{
		IItemDefManager *idef = m_gamedef->idef();
		ItemStack item;
		item.deSerialize(m_itemstring, idef);
		if(item.isKnown(idef))
			m_infotext = item.getDefinition(idef).description;
		else
			m_infotext = "Unknown item: '" + m_itemstring + "'";
		if(item.count >= 2)
			m_infotext += " (" + itos(item.count) + ")";
	}
	catch(SerializationError &e)
	{
		m_infotext = "Unknown item: '" + m_itemstring + "'";
	}
}

void ItemCAO::updateTexture()
{
	if(m_node == NULL)
		return;

	// Create an inventory item to see what is its image
	std::istringstream is(m_itemstring, std::ios_base::binary);
	video::ITexture *texture = NULL;
	try{
		IItemDefManager *idef = m_gamedef->idef();
		ItemStack item;
		item.deSerialize(is, idef);
		texture = idef->getInventoryTexture(item.getDefinition(idef).name, m_gamedef);
	}
	catch(SerializationError &e)
	{
		infostream<<"WARNING: "<<__FUNCTION_NAME
				<<": error deSerializing itemstring \""
				<<m_itemstring<<std::endl;
	}
	
	// Set meshbuffer texture
	m_node->getMaterial(0).setTexture(0, texture);
}


void ItemCAO::step(float dtime, ClientEnvironment *env)
{
	if(m_node)
	{
		/*v3f rot = m_node->getRotation();
		rot.Y += dtime * 120;
		m_node->setRotation(rot);*/
		LocalPlayer *player = env->getLocalPlayer();
		assert(player);
		v3f rot = m_node->getRotation();
		rot.Y = 180.0 - (player->getYaw());
		m_node->setRotation(rot);
	}
}

void ItemCAO::processMessage(const std::string &data)
{
	//infostream<<"ItemCAO: Got message"<<std::endl;
	std::istringstream is(data, std::ios::binary);
	// command
	u8 cmd = readU8(is);
	if(cmd == 0)
	{
		// pos
		m_position = readV3F1000(is);
		updateNodePos();
	}
	if(cmd == 1)
	{
		// itemstring
		m_itemstring = deSerializeString(is);
		updateInfoText();
		updateTexture();
	}
}

void ItemCAO::initialize(const std::string &data)
{
	infostream<<"ItemCAO: Got init data"<<std::endl;
	
	{
		std::istringstream is(data, std::ios::binary);
		// version
		u8 version = readU8(is);
		// check version
		if(version != 0)
			return;
		// pos
		m_position = readV3F1000(is);
		// itemstring
		m_itemstring = deSerializeString(is);
	}
	
	updateNodePos();
	updateInfoText();
}

/*
	GenericCAO
*/

#include "genericobject.h"

class GenericCAO : public ClientActiveObject
{
private:
	// Only set at initialization
	std::string m_name;
	bool m_is_player;
	bool m_is_local_player;
	int m_id;
	// Property-ish things
	ObjectProperties m_prop;
	//
	scene::ISceneManager *m_smgr;
	IrrlichtDevice *m_irr;
	core::aabbox3d<f32> m_selection_box;
	scene::IMeshSceneNode *m_meshnode;
	scene::IAnimatedMeshSceneNode *m_animated_meshnode;
	scene::IBillboardSceneNode *m_spritenode;
	scene::ITextSceneNode* m_textnode;
	v3f m_position;
	v3f m_velocity;
	v3f m_acceleration;
	float m_yaw;
	s16 m_hp;
	SmoothTranslator pos_translator;
	// Spritesheet/animation stuff
	v2f m_tx_size;
	v2s16 m_tx_basepos;
	bool m_initial_tx_basepos_set;
	bool m_tx_select_horiz_by_yawpitch;
	v2s32 m_animation_range;
	int m_animation_speed;
	int m_animation_blend;
	std::map<std::string, core::vector2d<v3f> > m_bone_position; // stores position and rotation for each bone name
	std::string m_attachment_bone;
	v3f m_attachment_position;
	v3f m_attachment_rotation;
	bool m_attached_to_local;
	int m_anim_frame;
	int m_anim_num_frames;
	float m_anim_framelength;
	float m_anim_timer;
	ItemGroupList m_armor_groups;
	float m_reset_textures_timer;
	bool m_visuals_expired;
	float m_step_distance_counter;
	u8 m_last_light;
	bool m_is_visible;

public:
	GenericCAO(IGameDef *gamedef, ClientEnvironment *env):
		ClientActiveObject(0, gamedef, env),
		//
		m_is_player(false),
		m_is_local_player(false),
		m_id(0),
		//
		m_smgr(NULL),
		m_irr(NULL),
		m_selection_box(-BS/3.,-BS/3.,-BS/3., BS/3.,BS/3.,BS/3.),
		m_meshnode(NULL),
		m_animated_meshnode(NULL),
		m_spritenode(NULL),
		m_textnode(NULL),
		m_position(v3f(0,10*BS,0)),
		m_velocity(v3f(0,0,0)),
		m_acceleration(v3f(0,0,0)),
		m_yaw(0),
		m_hp(1),
		m_tx_size(1,1),
		m_tx_basepos(0,0),
		m_initial_tx_basepos_set(false),
		m_tx_select_horiz_by_yawpitch(false),
		m_animation_range(v2s32(0,0)),
		m_animation_speed(15),
		m_animation_blend(0),
		m_bone_position(std::map<std::string, core::vector2d<v3f> >()),
		m_attachment_bone(""),
		m_attachment_position(v3f(0,0,0)),
		m_attachment_rotation(v3f(0,0,0)),
		m_attached_to_local(false),
		m_anim_frame(0),
		m_anim_num_frames(1),
		m_anim_framelength(0.2),
		m_anim_timer(0),
		m_reset_textures_timer(-1),
		m_visuals_expired(false),
		m_step_distance_counter(0),
		m_last_light(255),
		m_is_visible(false)
	{
		if(gamedef == NULL)
			ClientActiveObject::registerType(getType(), create);
	}

	bool getCollisionBox(aabb3f *toset) {
		if (m_prop.physical) {
			//update collision box
			toset->MinEdge = m_prop.collisionbox.MinEdge * BS;
			toset->MaxEdge = m_prop.collisionbox.MaxEdge * BS;

			toset->MinEdge += m_position;
			toset->MaxEdge += m_position;

			return true;
		}

		return false;
	}

	bool collideWithObjects() {
		return m_prop.collideWithObjects;
	}

	void initialize(const std::string &data)
	{
		/*
		infostream<<"GenericCAO: Got init data"<<std::endl;
		*/
		std::istringstream is(data, std::ios::binary);
		int num_messages = 0;
		// version
		u8 version = readU8(is);
		// check version
		if(version == 1) // In PROTOCOL_VERSION 14
		{
			m_name = deSerializeString(is);
			m_is_player = readU8(is);
			m_id = readS16(is);
			m_position = readV3F1000(is);
			m_yaw = readF1000(is);
			m_hp = readS16(is);
			num_messages = readU8(is);
		}
		else if(version == 0) // In PROTOCOL_VERSION 13
		{
			m_name = deSerializeString(is);
			m_is_player = readU8(is);
			m_position = readV3F1000(is);
			m_yaw = readF1000(is);
			m_hp = readS16(is);
			num_messages = readU8(is);
		}
		else
		{
			errorstream<<"GenericCAO: Unsupported init data version"
					<<std::endl;
			return;
		}

		for(int i=0; i<num_messages; i++){
			std::string message = deSerializeLongString(is);
			processMessage(message);
		}

		pos_translator.init(m_position);
		updateNodePos();
		
		if(m_is_player){
			Player *player = m_env->getPlayer(m_name.c_str());
			if(player && player->isLocal()){
				m_is_local_player = true;
			}
			m_env->addPlayerName(m_name.c_str());
		}
	}

	~GenericCAO()
	{
		if(m_is_player){
			m_env->removePlayerName(m_name.c_str());
		}
	}

	static ClientActiveObject* create(IGameDef *gamedef, ClientEnvironment *env)
	{
		return new GenericCAO(gamedef, env);
	}

	u8 getType() const
	{
		return ACTIVEOBJECT_TYPE_GENERIC;
	}
	core::aabbox3d<f32>* getSelectionBox()
	{
		if(!m_prop.is_visible || !m_is_visible || m_is_local_player || getParent() != NULL)
			return NULL;
		return &m_selection_box;
	}
	v3f getPosition()
	{
		if(getParent() != NULL){
			if(m_meshnode)
				return m_meshnode->getAbsolutePosition();
			if(m_animated_meshnode)
				return m_animated_meshnode->getAbsolutePosition();
			if(m_spritenode)
				return m_spritenode->getAbsolutePosition();
			return m_position;
		}
		return pos_translator.vect_show;
	}

	scene::IMeshSceneNode *getMeshSceneNode()
	{
		if(m_meshnode)
			return m_meshnode;
		return NULL;
	}

	scene::IAnimatedMeshSceneNode *getAnimatedMeshSceneNode()
	{
		if(m_animated_meshnode)
			return m_animated_meshnode;
		return NULL;
	}

	scene::IBillboardSceneNode *getSpriteSceneNode()
	{
		if(m_spritenode)
			return m_spritenode;
		return NULL;
	}

	bool isPlayer()
	{
		return m_is_player;
	}

	bool isLocalPlayer()
	{
		return m_is_local_player;
	}

	void setAttachments()
	{
		updateAttachments();
	}

	ClientActiveObject *getParent()
	{
		ClientActiveObject *obj = NULL;
		for(std::vector<core::vector2d<int> >::const_iterator cii = m_env->attachment_list.begin(); cii != m_env->attachment_list.end(); cii++)
		{
			if(cii->X == getId()){ // This ID is our child
				if(cii->Y > 0){ // A parent ID exists for our child
					if(cii->X != cii->Y){ // The parent and child ID are not the same
						obj = m_env->getActiveObject(cii->Y);
					}
				}
				break;
			}
		}
		if(obj)
			return obj;
		return NULL;
	}

	void removeFromScene(bool permanent)
	{
		if(permanent) // Should be true when removing the object permanently and false when refreshing (eg: updating visuals)
		{
			// Detach this object's children
			for(std::vector<core::vector2d<int> >::iterator ii = m_env->attachment_list.begin(); ii != m_env->attachment_list.end(); ii++)
			{
				if(ii->Y == getId()) // Is a child of our object
				{
					ii->Y = 0;
					ClientActiveObject *obj = m_env->getActiveObject(ii->X); // Get the object of the child
					if(obj)
						obj->setAttachments();
				}
			}
			// Delete this object from the attachments list
			for(std::vector<core::vector2d<int> >::iterator ii = m_env->attachment_list.begin(); ii != m_env->attachment_list.end(); ii++)
			{
				if(ii->X == getId()) // Is our object
				{
					m_env->attachment_list.erase(ii);
					break;
				}
			}
		}

		if(m_meshnode){
			m_meshnode->remove();
			m_meshnode = NULL;
		}
		if(m_animated_meshnode){
			m_animated_meshnode->remove();
			m_animated_meshnode = NULL;
		}
		if(m_spritenode){
			m_spritenode->remove();
			m_spritenode = NULL;
		}
	}

	void addToScene(scene::ISceneManager *smgr, ITextureSource *tsrc,
			IrrlichtDevice *irr)
	{
		m_smgr = smgr;
		m_irr = irr;

		if(m_meshnode != NULL || m_animated_meshnode != NULL || m_spritenode != NULL)
			return;
		
		m_visuals_expired = false;

		if(!m_prop.is_visible)
			return;
	
		//video::IVideoDriver* driver = smgr->getVideoDriver();

		if(m_prop.visual == "sprite"){
/*
			infostream<<"GenericCAO::addToScene(): single_sprite"<<std::endl;
*/
			m_spritenode = smgr->addBillboardSceneNode(
					NULL, v2f(1, 1), v3f(0,0,0), -1);
			m_spritenode->setMaterialTexture(0,
					tsrc->getTexture("unknown_node.png"));
			m_spritenode->setMaterialFlag(video::EMF_LIGHTING, false);
			m_spritenode->setMaterialFlag(video::EMF_BILINEAR_FILTER, false);
			m_spritenode->setMaterialType(video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF);
			m_spritenode->setMaterialFlag(video::EMF_FOG_ENABLE, true);
			u8 li = m_last_light;
			m_spritenode->setColor(video::SColor(255,li,li,li));
			m_spritenode->setSize(m_prop.visual_size*BS);
			{
				const float txs = 1.0 / 1;
				const float tys = 1.0 / 1;
				setBillboardTextureMatrix(m_spritenode,
						txs, tys, 0, 0);
			}
		}
		else if(m_prop.visual == "upright_sprite")
		{
			scene::SMesh *mesh = new scene::SMesh();
			double dx = BS*m_prop.visual_size.X/2;
			double dy = BS*m_prop.visual_size.Y/2;
			{ // Front
			scene::IMeshBuffer *buf = new scene::SMeshBuffer();
			u8 li = m_last_light;
			video::SColor c(255,li,li,li);
			video::S3DVertex vertices[4] =
			{
				video::S3DVertex(-dx,-dy,0, 0,0,0, c, 0,1),
				video::S3DVertex(dx,-dy,0, 0,0,0, c, 1,1),
				video::S3DVertex(dx,dy,0, 0,0,0, c, 1,0),
				video::S3DVertex(-dx,dy,0, 0,0,0, c, 0,0),
			};
			u16 indices[] = {0,1,2,2,3,0};
			buf->append(vertices, 4, indices, 6);
			// Set material
			buf->getMaterial().setFlag(video::EMF_LIGHTING, false);
			buf->getMaterial().setFlag(video::EMF_BILINEAR_FILTER, false);
			buf->getMaterial().setFlag(video::EMF_FOG_ENABLE, true);
			buf->getMaterial().MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;
			// Add to mesh
			mesh->addMeshBuffer(buf);
			buf->drop();
			}
			{ // Back
			scene::IMeshBuffer *buf = new scene::SMeshBuffer();
			u8 li = m_last_light;
			video::SColor c(255,li,li,li);
			video::S3DVertex vertices[4] =
			{
				video::S3DVertex(dx,-dy,0, 0,0,0, c, 1,1),
				video::S3DVertex(-dx,-dy,0, 0,0,0, c, 0,1),
				video::S3DVertex(-dx,dy,0, 0,0,0, c, 0,0),
				video::S3DVertex(dx,dy,0, 0,0,0, c, 1,0),
			};
			u16 indices[] = {0,1,2,2,3,0};
			buf->append(vertices, 4, indices, 6);
			// Set material
			buf->getMaterial().setFlag(video::EMF_LIGHTING, false);
			buf->getMaterial().setFlag(video::EMF_BILINEAR_FILTER, false);
			buf->getMaterial().setFlag(video::EMF_FOG_ENABLE, true);
			buf->getMaterial().MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF;
			// Add to mesh
			mesh->addMeshBuffer(buf);
			buf->drop();
			}
			m_meshnode = smgr->addMeshSceneNode(mesh, NULL);
			mesh->drop();
			// Set it to use the materials of the meshbuffers directly.
			// This is needed for changing the texture in the future
			m_meshnode->setReadOnlyMaterials(true);
		}
		else if(m_prop.visual == "cube"){
			infostream<<"GenericCAO::addToScene(): cube"<<std::endl;
			scene::IMesh *mesh = createCubeMesh(v3f(BS,BS,BS));
			m_meshnode = smgr->addMeshSceneNode(mesh, NULL);
			mesh->drop();
			
			m_meshnode->setScale(v3f(m_prop.visual_size.X,
					m_prop.visual_size.Y,
					m_prop.visual_size.X));
			u8 li = m_last_light;
			setMeshColor(m_meshnode->getMesh(), video::SColor(255,li,li,li));

			m_meshnode->setMaterialFlag(video::EMF_LIGHTING, false);
			m_meshnode->setMaterialFlag(video::EMF_BILINEAR_FILTER, false);
			m_meshnode->setMaterialType(video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF);
			m_meshnode->setMaterialFlag(video::EMF_FOG_ENABLE, true);
		}
		else if(m_prop.visual == "mesh"){
			infostream<<"GenericCAO::addToScene(): mesh"<<std::endl;
			scene::IAnimatedMesh *mesh = m_gamedef->getMesh(m_prop.mesh);
			if(mesh)
			{
				m_animated_meshnode = smgr->addAnimatedMeshSceneNode(mesh, NULL);
				mesh->drop(); // The scene node took hold of it
				m_animated_meshnode->animateJoints(); // Needed for some animations
				m_animated_meshnode->setScale(v3f(m_prop.visual_size.X,
						m_prop.visual_size.Y,
						m_prop.visual_size.X));
				u8 li = m_last_light;
				setMeshColor(m_animated_meshnode->getMesh(), video::SColor(255,li,li,li));

				m_animated_meshnode->setMaterialFlag(video::EMF_LIGHTING, false);
				m_animated_meshnode->setMaterialFlag(video::EMF_BILINEAR_FILTER, false);
				m_animated_meshnode->setMaterialType(video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF);
				m_animated_meshnode->setMaterialFlag(video::EMF_FOG_ENABLE, true);
			}
			else
				errorstream<<"GenericCAO::addToScene(): Could not load mesh "<<m_prop.mesh<<std::endl;
		}
		else if(m_prop.visual == "wielditem"){
/*
			infostream<<"GenericCAO::addToScene(): node"<<std::endl;
			infostream<<"textures: "<<m_prop.textures.size()<<std::endl;
*/
			if(m_prop.textures.size() >= 1){
/*
				infostream<<"textures[0]: "<<m_prop.textures[0]<<std::endl;
*/
				IItemDefManager *idef = m_gamedef->idef();
				ItemStack item(m_prop.textures[0], 1, 0, "", idef);
				scene::IMesh *item_mesh = idef->getWieldMesh(item.getDefinition(idef).name, m_gamedef);
				
				// Copy mesh to be able to set unique vertex colors
				scene::IMeshManipulator *manip =
						irr->getVideoDriver()->getMeshManipulator();
				scene::IMesh *mesh = manip->createMeshUniquePrimitives(item_mesh);

				m_meshnode = smgr->addMeshSceneNode(mesh, NULL);
				mesh->drop();
				
				m_meshnode->setScale(v3f(m_prop.visual_size.X/2,
						m_prop.visual_size.Y/2,
						m_prop.visual_size.X/2));
				u8 li = m_last_light;
				setMeshColor(m_meshnode->getMesh(), video::SColor(255,li,li,li));
			}
		} else {
			infostream<<"GenericCAO::addToScene(): \""<<m_prop.visual
					<<"\" not supported"<<std::endl;
		}
		updateTextures("");
		
		scene::ISceneNode *node = NULL;
		if(m_spritenode)
			node = m_spritenode;
		else if(m_animated_meshnode)
			node = m_animated_meshnode;
		else if(m_meshnode)
			node = m_meshnode;
		if(node && m_is_player && !m_is_local_player){
			// Add a text node for showing the name
			gui::IGUIEnvironment* gui = irr->getGUIEnvironment();
			std::wstring wname = narrow_to_wide(m_name);
			m_textnode = smgr->addTextSceneNode(gui->getBuiltInFont(),
					wname.c_str(), video::SColor(255,255,255,255), node);
			m_textnode->setPosition(v3f(0, BS*1.1, 0));
		}

		updateNodePos();
		updateAnimation();
		updateBonePosition();
		updateAttachments();
	}

	void expireVisuals()
	{
		m_visuals_expired = true;
	}
		
	void updateLight(u8 light_at_pos)
	{
		u8 li = decode_light(light_at_pos);
		if(li != m_last_light){
			m_last_light = li;
			video::SColor color(255,li,li,li);
			if(m_meshnode)
				setMeshColor(m_meshnode->getMesh(), color);
			if(m_animated_meshnode)
				setMeshColor(m_animated_meshnode->getMesh(), color);
			if(m_spritenode)
				m_spritenode->setColor(color);
		}
	}

	v3s16 getLightPosition()
	{
		return floatToInt(m_position, BS);
	}

	void updateNodePos()
	{
		if(getParent() != NULL)
			return;

		v3s16 camera_offset = m_env->getCameraOffset();
		if(m_meshnode){
			m_meshnode->setPosition(pos_translator.vect_show-intToFloat(camera_offset, BS));
			v3f rot = m_meshnode->getRotation();
			rot.Y = -m_yaw;
			m_meshnode->setRotation(rot);
		}
		if(m_animated_meshnode){
			m_animated_meshnode->setPosition(pos_translator.vect_show-intToFloat(camera_offset, BS));
			v3f rot = m_animated_meshnode->getRotation();
			rot.Y = -m_yaw;
			m_animated_meshnode->setRotation(rot);
		}
		if(m_spritenode){
			m_spritenode->setPosition(pos_translator.vect_show-intToFloat(camera_offset, BS));
		}
	}
	
	void step(float dtime, ClientEnvironment *env)
	{
		// Handel model of local player instantly to prevent lags
		if(m_is_local_player) {
			LocalPlayer *player = m_env->getLocalPlayer();

			if (player->camera_mode > CAMERA_MODE_FIRST) {
				int old_anim = player->last_animation;
				float old_anim_speed = player->last_animation_speed;
				m_is_visible = true;
				m_position = player->getPosition() + v3f(0,BS,0);
				m_velocity = v3f(0,0,0);
				m_acceleration = v3f(0,0,0);
				pos_translator.vect_show = m_position;
				m_yaw = player->getYaw();
				PlayerControl controls = player->getPlayerControl();

				bool walking = false;
				if(controls.up || controls.down || controls.left || controls.right)
					walking = true;

				f32 new_speed = player->local_animation_speed;
				v2s32 new_anim = v2s32(0,0);
				bool allow_update = false;

				// increase speed if using fast or flying fast
				if((g_settings->getBool("fast_move") &&
					m_gamedef->checkLocalPrivilege("fast")) &&
					(controls.aux1 ||
					(!player->touching_ground &&
					g_settings->getBool("free_move") &&
					m_gamedef->checkLocalPrivilege("fly"))))
						new_speed *= 1.5;
				// slowdown speed if sneeking
				if(controls.sneak && walking)
					new_speed /= 2;

				if(walking && (controls.LMB || controls.RMB)) {
					new_anim = player->local_animations[3];
					player->last_animation = WD_ANIM;
				} else if(walking) {
					new_anim = player->local_animations[1];
					player->last_animation = WALK_ANIM;
				} else if(controls.LMB || controls.RMB) {
					new_anim = player->local_animations[2];
					player->last_animation = DIG_ANIM;
				}

				// Apply animations if input detected and not attached
				// or set idle animation
				if ((new_anim.X + new_anim.Y) > 0 && !player->isAttached) {
					allow_update = true;
					m_animation_range = new_anim;
					m_animation_speed = new_speed;
					player->last_animation_speed = m_animation_speed;
				} else {
					player->last_animation = NO_ANIM;
					if (old_anim != NO_ANIM) {
						m_animation_range = player->local_animations[0];
						updateAnimation();
					}
				}

				// Update local player animations
				if ((player->last_animation != old_anim || m_animation_speed != old_anim_speed) &&
					player->last_animation != NO_ANIM && allow_update)
						updateAnimation();

			} else {
				m_is_visible = false;
			}
        }

		if(m_visuals_expired && m_smgr && m_irr){
			m_visuals_expired = false;

			// Attachments, part 1: All attached objects must be unparented first, or Irrlicht causes a segmentation fault
			for(std::vector<core::vector2d<int> >::iterator ii = m_env->attachment_list.begin(); ii != m_env->attachment_list.end(); ii++)
			{
				if(ii->Y == getId()) // This is a child of our parent
				{
					ClientActiveObject *obj = m_env->getActiveObject(ii->X); // Get the object of the child
					if(obj)
					{
						scene::IMeshSceneNode *m_child_meshnode = obj->getMeshSceneNode();
						scene::IAnimatedMeshSceneNode *m_child_animated_meshnode = obj->getAnimatedMeshSceneNode();
						scene::IBillboardSceneNode *m_child_spritenode = obj->getSpriteSceneNode();
						if(m_child_meshnode)
							m_child_meshnode->setParent(m_smgr->getRootSceneNode());
						if(m_child_animated_meshnode)
							m_child_animated_meshnode->setParent(m_smgr->getRootSceneNode());
						if(m_child_spritenode)
							m_child_spritenode->setParent(m_smgr->getRootSceneNode());
					}
				}
			}

			removeFromScene(false);
			addToScene(m_smgr, m_gamedef->tsrc(), m_irr);

			// Attachments, part 2: Now that the parent has been refreshed, put its attachments back
			for(std::vector<core::vector2d<int> >::iterator ii = m_env->attachment_list.begin(); ii != m_env->attachment_list.end(); ii++)
			{
				if(ii->Y == getId()) // This is a child of our parent
				{
					ClientActiveObject *obj = m_env->getActiveObject(ii->X); // Get the object of the child
					if(obj)
						obj->setAttachments();
				}
			}
		}

		// Make sure m_is_visible is always applied
		if(m_meshnode)
			m_meshnode->setVisible(m_is_visible);
		if(m_animated_meshnode)
			m_animated_meshnode->setVisible(m_is_visible);
		if(m_spritenode)
			m_spritenode->setVisible(m_is_visible);
		if(m_textnode)
			m_textnode->setVisible(m_is_visible);

		if(getParent() != NULL) // Attachments should be glued to their parent by Irrlicht
		{
			// Set these for later
			m_position = getPosition();
			m_velocity = v3f(0,0,0);
			m_acceleration = v3f(0,0,0);
			pos_translator.vect_show = m_position;

			if(m_is_local_player) // Update local player attachment position
			{
				LocalPlayer *player = m_env->getLocalPlayer();
				player->overridePosition = getParent()->getPosition();
				m_env->getLocalPlayer()->parent = getParent();
			}
		}
		else
		{
			v3f lastpos = pos_translator.vect_show;

			if(m_prop.physical){
				core::aabbox3d<f32> box = m_prop.collisionbox;
				box.MinEdge *= BS;
				box.MaxEdge *= BS;
				collisionMoveResult moveresult;
				f32 pos_max_d = BS*0.125; // Distance per iteration
				v3f p_pos = m_position;
				v3f p_velocity = m_velocity;
				v3f p_acceleration = m_acceleration;
				moveresult = collisionMoveSimple(env,env->getGameDef(),
						pos_max_d, box, m_prop.stepheight, dtime,
						p_pos, p_velocity, p_acceleration,
						this, m_prop.collideWithObjects);
				// Apply results
				m_position = p_pos;
				m_velocity = p_velocity;
				m_acceleration = p_acceleration;
				
				bool is_end_position = moveresult.collides;
				pos_translator.update(m_position, is_end_position, dtime);
				pos_translator.translate(dtime);
				updateNodePos();
			} else {
				m_position += dtime * m_velocity + 0.5 * dtime * dtime * m_acceleration;
				m_velocity += dtime * m_acceleration;
				pos_translator.update(m_position, pos_translator.aim_is_end, pos_translator.anim_time);
				pos_translator.translate(dtime);
				updateNodePos();
			}

			float moved = lastpos.getDistanceFrom(pos_translator.vect_show);
			m_step_distance_counter += moved;
			if(m_step_distance_counter > 1.5*BS){
				m_step_distance_counter = 0;
				if(!m_is_local_player && m_prop.makes_footstep_sound){
					INodeDefManager *ndef = m_gamedef->ndef();
					v3s16 p = floatToInt(getPosition() + v3f(0,
							(m_prop.collisionbox.MinEdge.Y-0.5)*BS, 0), BS);
					MapNode n = m_env->getMap().getNodeNoEx(p);
					SimpleSoundSpec spec = ndef->get(n).sound_footstep;
					m_gamedef->sound()->playSoundAt(spec, false, getPosition());
				}
			}
		}

		m_anim_timer += dtime;
		if(m_anim_timer >= m_anim_framelength){
			m_anim_timer -= m_anim_framelength;
			m_anim_frame++;
			if(m_anim_frame >= m_anim_num_frames)
				m_anim_frame = 0;
		}

		updateTexturePos();

		if(m_reset_textures_timer >= 0){
			m_reset_textures_timer -= dtime;
			if(m_reset_textures_timer <= 0){
				m_reset_textures_timer = -1;
				updateTextures("");
			}
		}
		if(getParent() == NULL && fabs(m_prop.automatic_rotate) > 0.001){
			m_yaw += dtime * m_prop.automatic_rotate * 180 / M_PI;
			updateNodePos();
		}

		if (getParent() == NULL && m_prop.automatic_face_movement_dir &&
				(fabs(m_velocity.Z) > 0.001 || fabs(m_velocity.X) > 0.001)){
			m_yaw = atan2(m_velocity.Z,m_velocity.X) * 180 / M_PI + m_prop.automatic_face_movement_dir_offset;
			updateNodePos();
		}
	}

	void updateTexturePos()
	{
		if(m_spritenode){
			scene::ICameraSceneNode* camera =
					m_spritenode->getSceneManager()->getActiveCamera();
			if(!camera)
				return;
			v3f cam_to_entity = m_spritenode->getAbsolutePosition()
					- camera->getAbsolutePosition();
			cam_to_entity.normalize();

			int row = m_tx_basepos.Y;
			int col = m_tx_basepos.X;
			
			if(m_tx_select_horiz_by_yawpitch)
			{
				if(cam_to_entity.Y > 0.75)
					col += 5;
				else if(cam_to_entity.Y < -0.75)
					col += 4;
				else{
					float mob_dir = atan2(cam_to_entity.Z, cam_to_entity.X) / M_PI * 180.;
					float dir = mob_dir - m_yaw;
					dir = wrapDegrees_180(dir);
					//infostream<<"id="<<m_id<<" dir="<<dir<<std::endl;
					if(fabs(wrapDegrees_180(dir - 0)) <= 45.1)
						col += 2;
					else if(fabs(wrapDegrees_180(dir - 90)) <= 45.1)
						col += 3;
					else if(fabs(wrapDegrees_180(dir - 180)) <= 45.1)
						col += 0;
					else if(fabs(wrapDegrees_180(dir + 90)) <= 45.1)
						col += 1;
					else
						col += 4;
				}
			}
			
			// Animation goes downwards
			row += m_anim_frame;

			float txs = m_tx_size.X;
			float tys = m_tx_size.Y;
			setBillboardTextureMatrix(m_spritenode,
					txs, tys, col, row);
		}
	}

	void updateTextures(const std::string &mod)
	{
		ITextureSource *tsrc = m_gamedef->tsrc();

		bool use_trilinear_filter = g_settings->getBool("trilinear_filter");
		bool use_bilinear_filter = g_settings->getBool("bilinear_filter");
		bool use_anisotropic_filter = g_settings->getBool("anisotropic_filter");

		if(m_spritenode)
		{
			if(m_prop.visual == "sprite")
			{
				std::string texturestring = "unknown_node.png";
				if(m_prop.textures.size() >= 1)
					texturestring = m_prop.textures[0];
				texturestring += mod;
				m_spritenode->setMaterialTexture(0,
						tsrc->getTexture(texturestring));

				// This allows setting per-material colors. However, until a real lighting
				// system is added, the code below will have no effect. Once MineTest
				// has directional lighting, it should work automatically.
				if(m_prop.colors.size() >= 1)
				{
					m_spritenode->getMaterial(0).AmbientColor = m_prop.colors[0];
					m_spritenode->getMaterial(0).DiffuseColor = m_prop.colors[0];
					m_spritenode->getMaterial(0).SpecularColor = m_prop.colors[0];
				}

				m_spritenode->getMaterial(0).setFlag(video::EMF_TRILINEAR_FILTER, use_trilinear_filter);
				m_spritenode->getMaterial(0).setFlag(video::EMF_BILINEAR_FILTER, use_bilinear_filter);
				m_spritenode->getMaterial(0).setFlag(video::EMF_ANISOTROPIC_FILTER, use_anisotropic_filter);
			}
		}
		if(m_animated_meshnode)
		{
			if(m_prop.visual == "mesh")
			{
				for (u32 i = 0; i < m_prop.textures.size() && i < m_animated_meshnode->getMaterialCount(); ++i)
				{
					std::string texturestring = m_prop.textures[i];
					if(texturestring == "")
						continue; // Empty texture string means don't modify that material
					texturestring += mod;
					video::ITexture* texture = tsrc->getTexture(texturestring);
					if(!texture)
					{
						errorstream<<"GenericCAO::updateTextures(): Could not load texture "<<texturestring<<std::endl;
						continue;
					}

					// Set material flags and texture
					video::SMaterial& material = m_animated_meshnode->getMaterial(i);
					material.TextureLayer[0].Texture = texture;
					material.setFlag(video::EMF_LIGHTING, false);
					material.setFlag(video::EMF_BILINEAR_FILTER, false);

					m_animated_meshnode->getMaterial(i).setFlag(video::EMF_TRILINEAR_FILTER, use_trilinear_filter);
					m_animated_meshnode->getMaterial(i).setFlag(video::EMF_BILINEAR_FILTER, use_bilinear_filter);
					m_animated_meshnode->getMaterial(i).setFlag(video::EMF_ANISOTROPIC_FILTER, use_anisotropic_filter);
				}
				for (u32 i = 0; i < m_prop.colors.size() && i < m_animated_meshnode->getMaterialCount(); ++i)
				{
					// This allows setting per-material colors. However, until a real lighting
					// system is added, the code below will have no effect. Once MineTest
					// has directional lighting, it should work automatically.
					m_animated_meshnode->getMaterial(i).AmbientColor = m_prop.colors[i];
					m_animated_meshnode->getMaterial(i).DiffuseColor = m_prop.colors[i];
					m_animated_meshnode->getMaterial(i).SpecularColor = m_prop.colors[i];
				}
			}
		}
		if(m_meshnode)
		{
			if(m_prop.visual == "cube")
			{
				for (u32 i = 0; i < 6; ++i)
				{
					std::string texturestring = "unknown_node.png";
					if(m_prop.textures.size() > i)
						texturestring = m_prop.textures[i];
					texturestring += mod;


					// Set material flags and texture
					video::SMaterial& material = m_meshnode->getMaterial(i);
					material.setFlag(video::EMF_LIGHTING, false);
					material.setFlag(video::EMF_BILINEAR_FILTER, false);
					material.setTexture(0,
							tsrc->getTexture(texturestring));
					material.getTextureMatrix(0).makeIdentity();

					// This allows setting per-material colors. However, until a real lighting
					// system is added, the code below will have no effect. Once MineTest
					// has directional lighting, it should work automatically.
					if(m_prop.colors.size() > i)
					{
						m_meshnode->getMaterial(i).AmbientColor = m_prop.colors[i];
						m_meshnode->getMaterial(i).DiffuseColor = m_prop.colors[i];
						m_meshnode->getMaterial(i).SpecularColor = m_prop.colors[i];
					}

					m_meshnode->getMaterial(i).setFlag(video::EMF_TRILINEAR_FILTER, use_trilinear_filter);
					m_meshnode->getMaterial(i).setFlag(video::EMF_BILINEAR_FILTER, use_bilinear_filter);
					m_meshnode->getMaterial(i).setFlag(video::EMF_ANISOTROPIC_FILTER, use_anisotropic_filter);
				}
			}
			else if(m_prop.visual == "upright_sprite")
			{
				scene::IMesh *mesh = m_meshnode->getMesh();
				{
					std::string tname = "unknown_object.png";
					if(m_prop.textures.size() >= 1)
						tname = m_prop.textures[0];
					tname += mod;
					scene::IMeshBuffer *buf = mesh->getMeshBuffer(0);
					buf->getMaterial().setTexture(0,
							tsrc->getTexture(tname));
					
					// This allows setting per-material colors. However, until a real lighting
					// system is added, the code below will have no effect. Once MineTest
					// has directional lighting, it should work automatically.
					if(m_prop.colors.size() >= 1)
					{
						buf->getMaterial().AmbientColor = m_prop.colors[0];
						buf->getMaterial().DiffuseColor = m_prop.colors[0];
						buf->getMaterial().SpecularColor = m_prop.colors[0];
					}

					buf->getMaterial().setFlag(video::EMF_TRILINEAR_FILTER, use_trilinear_filter);
					buf->getMaterial().setFlag(video::EMF_BILINEAR_FILTER, use_bilinear_filter);
					buf->getMaterial().setFlag(video::EMF_ANISOTROPIC_FILTER, use_anisotropic_filter);
				}
				{
					std::string tname = "unknown_object.png";
					if(m_prop.textures.size() >= 2)
						tname = m_prop.textures[1];
					else if(m_prop.textures.size() >= 1)
						tname = m_prop.textures[0];
					tname += mod;
					scene::IMeshBuffer *buf = mesh->getMeshBuffer(1);
					buf->getMaterial().setTexture(0,
							tsrc->getTexture(tname));

					// This allows setting per-material colors. However, until a real lighting
					// system is added, the code below will have no effect. Once MineTest
					// has directional lighting, it should work automatically.
					if(m_prop.colors.size() >= 2)
					{
						buf->getMaterial().AmbientColor = m_prop.colors[1];
						buf->getMaterial().DiffuseColor = m_prop.colors[1];
						buf->getMaterial().SpecularColor = m_prop.colors[1];
					}
					else if(m_prop.colors.size() >= 1)
					{
						buf->getMaterial().AmbientColor = m_prop.colors[0];
						buf->getMaterial().DiffuseColor = m_prop.colors[0];
						buf->getMaterial().SpecularColor = m_prop.colors[0];
					}

					buf->getMaterial().setFlag(video::EMF_TRILINEAR_FILTER, use_trilinear_filter);
					buf->getMaterial().setFlag(video::EMF_BILINEAR_FILTER, use_bilinear_filter);
					buf->getMaterial().setFlag(video::EMF_ANISOTROPIC_FILTER, use_anisotropic_filter);
				}
			}
		}
	}

	void updateAnimation()
	{
		if(m_animated_meshnode == NULL)
			return;
		m_animated_meshnode->setFrameLoop(m_animation_range.X, m_animation_range.Y);
		m_animated_meshnode->setAnimationSpeed(m_animation_speed);
		m_animated_meshnode->setTransitionTime(m_animation_blend);
	}

	void updateBonePosition()
	{
		if(!m_bone_position.size() || m_animated_meshnode == NULL)
			return;

		m_animated_meshnode->setJointMode(irr::scene::EJUOR_CONTROL); // To write positions to the mesh on render
		for(std::map<std::string, core::vector2d<v3f> >::const_iterator ii = m_bone_position.begin(); ii != m_bone_position.end(); ++ii){
			std::string bone_name = (*ii).first;
			v3f bone_pos = (*ii).second.X;
			v3f bone_rot = (*ii).second.Y;
			irr::scene::IBoneSceneNode* bone = m_animated_meshnode->getJointNode(bone_name.c_str());
			if(bone)
			{
				bone->setPosition(bone_pos);
				bone->setRotation(bone_rot);
			}
		}
	}
	
	void updateAttachments()
	{
		m_attached_to_local = getParent() != NULL && getParent()->isLocalPlayer();
		m_is_visible = !m_attached_to_local; // Objects attached to the local player should always be hidden

		if(getParent() == NULL || m_attached_to_local) // Detach or don't attach
		{
			if(m_meshnode)
			{
				v3f old_position = m_meshnode->getAbsolutePosition();
				v3f old_rotation = m_meshnode->getRotation();
				m_meshnode->setParent(m_smgr->getRootSceneNode());
				m_meshnode->setPosition(old_position);
				m_meshnode->setRotation(old_rotation);
				m_meshnode->updateAbsolutePosition();
			}
			if(m_animated_meshnode)
			{
				v3f old_position = m_animated_meshnode->getAbsolutePosition();
				v3f old_rotation = m_animated_meshnode->getRotation();
				m_animated_meshnode->setParent(m_smgr->getRootSceneNode());
				m_animated_meshnode->setPosition(old_position);
				m_animated_meshnode->setRotation(old_rotation);
				m_animated_meshnode->updateAbsolutePosition();
			}
			if(m_spritenode)
			{
				v3f old_position = m_spritenode->getAbsolutePosition();
				v3f old_rotation = m_spritenode->getRotation();
				m_spritenode->setParent(m_smgr->getRootSceneNode());
				m_spritenode->setPosition(old_position);
				m_spritenode->setRotation(old_rotation);
				m_spritenode->updateAbsolutePosition();
			}
			if(m_is_local_player)
			{
				LocalPlayer *player = m_env->getLocalPlayer();
				player->isAttached = false;
			}
		}
		else // Attach
		{
			scene::IMeshSceneNode *parent_mesh = NULL;
			if(getParent()->getMeshSceneNode())
				parent_mesh = getParent()->getMeshSceneNode();
			scene::IAnimatedMeshSceneNode *parent_animated_mesh = NULL;
			if(getParent()->getAnimatedMeshSceneNode())
				parent_animated_mesh = getParent()->getAnimatedMeshSceneNode();
			scene::IBillboardSceneNode *parent_sprite = NULL;
			if(getParent()->getSpriteSceneNode())
				parent_sprite = getParent()->getSpriteSceneNode();

			scene::IBoneSceneNode *parent_bone = NULL;
			if(parent_animated_mesh && m_attachment_bone != "")
				parent_bone = parent_animated_mesh->getJointNode(m_attachment_bone.c_str());

			// The spaghetti code below makes sure attaching works if either the parent or child is a spritenode, meshnode, or animatedmeshnode
			// TODO: Perhaps use polymorphism here to save code duplication
			if(m_meshnode){
				if(parent_bone){
					m_meshnode->setParent(parent_bone);
					m_meshnode->setPosition(m_attachment_position);
					m_meshnode->setRotation(m_attachment_rotation);
					m_meshnode->updateAbsolutePosition();
				}
				else
				{
					if(parent_mesh){
						m_meshnode->setParent(parent_mesh);
						m_meshnode->setPosition(m_attachment_position);
						m_meshnode->setRotation(m_attachment_rotation);
						m_meshnode->updateAbsolutePosition();
					}
					else if(parent_animated_mesh){
						m_meshnode->setParent(parent_animated_mesh);
						m_meshnode->setPosition(m_attachment_position);
						m_meshnode->setRotation(m_attachment_rotation);
						m_meshnode->updateAbsolutePosition();
					}
					else if(parent_sprite){
						m_meshnode->setParent(parent_sprite);
						m_meshnode->setPosition(m_attachment_position);
						m_meshnode->setRotation(m_attachment_rotation);
						m_meshnode->updateAbsolutePosition();
					}
				}
			}
			if(m_animated_meshnode){
				if(parent_bone){
					m_animated_meshnode->setParent(parent_bone);
					m_animated_meshnode->setPosition(m_attachment_position);
					m_animated_meshnode->setRotation(m_attachment_rotation);
					m_animated_meshnode->updateAbsolutePosition();
				}
				else
				{
					if(parent_mesh){
						m_animated_meshnode->setParent(parent_mesh);
						m_animated_meshnode->setPosition(m_attachment_position);
						m_animated_meshnode->setRotation(m_attachment_rotation);
						m_animated_meshnode->updateAbsolutePosition();
					}
					else if(parent_animated_mesh){
						m_animated_meshnode->setParent(parent_animated_mesh);
						m_animated_meshnode->setPosition(m_attachment_position);
						m_animated_meshnode->setRotation(m_attachment_rotation);
						m_animated_meshnode->updateAbsolutePosition();
					}
					else if(parent_sprite){
						m_animated_meshnode->setParent(parent_sprite);
						m_animated_meshnode->setPosition(m_attachment_position);
						m_animated_meshnode->setRotation(m_attachment_rotation);
						m_animated_meshnode->updateAbsolutePosition();
					}
				}
			}
			if(m_spritenode){
				if(parent_bone){
					m_spritenode->setParent(parent_bone);
					m_spritenode->setPosition(m_attachment_position);
					m_spritenode->setRotation(m_attachment_rotation);
					m_spritenode->updateAbsolutePosition();
				}
				else
				{
					if(parent_mesh){
						m_spritenode->setParent(parent_mesh);
						m_spritenode->setPosition(m_attachment_position);
						m_spritenode->setRotation(m_attachment_rotation);
						m_spritenode->updateAbsolutePosition();
					}
					else if(parent_animated_mesh){
						m_spritenode->setParent(parent_animated_mesh);
						m_spritenode->setPosition(m_attachment_position);
						m_spritenode->setRotation(m_attachment_rotation);
						m_spritenode->updateAbsolutePosition();
					}
					else if(parent_sprite){
						m_spritenode->setParent(parent_sprite);
						m_spritenode->setPosition(m_attachment_position);
						m_spritenode->setRotation(m_attachment_rotation);
						m_spritenode->updateAbsolutePosition();
					}
				}
			}
			if(m_is_local_player)
			{
				LocalPlayer *player = m_env->getLocalPlayer();
				player->isAttached = true;
			}
		}
	}

	void processMessage(const std::string &data)
	{
		//infostream<<"GenericCAO: Got message"<<std::endl;
		std::istringstream is(data, std::ios::binary);
		// command
		u8 cmd = readU8(is);
		if(cmd == GENERIC_CMD_SET_PROPERTIES)
		{
			m_prop = gob_read_set_properties(is);

			m_selection_box = m_prop.collisionbox;
			m_selection_box.MinEdge *= BS;
			m_selection_box.MaxEdge *= BS;
				
			m_tx_size.X = 1.0 / m_prop.spritediv.X;
			m_tx_size.Y = 1.0 / m_prop.spritediv.Y;

			if(!m_initial_tx_basepos_set){
				m_initial_tx_basepos_set = true;
				m_tx_basepos = m_prop.initial_sprite_basepos;
			}
			
			expireVisuals();
		}
		else if(cmd == GENERIC_CMD_UPDATE_POSITION)
		{
			// Not sent by the server if this object is an attachment.
			// We might however get here if the server notices the object being detached before the client.
			m_position = readV3F1000(is);
			m_velocity = readV3F1000(is);
			m_acceleration = readV3F1000(is);
			if(fabs(m_prop.automatic_rotate) < 0.001)
				m_yaw = readF1000(is);
			else
				readF1000(is);
			bool do_interpolate = readU8(is);
			bool is_end_position = readU8(is);
			float update_interval = readF1000(is);

			// Place us a bit higher if we're physical, to not sink into
			// the ground due to sucky collision detection...
			if(m_prop.physical)
				m_position += v3f(0,0.002,0);

			if(getParent() != NULL) // Just in case
				return;

			if(do_interpolate){
				if(!m_prop.physical)
					pos_translator.update(m_position, is_end_position, update_interval);
			} else {
				pos_translator.init(m_position);
			}
			updateNodePos();
		}
		else if(cmd == GENERIC_CMD_SET_TEXTURE_MOD)
		{
			std::string mod = deSerializeString(is);
			updateTextures(mod);
		}
		else if(cmd == GENERIC_CMD_SET_SPRITE)
		{
			v2s16 p = readV2S16(is);
			int num_frames = readU16(is);
			float framelength = readF1000(is);
			bool select_horiz_by_yawpitch = readU8(is);
			
			m_tx_basepos = p;
			m_anim_num_frames = num_frames;
			m_anim_framelength = framelength;
			m_tx_select_horiz_by_yawpitch = select_horiz_by_yawpitch;

			updateTexturePos();
		}
		else if(cmd == GENERIC_CMD_SET_PHYSICS_OVERRIDE)
		{
			float override_speed = readF1000(is);
			float override_jump = readF1000(is);
			float override_gravity = readF1000(is);
			// these are sent inverted so we get true when the server sends nothing
			bool sneak = !readU8(is);
			bool sneak_glitch = !readU8(is);
			

			if(m_is_local_player)
			{
				LocalPlayer *player = m_env->getLocalPlayer();
				player->physics_override_speed = override_speed;
				player->physics_override_jump = override_jump;
				player->physics_override_gravity = override_gravity;
				player->physics_override_sneak = sneak;
				player->physics_override_sneak_glitch = sneak_glitch;
			}
		}
		else if(cmd == GENERIC_CMD_SET_ANIMATION)
		{
			// TODO: change frames send as v2s32 value
			v2f range = readV2F1000(is);
			if (!m_is_local_player) {
			 	m_animation_range = v2s32((s32)range.X, (s32)range.Y);
				m_animation_speed = readF1000(is);
				m_animation_blend = readF1000(is);
				updateAnimation();
			} else {
				LocalPlayer *player = m_env->getLocalPlayer();
				if(player->last_animation == NO_ANIM) {
					m_animation_range = v2s32((s32)range.X, (s32)range.Y);
					m_animation_speed = readF1000(is);
					m_animation_blend = readF1000(is);
				}
				// update animation only if local animations present
				// and received animation is unknown (except idle animation)
				bool is_known = false;
				for (int i = 1;i<4;i++) {
					if(m_animation_range.Y == player->local_animations[i].Y)
						is_known = true;
				}
				if(!is_known ||
					(player->local_animations[1].Y + player->local_animations[2].Y < 1)) {
						updateAnimation();
				}
			}
		}
		else if(cmd == GENERIC_CMD_SET_BONE_POSITION)
		{
			std::string bone = deSerializeString(is);
			v3f position = readV3F1000(is);
			v3f rotation = readV3F1000(is);
			m_bone_position[bone] = core::vector2d<v3f>(position, rotation);

			updateBonePosition();
		}
		else if(cmd == GENERIC_CMD_SET_ATTACHMENT)
		{
			// If an entry already exists for this object, delete it first to avoid duplicates
			for(std::vector<core::vector2d<int> >::iterator ii = m_env->attachment_list.begin(); ii != m_env->attachment_list.end(); ii++)
			{
				if(ii->X == getId()) // This is the ID of our object
				{
					m_env->attachment_list.erase(ii);
					break;
				}
			}
			m_env->attachment_list.push_back(core::vector2d<int>(getId(), readS16(is)));
			m_attachment_bone = deSerializeString(is);
			m_attachment_position = readV3F1000(is);
			m_attachment_rotation = readV3F1000(is);

			updateAttachments();
		}
		else if(cmd == GENERIC_CMD_PUNCHED)
		{
			/*s16 damage =*/ readS16(is);
			s16 result_hp = readS16(is);

			// Use this instead of the send damage to not interfere with prediction
			s16 damage = m_hp - result_hp;

			m_hp = result_hp;

			if (damage > 0) {
				if (m_hp <= 0) {
					// TODO: Execute defined fast response
					// As there is no definition, make a smoke puff
					ClientSimpleObject *simple = createSmokePuff(
							m_smgr, m_env, m_position,
							m_prop.visual_size * BS);
					m_env->addSimpleObject(simple);
				} else {
					// TODO: Execute defined fast response
					// Flashing shall suffice as there is no definition
					m_reset_textures_timer = 0.05;
					if(damage >= 2)
						m_reset_textures_timer += 0.05 * damage;
					updateTextures("^[brighten");
				}
			}
		}
		else if(cmd == GENERIC_CMD_UPDATE_ARMOR_GROUPS)
		{
			m_armor_groups.clear();
			int armor_groups_size = readU16(is);
			for(int i=0; i<armor_groups_size; i++){
				std::string name = deSerializeString(is);
				int rating = readS16(is);
				m_armor_groups[name] = rating;
			}
		}
	}
	
	bool directReportPunch(v3f dir, const ItemStack *punchitem=NULL,
			float time_from_last_punch=1000000)
	{
		assert(punchitem);
		const ToolCapabilities *toolcap =
				&punchitem->getToolCapabilities(m_gamedef->idef());
		PunchDamageResult result = getPunchDamage(
				m_armor_groups,
				toolcap,
				punchitem,
				time_from_last_punch);

		if(result.did_punch && result.damage != 0)
		{
			if(result.damage < m_hp){
				m_hp -= result.damage;
			} else {
				m_hp = 0;
				// TODO: Execute defined fast response
				// As there is no definition, make a smoke puff
				ClientSimpleObject *simple = createSmokePuff(
						m_smgr, m_env, m_position,
						m_prop.visual_size * BS);
				m_env->addSimpleObject(simple);
			}
			// TODO: Execute defined fast response
			// Flashing shall suffice as there is no definition
			m_reset_textures_timer = 0.05;
			if(result.damage >= 2)
				m_reset_textures_timer += 0.05 * result.damage;
			updateTextures("^[brighten");
		}
		
		return false;
	}
	
	std::string debugInfoText()
	{
		std::ostringstream os(std::ios::binary);
		os<<"GenericCAO hp="<<m_hp<<"\n";
		os<<"armor={";
		for(ItemGroupList::const_iterator i = m_armor_groups.begin();
				i != m_armor_groups.end(); i++){
			os<<i->first<<"="<<i->second<<", ";
		}
		os<<"}";
		return os.str();
	}
};

// Prototype
GenericCAO proto_GenericCAO(NULL, NULL);


