/*
tile.cpp
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

#include "tile.h"
#include "irrlichttypes_extrabloated.h"
#include "debug.h"
#include "main.h" // for g_settings
#include "filesys.h"
#include "settings.h"
#include "mesh.h"
#include <ICameraSceneNode.h>
#include "log.h"
#include "gamedef.h"
#include "util/string.h"
#include "util/container.h"
#include "util/thread.h"
#include "util/numeric.h"

/*
	A cache from texture name to texture path
*/
MutexedMap<std::string, std::string> g_texturename_to_path_cache;

/*
	Replaces the filename extension.
	eg:
		std::string image = "a/image.png"
		replace_ext(image, "jpg")
		-> image = "a/image.jpg"
	Returns true on success.
*/
static bool replace_ext(std::string &path, const char *ext)
{
	if(ext == NULL)
		return false;
	// Find place of last dot, fail if \ or / found.
	s32 last_dot_i = -1;
	for(s32 i=path.size()-1; i>=0; i--)
	{
		if(path[i] == '.')
		{
			last_dot_i = i;
			break;
		}

		if(path[i] == '\\' || path[i] == '/')
			break;
	}
	// If not found, return an empty string
	if(last_dot_i == -1)
		return false;
	// Else make the new path
	path = path.substr(0, last_dot_i+1) + ext;
	return true;
}

/*
	Find out the full path of an image by trying different filename
	extensions.

	If failed, return "".
*/
std::string getImagePath(std::string path)
{
	// A NULL-ended list of possible image extensions
	const char *extensions[] = {
		"png", "jpg", "bmp", "tga",
		"pcx", "ppm", "psd", "wal", "rgb",
		NULL
	};
	// If there is no extension, add one
	if(removeStringEnd(path, extensions) == "")
		path = path + ".png";
	// Check paths until something is found to exist
	const char **ext = extensions;
	do{
		bool r = replace_ext(path, *ext);
		if(r == false)
			return "";
		if(fs::PathExists(path))
			return path;
	}
	while((++ext) != NULL);

	return "";
}

/*
	Gets the path to a texture by first checking if the texture exists
	in texture_path and if not, using the data path.

	Checks all supported extensions by replacing the original extension.

	If not found, returns "".

	Utilizes a thread-safe cache.
*/
std::string getTexturePath(const std::string &filename)
{
	std::string fullpath = "";
	/*
		Check from cache
	*/
	bool incache = g_texturename_to_path_cache.get(filename, &fullpath);
	if(incache)
		return fullpath;

	/*
		Check from texture_path
	*/
	std::string texture_path = g_settings->get("texture_path");
	if(texture_path != "")
	{
		std::string testpath = texture_path + DIR_DELIM + filename;
		// Check all filename extensions. Returns "" if not found.
		fullpath = getImagePath(testpath);
	}

	/*
		Check from default data directory
	*/
	if(fullpath == "")
	{
		std::string base_path = porting::path_share + DIR_DELIM + "textures"
				+ DIR_DELIM + "base" + DIR_DELIM + "pack";
		std::string testpath = base_path + DIR_DELIM + filename;
		// Check all filename extensions. Returns "" if not found.
		fullpath = getImagePath(testpath);
	}

	// Add to cache (also an empty result is cached)
	g_texturename_to_path_cache.set(filename, fullpath);

	// Finally return it
	return fullpath;
}

void clearTextureNameCache()
{
	g_texturename_to_path_cache.clear();
}

/*
	Stores internal information about a texture.
*/

/*
	SourceImageCache: A cache used for storing source images.
*/

class SourceImageCache
{
public:
	~SourceImageCache() {
		for(std::map<std::string, video::IImage*>::iterator iter = m_images.begin();
				iter != m_images.end(); iter++) {
			iter->second->drop();
		}
		m_images.clear();
	}
	void insert(const std::string &name, video::IImage *img,
			bool prefer_local, video::IVideoDriver *driver)
	{
		assert(img);
		// Remove old image
		std::map<std::string, video::IImage*>::iterator n;
		n = m_images.find(name);
		if(n != m_images.end()){
			if(n->second)
				n->second->drop();
		}

		video::IImage* toadd = img;
		bool need_to_grab = true;

		// Try to use local texture instead if asked to
		if(prefer_local){
			std::string path = getTexturePath(name.c_str());
			if(path != ""){
				video::IImage *img2 = driver->createImageFromFile(path.c_str());
				if(img2){
					toadd = img2;
					need_to_grab = false;
				}
			}
		}

		if (need_to_grab)
			toadd->grab();
		m_images[name] = toadd;
	}
	video::IImage* get(const std::string &name)
	{
		std::map<std::string, video::IImage*>::iterator n;
		n = m_images.find(name);
		if(n != m_images.end())
			return n->second;
		return NULL;
	}
	// Primarily fetches from cache, secondarily tries to read from filesystem
	video::IImage* getOrLoad(const std::string &name, IrrlichtDevice *device)
	{
		std::map<std::string, video::IImage*>::iterator n;
		n = m_images.find(name);
		if(n != m_images.end()){
			n->second->grab(); // Grab for caller
			return n->second;
		}
		video::IVideoDriver* driver = device->getVideoDriver();
		std::string path = getTexturePath(name.c_str());
		if(path == ""){
			infostream<<"SourceImageCache::getOrLoad(): No path found for \""
					<<name<<"\""<<std::endl;
			return NULL;
		}
		infostream<<"SourceImageCache::getOrLoad(): Loading path \""<<path
				<<"\""<<std::endl;
		video::IImage *img = driver->createImageFromFile(path.c_str());

		if(img){
			m_images[name] = img;
			img->grab(); // Grab for caller
		}
		return img;
	}
private:
	std::map<std::string, video::IImage*> m_images;
};

/*
	TextureSource
*/

class TextureSource : public IWritableTextureSource
{
public:
	TextureSource(IrrlichtDevice *device);
	virtual ~TextureSource();

	/*
		Example case:
		Now, assume a texture with the id 1 exists, and has the name
		"stone.png^mineral1".
		Then a random thread calls getTextureId for a texture called
		"stone.png^mineral1^crack0".
		...Now, WTF should happen? Well:
		- getTextureId strips off stuff recursively from the end until
		  the remaining part is found, or nothing is left when
		  something is stripped out

		But it is slow to search for textures by names and modify them
		like that?
		- ContentFeatures is made to contain ids for the basic plain
		  textures
		- Crack textures can be slow by themselves, but the framework
		  must be fast.

		Example case #2:
		- Assume a texture with the id 1 exists, and has the name
		  "stone.png^mineral_coal.png".
		- Now getNodeTile() stumbles upon a node which uses
		  texture id 1, and determines that MATERIAL_FLAG_CRACK
		  must be applied to the tile
		- MapBlockMesh::animate() finds the MATERIAL_FLAG_CRACK and
		  has received the current crack level 0 from the client. It
		  finds out the name of the texture with getTextureName(1),
		  appends "^crack0" to it and gets a new texture id with
		  getTextureId("stone.png^mineral_coal.png^crack0").

	*/

	/*
		Gets a texture id from cache or
		- if main thread, from getTextureIdDirect
		- if other thread, adds to request queue and waits for main thread
	*/
	u32 getTextureId(const std::string &name);

	/*
		Example names:
		"stone.png"
		"stone.png^crack2"
		"stone.png^mineral_coal.png"
		"stone.png^mineral_coal.png^crack1"

		- If texture specified by name is found from cache, return the
		  cached id.
		- Otherwise generate the texture, add to cache and return id.
		  Recursion is used to find out the largest found part of the
		  texture and continue based on it.

		The id 0 points to a NULL texture. It is returned in case of error.
	*/
	u32 getTextureIdDirect(const std::string &name);

	// Finds out the name of a cached texture.
	std::string getTextureName(u32 id);

	/*
		If texture specified by the name pointed by the id doesn't
		exist, create it, then return the cached texture.

		Can be called from any thread. If called from some other thread
		and not found in cache, the call is queued to the main thread
		for processing.
	*/
	video::ITexture* getTexture(u32 id);

	video::ITexture* getTexture(const std::string &name, u32 *id);

	TextureInfo* getTextureInfo(u32 id);

	// Returns a pointer to the irrlicht device
	virtual IrrlichtDevice* getDevice()
	{
		return m_device;
	}

	bool isKnownSourceImage(const std::string &name)
	{
		bool is_known = false;
		bool cache_found = m_source_image_existence.get(name, &is_known);
		if(cache_found)
			return is_known;
		// Not found in cache; find out if a local file exists
		is_known = (getTexturePath(name) != "");
		m_source_image_existence.set(name, is_known);
		return is_known;
	}

	// Processes queued texture requests from other threads.
	// Shall be called from the main thread.
	void processQueue();

	// Insert an image into the cache without touching the filesystem.
	// Shall be called from the main thread.
	void insertSourceImage(const std::string &name, video::IImage *img);

	// Rebuild images and textures from the current set of source images
	// Shall be called from the main thread.
	void rebuildImagesAndTextures();

	// Render a mesh to a texture.
	// Returns NULL if render-to-texture failed.
	// Shall be called from the main thread.
	video::ITexture* generateTextureFromMesh(
			const TextureFromMeshParams &params);

	// Generates an image from a full string like
	// "stone.png^mineral_coal.png^[crack:1:0".
	// Shall be called from the main thread.
	video::IImage* generateImageFromScratch(std::string name);

	// Generate image based on a string like "stone.png" or "[crack:1:0".
	// if baseimg is NULL, it is created. Otherwise stuff is made on it.
	// Shall be called from the main thread.
	bool generateImage(std::string part_of_name, video::IImage *& baseimg);

private:

	// The id of the thread that is allowed to use irrlicht directly
	threadid_t m_main_thread;
	// The irrlicht device
	IrrlichtDevice *m_device;

	// Cache of source images
	// This should be only accessed from the main thread
	SourceImageCache m_sourcecache;

	// Thread-safe cache of what source images are known (true = known)
	MutexedMap<std::string, bool> m_source_image_existence;

	// A texture id is index in this array.
	// The first position contains a NULL texture.
	std::vector<TextureInfo> m_textureinfo_cache;
	// Maps a texture name to an index in the former.
	std::map<std::string, u32> m_name_to_id;
	// The two former containers are behind this mutex
	JMutex m_textureinfo_cache_mutex;

	// Queued texture fetches (to be processed by the main thread)
	RequestQueue<std::string, u32, u8, u8> m_get_texture_queue;

	// Textures that have been overwritten with other ones
	// but can't be deleted because the ITexture* might still be used
	std::list<video::ITexture*> m_texture_trash;

	// Cached settings needed for making textures from meshes
	bool m_setting_trilinear_filter;
	bool m_setting_bilinear_filter;
	bool m_setting_anisotropic_filter;
};

IWritableTextureSource* createTextureSource(IrrlichtDevice *device)
{
	return new TextureSource(device);
}

TextureSource::TextureSource(IrrlichtDevice *device):
		m_device(device)
{
	assert(m_device);

	m_main_thread = get_current_thread_id();

	// Add a NULL TextureInfo as the first index, named ""
	m_textureinfo_cache.push_back(TextureInfo(""));
	m_name_to_id[""] = 0;

	// Cache some settings
	// Note: Since this is only done once, the game must be restarted
	// for these settings to take effect
	m_setting_trilinear_filter = g_settings->getBool("trilinear_filter");
	m_setting_bilinear_filter = g_settings->getBool("bilinear_filter");
	m_setting_anisotropic_filter = g_settings->getBool("anisotropic_filter");
}

TextureSource::~TextureSource()
{
	video::IVideoDriver* driver = m_device->getVideoDriver();

	unsigned int textures_before = driver->getTextureCount();

	for (std::vector<TextureInfo>::iterator iter =
			m_textureinfo_cache.begin();
			iter != m_textureinfo_cache.end(); iter++)
	{
		//cleanup texture
		if (iter->texture)
			driver->removeTexture(iter->texture);

		//cleanup source image
		if (iter->img)
			iter->img->drop();
	}
	m_textureinfo_cache.clear();

	for (std::list<video::ITexture*>::iterator iter =
			m_texture_trash.begin(); iter != m_texture_trash.end();
			iter++)
	{
		video::ITexture *t = *iter;

		//cleanup trashed texture
		driver->removeTexture(t);
	}

	infostream << "~TextureSource() "<< textures_before << "/"
			<< driver->getTextureCount() << std::endl;
}

u32 TextureSource::getTextureId(const std::string &name)
{
	//infostream<<"getTextureId(): \""<<name<<"\""<<std::endl;

	{
		/*
			See if texture already exists
		*/
		JMutexAutoLock lock(m_textureinfo_cache_mutex);
		std::map<std::string, u32>::iterator n;
		n = m_name_to_id.find(name);
		if(n != m_name_to_id.end())
		{
			return n->second;
		}
	}

	/*
		Get texture
	*/
	if(get_current_thread_id() == m_main_thread)
	{
		return getTextureIdDirect(name);
	}
	else
	{
		infostream<<"getTextureId(): Queued: name=\""<<name<<"\""<<std::endl;

		// We're gonna ask the result to be put into here
		static ResultQueue<std::string, u32, u8, u8> result_queue;

		// Throw a request in
		m_get_texture_queue.add(name, 0, 0, &result_queue);

		/*infostream<<"Waiting for texture from main thread, name=\""
				<<name<<"\""<<std::endl;*/

		try
		{
			while(true) {
				// Wait result for a second
				GetResult<std::string, u32, u8, u8>
					result = result_queue.pop_front(1000);

				if (result.key == name) {
					return result.item;
				}
			}
		}
		catch(ItemNotFoundException &e)
		{
			errorstream<<"Waiting for texture " << name << " timed out."<<std::endl;
			return 0;
		}
	}

	infostream<<"getTextureId(): Failed"<<std::endl;

	return 0;
}

// Draw an image on top of an another one, using the alpha channel of the
// source image
static void blit_with_alpha(video::IImage *src, video::IImage *dst,
		v2s32 src_pos, v2s32 dst_pos, v2u32 size);

// Like blit_with_alpha, but only modifies destination pixels that
// are fully opaque
static void blit_with_alpha_overlay(video::IImage *src, video::IImage *dst,
		v2s32 src_pos, v2s32 dst_pos, v2u32 size);

// Draw or overlay a crack
static void draw_crack(video::IImage *crack, video::IImage *dst,
		bool use_overlay, s32 frame_count, s32 progression,
		video::IVideoDriver *driver);

// Brighten image
void brighten(video::IImage *image);
// Parse a transform name
u32 parseImageTransform(const std::string& s);
// Apply transform to image dimension
core::dimension2d<u32> imageTransformDimension(u32 transform, core::dimension2d<u32> dim);
// Apply transform to image data
void imageTransform(u32 transform, video::IImage *src, video::IImage *dst);

/*
	This method generates all the textures
*/
u32 TextureSource::getTextureIdDirect(const std::string &name)
{
	//infostream<<"getTextureIdDirect(): name=\""<<name<<"\""<<std::endl;

	// Empty name means texture 0
	if(name == "")
	{
		infostream<<"getTextureIdDirect(): name is empty"<<std::endl;
		return 0;
	}

	/*
		Calling only allowed from main thread
	*/
	if(get_current_thread_id() != m_main_thread)
	{
		errorstream<<"TextureSource::getTextureIdDirect() "
				"called not from main thread"<<std::endl;
		return 0;
	}

	/*
		See if texture already exists
	*/
	{
		JMutexAutoLock lock(m_textureinfo_cache_mutex);

		std::map<std::string, u32>::iterator n;
		n = m_name_to_id.find(name);
		if(n != m_name_to_id.end())
		{
			/*infostream<<"getTextureIdDirect(): \""<<name
					<<"\" found in cache"<<std::endl;*/
			return n->second;
		}
	}

	/*infostream<<"getTextureIdDirect(): \""<<name
			<<"\" NOT found in cache. Creating it."<<std::endl;*/

	/*
		Get the base image
	*/

	char separator = '^';

	/*
		This is set to the id of the base image.
		If left 0, there is no base image and a completely new image
		is made.
	*/
	u32 base_image_id = 0;

	// Find last meta separator in name
	s32 last_separator_position = -1;
	for(s32 i=name.size()-1; i>=0; i--)
	{
		if(name[i] == separator)
		{
			last_separator_position = i;
			break;
		}
	}
	/*
		If separator was found, construct the base name and make the
		base image using a recursive call
	*/
	std::string base_image_name;
	if(last_separator_position != -1)
	{
		// Construct base name
		base_image_name = name.substr(0, last_separator_position);
		/*infostream<<"getTextureIdDirect(): Calling itself recursively"
				" to get base image of \""<<name<<"\" = \""
                <<base_image_name<<"\""<<std::endl;*/
		base_image_id = getTextureIdDirect(base_image_name);
	}

	//infostream<<"base_image_id="<<base_image_id<<std::endl;

	video::IVideoDriver* driver = m_device->getVideoDriver();
	assert(driver);

	video::ITexture *t = NULL;

	/*
		An image will be built from files and then converted into a texture.
	*/
	video::IImage *baseimg = NULL;

	// If a base image was found, copy it to baseimg
	if(base_image_id != 0)
	{
		JMutexAutoLock lock(m_textureinfo_cache_mutex);

		TextureInfo *ti = &m_textureinfo_cache[base_image_id];

		if(ti->img == NULL)
		{
			infostream<<"getTextureIdDirect(): WARNING: NULL image in "
					<<"cache: \""<<base_image_name<<"\""
					<<std::endl;
		}
		else
		{
			core::dimension2d<u32> dim = ti->img->getDimension();

			baseimg = driver->createImage(video::ECF_A8R8G8B8, dim);

			ti->img->copyTo(
					baseimg, // target
					v2s32(0,0), // position in target
					core::rect<s32>(v2s32(0,0), dim) // from
			);

			/*infostream<<"getTextureIdDirect(): Loaded \""
					<<base_image_name<<"\" from image cache"
					<<std::endl;*/
		}
	}

	/*
		Parse out the last part of the name of the image and act
		according to it
	*/

	std::string last_part_of_name = name.substr(last_separator_position+1);
	//infostream<<"last_part_of_name=\""<<last_part_of_name<<"\""<<std::endl;

	// Generate image according to part of name
	if(!generateImage(last_part_of_name, baseimg))
	{
		errorstream<<"getTextureIdDirect(): "
				"failed to generate \""<<last_part_of_name<<"\""
				<<std::endl;
	}

	// If no resulting image, print a warning
	if(baseimg == NULL)
	{
		errorstream<<"getTextureIdDirect(): baseimg is NULL (attempted to"
				" create texture \""<<name<<"\""<<std::endl;
	}

	if(baseimg != NULL)
	{
		// Create texture from resulting image
		t = driver->addTexture(name.c_str(), baseimg);
	}

	/*
		Add texture to caches (add NULL textures too)
	*/

	JMutexAutoLock lock(m_textureinfo_cache_mutex);

	u32 id = m_textureinfo_cache.size();
	TextureInfo ti(name, t, baseimg);
	m_textureinfo_cache.push_back(ti);
	m_name_to_id[name] = id;

	return id;
}

std::string TextureSource::getTextureName(u32 id)
{
	JMutexAutoLock lock(m_textureinfo_cache_mutex);

	if(id >= m_textureinfo_cache.size())
	{
		errorstream<<"TextureSource::getTextureName(): id="<<id
				<<" >= m_textureinfo_cache.size()="
				<<m_textureinfo_cache.size()<<std::endl;
		return "";
	}

	return m_textureinfo_cache[id].name;
}

video::ITexture* TextureSource::getTexture(u32 id)
{
	JMutexAutoLock lock(m_textureinfo_cache_mutex);

	if(id >= m_textureinfo_cache.size())
		return NULL;

	return m_textureinfo_cache[id].texture;
}

video::ITexture* TextureSource::getTexture(const std::string &name, u32 *id)
{
	u32 actual_id = getTextureId(name);
	if(id){
		*id = actual_id;
	}
	return getTexture(actual_id);
}

TextureInfo * TextureSource::getTextureInfo(u32 id)
{
	JMutexAutoLock lock(m_textureinfo_cache_mutex);

	if(id >= m_textureinfo_cache.size())
		return NULL;

	return &m_textureinfo_cache[id];
}

void TextureSource::processQueue()
{
	/*
		Fetch textures
	*/
	//NOTE this is only thread safe for ONE consumer thread!
	if(!m_get_texture_queue.empty())
	{
		GetRequest<std::string, u32, u8, u8>
				request = m_get_texture_queue.pop();

		/*infostream<<"TextureSource::processQueue(): "
				<<"got texture request with "
				<<"name=\""<<request.key<<"\""
				<<std::endl;*/

		m_get_texture_queue.pushResult(request,getTextureIdDirect(request.key));
	}
}

void TextureSource::insertSourceImage(const std::string &name, video::IImage *img)
{
	//infostream<<"TextureSource::insertSourceImage(): name="<<name<<std::endl;

	assert(get_current_thread_id() == m_main_thread);

	m_sourcecache.insert(name, img, true, m_device->getVideoDriver());
	m_source_image_existence.set(name, true);
}

void TextureSource::rebuildImagesAndTextures()
{
	JMutexAutoLock lock(m_textureinfo_cache_mutex);

	video::IVideoDriver* driver = m_device->getVideoDriver();

	// Recreate textures
	for(u32 i=0; i<m_textureinfo_cache.size(); i++){
		TextureInfo *ti = &m_textureinfo_cache[i];
		video::IImage *img = generateImageFromScratch(ti->name);
		// Create texture from resulting image
		video::ITexture *t = NULL;
		if(img)
			t = driver->addTexture(ti->name.c_str(), img);
		video::ITexture *t_old = ti->texture;
		// Replace texture
		ti->texture = t;
		ti->img = img;

		if (t_old != 0)
			m_texture_trash.push_back(t_old);
	}
}

video::ITexture* TextureSource::generateTextureFromMesh(
		const TextureFromMeshParams &params)
{
	video::IVideoDriver *driver = m_device->getVideoDriver();
	assert(driver);

	if(driver->queryFeature(video::EVDF_RENDER_TO_TARGET) == false)
	{
		static bool warned = false;
		if(!warned)
		{
			errorstream<<"TextureSource::generateTextureFromMesh(): "
				<<"EVDF_RENDER_TO_TARGET not supported."<<std::endl;
			warned = true;
		}
		return NULL;
	}

	// Create render target texture
	video::ITexture *rtt = driver->addRenderTargetTexture(
			params.dim, params.rtt_texture_name.c_str(),
			video::ECF_A8R8G8B8);
	if(rtt == NULL)
	{
		errorstream<<"TextureSource::generateTextureFromMesh(): "
			<<"addRenderTargetTexture returned NULL."<<std::endl;
		return NULL;
	}

	// Set render target
	driver->setRenderTarget(rtt, false, true, video::SColor(0,0,0,0));

	// Get a scene manager
	scene::ISceneManager *smgr_main = m_device->getSceneManager();
	assert(smgr_main);
	scene::ISceneManager *smgr = smgr_main->createNewSceneManager();
	assert(smgr);

	scene::IMeshSceneNode* meshnode = smgr->addMeshSceneNode(params.mesh, NULL, -1, v3f(0,0,0), v3f(0,0,0), v3f(1,1,1), true);
	meshnode->setMaterialFlag(video::EMF_LIGHTING, true);
	meshnode->setMaterialFlag(video::EMF_ANTI_ALIASING, true);
	meshnode->setMaterialFlag(video::EMF_TRILINEAR_FILTER, m_setting_trilinear_filter);
	meshnode->setMaterialFlag(video::EMF_BILINEAR_FILTER, m_setting_bilinear_filter);
	meshnode->setMaterialFlag(video::EMF_ANISOTROPIC_FILTER, m_setting_anisotropic_filter);

	scene::ICameraSceneNode* camera = smgr->addCameraSceneNode(0,
			params.camera_position, params.camera_lookat);
	// second parameter of setProjectionMatrix (isOrthogonal) is ignored
	camera->setProjectionMatrix(params.camera_projection_matrix, false);

	smgr->setAmbientLight(params.ambient_light);
	smgr->addLightSceneNode(0,
			params.light_position,
			params.light_color,
			params.light_radius);

	// Wield light shouldn't be used here
	bool disable_wieldlight = g_settings->getBool("disable_wieldlight");
	g_settings->setBool("disable_wieldlight", true);

	// Render scene
	driver->beginScene(true, true, video::SColor(0,0,0,0));
	smgr->drawAll();
	driver->endScene();
	
	g_settings->setBool("disable_wieldlight", disable_wieldlight);

	// NOTE: The scene nodes should not be dropped, otherwise
	//       smgr->drop() segfaults
	/*cube->drop();
	camera->drop();
	light->drop();*/
	// Drop scene manager
	smgr->drop();

	// Unset render target
	driver->setRenderTarget(0, false, true, 0);

	if(params.delete_texture_on_shutdown)
		m_texture_trash.push_back(rtt);

	return rtt;
}

video::IImage* TextureSource::generateImageFromScratch(std::string name)
{
	/*infostream<<"generateImageFromScratch(): "
			"\""<<name<<"\""<<std::endl;*/

	video::IVideoDriver *driver = m_device->getVideoDriver();
	assert(driver);

	/*
		Get the base image
	*/

	video::IImage *baseimg = NULL;

	char separator = '^';

	// Find last meta separator in name
	s32 last_separator_position = name.find_last_of(separator);

	/*
		If separator was found, construct the base name and make the
		base image using a recursive call
	*/
	std::string base_image_name;
	if(last_separator_position != -1)
	{
		// Construct base name
		base_image_name = name.substr(0, last_separator_position);
		baseimg = generateImageFromScratch(base_image_name);
	}

	/*
		Parse out the last part of the name of the image and act
		according to it
	*/

	std::string last_part_of_name = name.substr(last_separator_position+1);

	// Generate image according to part of name
	if(!generateImage(last_part_of_name, baseimg))
	{
		errorstream<<"generateImageFromScratch(): "
				"failed to generate \""<<last_part_of_name<<"\""
				<<std::endl;
		return NULL;
	}

	return baseimg;
}

bool TextureSource::generateImage(std::string part_of_name, video::IImage *& baseimg)
{
	video::IVideoDriver* driver = m_device->getVideoDriver();
	assert(driver);

	// Stuff starting with [ are special commands
	if(part_of_name.size() == 0 || part_of_name[0] != '[')
	{
		video::IImage *image = m_sourcecache.getOrLoad(part_of_name, m_device);

		if (image != NULL) {
			if (!driver->queryFeature(irr::video::EVDF_TEXTURE_NPOT)) {
				core::dimension2d<u32> dim = image->getDimension();


				if ((dim.Height %2 != 0) ||
						(dim.Width %2 != 0)) {
					errorstream << "TextureSource::generateImage "
							<< part_of_name << " size npot2 x=" << dim.Width
							<< " y=" << dim.Height << std::endl;
				}
			}
		}

		if (image == NULL) {
			if (part_of_name != "") {
				if (part_of_name.find("_normal.png") == std::string::npos){			
					errorstream<<"generateImage(): Could not load image \""
						<<part_of_name<<"\""<<" while building texture"<<std::endl;
					errorstream<<"generateImage(): Creating a dummy"
						<<" image for \""<<part_of_name<<"\""<<std::endl;
				} else {
					infostream<<"generateImage(): Could not load normal map \""
						<<part_of_name<<"\""<<std::endl;
					infostream<<"generateImage(): Creating a dummy"
						<<" normal map for \""<<part_of_name<<"\""<<std::endl;
				}
			}

			// Just create a dummy image
			//core::dimension2d<u32> dim(2,2);
			core::dimension2d<u32> dim(1,1);
			image = driver->createImage(video::ECF_A8R8G8B8, dim);
			assert(image);
			/*image->setPixel(0,0, video::SColor(255,255,0,0));
			image->setPixel(1,0, video::SColor(255,0,255,0));
			image->setPixel(0,1, video::SColor(255,0,0,255));
			image->setPixel(1,1, video::SColor(255,255,0,255));*/
			image->setPixel(0,0, video::SColor(255,myrand()%256,
					myrand()%256,myrand()%256));
			/*image->setPixel(1,0, video::SColor(255,myrand()%256,
					myrand()%256,myrand()%256));
			image->setPixel(0,1, video::SColor(255,myrand()%256,
					myrand()%256,myrand()%256));
			image->setPixel(1,1, video::SColor(255,myrand()%256,
					myrand()%256,myrand()%256));*/
		}

		// If base image is NULL, load as base.
		if(baseimg == NULL)
		{
			//infostream<<"Setting "<<part_of_name<<" as base"<<std::endl;
			/*
				Copy it this way to get an alpha channel.
				Otherwise images with alpha cannot be blitted on
				images that don't have alpha in the original file.
			*/
			core::dimension2d<u32> dim = image->getDimension();
			baseimg = driver->createImage(video::ECF_A8R8G8B8, dim);
			image->copyTo(baseimg);
		}
		// Else blit on base.
		else
		{
			//infostream<<"Blitting "<<part_of_name<<" on base"<<std::endl;
			// Size of the copied area
			core::dimension2d<u32> dim = image->getDimension();
			//core::dimension2d<u32> dim(16,16);
			// Position to copy the blitted to in the base image
			core::position2d<s32> pos_to(0,0);
			// Position to copy the blitted from in the blitted image
			core::position2d<s32> pos_from(0,0);
			// Blit
			/*image->copyToWithAlpha(baseimg, pos_to,
					core::rect<s32>(pos_from, dim),
					video::SColor(255,255,255,255),
					NULL);*/
			blit_with_alpha(image, baseimg, pos_from, pos_to, dim);
		}
		//cleanup
		image->drop();
	}
	else
	{
		// A special texture modification

		/*infostream<<"generateImage(): generating special "
				<<"modification \""<<part_of_name<<"\""
				<<std::endl;*/

		/*
			[crack:N:P
			[cracko:N:P
			Adds a cracking texture
			N = animation frame count, P = crack progression
		*/
		if(part_of_name.substr(0,6) == "[crack")
		{
			if(baseimg == NULL)
			{
				errorstream<<"generateImage(): baseimg==NULL "
						<<"for part_of_name=\""<<part_of_name
						<<"\", cancelling."<<std::endl;
				return false;
			}

			// Crack image number and overlay option
			bool use_overlay = (part_of_name[6] == 'o');
			Strfnd sf(part_of_name);
			sf.next(":");
			s32 frame_count = stoi(sf.next(":"));
			s32 progression = stoi(sf.next(":"));

			/*
				Load crack image.

				It is an image with a number of cracking stages
				horizontally tiled.
			*/
			video::IImage *img_crack = m_sourcecache.getOrLoad(
					"crack_anylength.png", m_device);

			if(img_crack && progression >= 0)
			{
				draw_crack(img_crack, baseimg,
						use_overlay, frame_count,
						progression, driver);
				img_crack->drop();
			}
		}
		/*
			[combine:WxH:X,Y=filename:X,Y=filename2
			Creates a bigger texture from an amount of smaller ones
		*/
		else if(part_of_name.substr(0,8) == "[combine")
		{
			Strfnd sf(part_of_name);
			sf.next(":");
			u32 w0 = stoi(sf.next("x"));
			u32 h0 = stoi(sf.next(":"));
			infostream<<"combined w="<<w0<<" h="<<h0<<std::endl;
			core::dimension2d<u32> dim(w0,h0);
			if(baseimg == NULL)
			{
				baseimg = driver->createImage(video::ECF_A8R8G8B8, dim);
				baseimg->fill(video::SColor(0,0,0,0));
			}
			while(sf.atend() == false)
			{
				u32 x = stoi(sf.next(","));
				u32 y = stoi(sf.next("="));
				std::string filename = sf.next(":");
				infostream<<"Adding \""<<filename
						<<"\" to combined ("<<x<<","<<y<<")"
						<<std::endl;
				video::IImage *img = m_sourcecache.getOrLoad(filename, m_device);
				if(img)
				{
					core::dimension2d<u32> dim = img->getDimension();
					infostream<<"Size "<<dim.Width
							<<"x"<<dim.Height<<std::endl;
					core::position2d<s32> pos_base(x, y);
					video::IImage *img2 =
							driver->createImage(video::ECF_A8R8G8B8, dim);
					img->copyTo(img2);
					img->drop();
					/*img2->copyToWithAlpha(baseimg, pos_base,
							core::rect<s32>(v2s32(0,0), dim),
							video::SColor(255,255,255,255),
							NULL);*/
					blit_with_alpha(img2, baseimg, v2s32(0,0), pos_base, dim);
					img2->drop();
				}
				else
				{
					infostream<<"img==NULL"<<std::endl;
				}
			}
		}
		/*
			"[brighten"
		*/
		else if(part_of_name.substr(0,9) == "[brighten")
		{
			if(baseimg == NULL)
			{
				errorstream<<"generateImage(): baseimg==NULL "
						<<"for part_of_name=\""<<part_of_name
						<<"\", cancelling."<<std::endl;
				return false;
			}

			brighten(baseimg);
		}
		/*
			"[noalpha"
			Make image completely opaque.
			Used for the leaves texture when in old leaves mode, so
			that the transparent parts don't look completely black
			when simple alpha channel is used for rendering.
		*/
		else if(part_of_name.substr(0,8) == "[noalpha")
		{
			if(baseimg == NULL)
			{
				errorstream<<"generateImage(): baseimg==NULL "
						<<"for part_of_name=\""<<part_of_name
						<<"\", cancelling."<<std::endl;
				return false;
			}

			core::dimension2d<u32> dim = baseimg->getDimension();

			// Set alpha to full
			for(u32 y=0; y<dim.Height; y++)
			for(u32 x=0; x<dim.Width; x++)
			{
				video::SColor c = baseimg->getPixel(x,y);
				c.setAlpha(255);
				baseimg->setPixel(x,y,c);
			}
		}
		/*
			"[makealpha:R,G,B"
			Convert one color to transparent.
		*/
		else if(part_of_name.substr(0,11) == "[makealpha:")
		{
			if(baseimg == NULL)
			{
				errorstream<<"generateImage(): baseimg==NULL "
						<<"for part_of_name=\""<<part_of_name
						<<"\", cancelling."<<std::endl;
				return false;
			}

			Strfnd sf(part_of_name.substr(11));
			u32 r1 = stoi(sf.next(","));
			u32 g1 = stoi(sf.next(","));
			u32 b1 = stoi(sf.next(""));
			std::string filename = sf.next("");

			core::dimension2d<u32> dim = baseimg->getDimension();

			/*video::IImage *oldbaseimg = baseimg;
			baseimg = driver->createImage(video::ECF_A8R8G8B8, dim);
			oldbaseimg->copyTo(baseimg);
			oldbaseimg->drop();*/

			// Set alpha to full
			for(u32 y=0; y<dim.Height; y++)
			for(u32 x=0; x<dim.Width; x++)
			{
				video::SColor c = baseimg->getPixel(x,y);
				u32 r = c.getRed();
				u32 g = c.getGreen();
				u32 b = c.getBlue();
				if(!(r == r1 && g == g1 && b == b1))
					continue;
				c.setAlpha(0);
				baseimg->setPixel(x,y,c);
			}
		}
		/*
			"[transformN"
			Rotates and/or flips the image.

			N can be a number (between 0 and 7) or a transform name.
			Rotations are counter-clockwise.
			0  I      identity
			1  R90    rotate by 90 degrees
			2  R180   rotate by 180 degrees
			3  R270   rotate by 270 degrees
			4  FX     flip X
			5  FXR90  flip X then rotate by 90 degrees
			6  FY     flip Y
			7  FYR90  flip Y then rotate by 90 degrees

			Note: Transform names can be concatenated to produce
			their product (applies the first then the second).
			The resulting transform will be equivalent to one of the
			eight existing ones, though (see: dihedral group).
		*/
		else if(part_of_name.substr(0,10) == "[transform")
		{
			if(baseimg == NULL)
			{
				errorstream<<"generateImage(): baseimg==NULL "
						<<"for part_of_name=\""<<part_of_name
						<<"\", cancelling."<<std::endl;
				return false;
			}

			u32 transform = parseImageTransform(part_of_name.substr(10));
			core::dimension2d<u32> dim = imageTransformDimension(
					transform, baseimg->getDimension());
			video::IImage *image = driver->createImage(
					baseimg->getColorFormat(), dim);
			assert(image);
			imageTransform(transform, baseimg, image);
			baseimg->drop();
			baseimg = image;
		}
		/*
			[inventorycube{topimage{leftimage{rightimage
			In every subimage, replace ^ with &.
			Create an "inventory cube".
			NOTE: This should be used only on its own.
			Example (a grass block (not actually used in game):
			"[inventorycube{grass.png{mud.png&grass_side.png{mud.png&grass_side.png"
		*/
		else if(part_of_name.substr(0,14) == "[inventorycube")
		{
			if(baseimg != NULL)
			{
				errorstream<<"generateImage(): baseimg!=NULL "
						<<"for part_of_name=\""<<part_of_name
						<<"\", cancelling."<<std::endl;
				return false;
			}

			str_replace_char(part_of_name, '&', '^');
			Strfnd sf(part_of_name);
			sf.next("{");
			std::string imagename_top = sf.next("{");
			std::string imagename_left = sf.next("{");
			std::string imagename_right = sf.next("{");

			// Generate images for the faces of the cube
			video::IImage *img_top =
				generateImageFromScratch(imagename_top);
			video::IImage *img_left =
				generateImageFromScratch(imagename_left);
			video::IImage *img_right =
				generateImageFromScratch(imagename_right);
			assert(img_top && img_left && img_right);

			// Create textures from images
			video::ITexture *texture_top = driver->addTexture(
					(imagename_top + "__temp__").c_str(), img_top);
			video::ITexture *texture_left = driver->addTexture(
					(imagename_left + "__temp__").c_str(), img_left);
			video::ITexture *texture_right = driver->addTexture(
					(imagename_right + "__temp__").c_str(), img_right);
			assert(texture_top && texture_left && texture_right);

			// Drop images
			img_top->drop();
			img_left->drop();
			img_right->drop();

			/*
				Draw a cube mesh into a render target texture
			*/
			scene::IMesh* cube = createCubeMesh(v3f(1, 1, 1));
			setMeshColor(cube, video::SColor(255, 255, 255, 255));
			cube->getMeshBuffer(0)->getMaterial().setTexture(0, texture_top);
			cube->getMeshBuffer(1)->getMaterial().setTexture(0, texture_top);
			cube->getMeshBuffer(2)->getMaterial().setTexture(0, texture_right);
			cube->getMeshBuffer(3)->getMaterial().setTexture(0, texture_right);
			cube->getMeshBuffer(4)->getMaterial().setTexture(0, texture_left);
			cube->getMeshBuffer(5)->getMaterial().setTexture(0, texture_left);

			TextureFromMeshParams params;
			params.mesh = cube;
			params.dim.set(64, 64);
			params.rtt_texture_name = part_of_name + "_RTT";
			// We will delete the rtt texture ourselves
			params.delete_texture_on_shutdown = false;
			params.camera_position.set(0, 1.0, -1.5);
			params.camera_position.rotateXZBy(45);
			params.camera_lookat.set(0, 0, 0);
			// Set orthogonal projection
			params.camera_projection_matrix.buildProjectionMatrixOrthoLH(
					1.65, 1.65, 0, 100);

			params.ambient_light.set(1.0, 0.2, 0.2, 0.2);
			params.light_position.set(10, 100, -50);
			params.light_color.set(1.0, 0.5, 0.5, 0.5);
			params.light_radius = 1000;

			video::ITexture *rtt = generateTextureFromMesh(params);

			// Drop mesh
			cube->drop();

			// Free textures of images
			driver->removeTexture(texture_top);
			driver->removeTexture(texture_left);
			driver->removeTexture(texture_right);

			if(rtt == NULL)
			{
				baseimg = generateImageFromScratch(imagename_top);
				return true;
			}

			// Create image of render target
			video::IImage *image = driver->createImage(rtt, v2s32(0,0), params.dim);
			assert(image);

			// Cleanup texture
			driver->removeTexture(rtt);

			baseimg = driver->createImage(video::ECF_A8R8G8B8, params.dim);

			if(image)
			{
				image->copyTo(baseimg);
				image->drop();
			}
		}
		/*
			[lowpart:percent:filename
			Adds the lower part of a texture
		*/
		else if(part_of_name.substr(0,9) == "[lowpart:")
		{
			Strfnd sf(part_of_name);
			sf.next(":");
			u32 percent = stoi(sf.next(":"));
			std::string filename = sf.next(":");
			//infostream<<"power part "<<percent<<"%% of "<<filename<<std::endl;

			if(baseimg == NULL)
				baseimg = driver->createImage(video::ECF_A8R8G8B8, v2u32(16,16));
			video::IImage *img = m_sourcecache.getOrLoad(filename, m_device);
			if(img)
			{
				core::dimension2d<u32> dim = img->getDimension();
				core::position2d<s32> pos_base(0, 0);
				video::IImage *img2 =
						driver->createImage(video::ECF_A8R8G8B8, dim);
				img->copyTo(img2);
				img->drop();
				core::position2d<s32> clippos(0, 0);
				clippos.Y = dim.Height * (100-percent) / 100;
				core::dimension2d<u32> clipdim = dim;
				clipdim.Height = clipdim.Height * percent / 100 + 1;
				core::rect<s32> cliprect(clippos, clipdim);
				img2->copyToWithAlpha(baseimg, pos_base,
						core::rect<s32>(v2s32(0,0), dim),
						video::SColor(255,255,255,255),
						&cliprect);
				img2->drop();
			}
		}
		/*
			[verticalframe:N:I
			Crops a frame of a vertical animation.
			N = frame count, I = frame index
		*/
		else if(part_of_name.substr(0,15) == "[verticalframe:")
		{
			Strfnd sf(part_of_name);
			sf.next(":");
			u32 frame_count = stoi(sf.next(":"));
			u32 frame_index = stoi(sf.next(":"));

			if(baseimg == NULL){
				errorstream<<"generateImage(): baseimg!=NULL "
						<<"for part_of_name=\""<<part_of_name
						<<"\", cancelling."<<std::endl;
				return false;
			}

			v2u32 frame_size = baseimg->getDimension();
			if (frame_count)
			frame_size.Y /= frame_count;

			video::IImage *img = driver->createImage(video::ECF_A8R8G8B8,
					frame_size);
			if(!img){
				errorstream<<"generateImage(): Could not create image "
						<<"for part_of_name=\""<<part_of_name
						<<"\", cancelling."<<std::endl;
				return false;
			}

			// Fill target image with transparency
			img->fill(video::SColor(0,0,0,0));

			core::dimension2d<u32> dim = frame_size;
			core::position2d<s32> pos_dst(0, 0);
			core::position2d<s32> pos_src(0, frame_index * frame_size.Y);
			baseimg->copyToWithAlpha(img, pos_dst,
					core::rect<s32>(pos_src, dim),
					video::SColor(255,255,255,255),
					NULL);
			// Replace baseimg
			baseimg->drop();
			baseimg = img;
		}
		else
		{
			errorstream<<"generateImage(): Invalid "
					" modification: \""<<part_of_name<<"\""<<std::endl;
		}
	}

	return true;
}

/*
	Draw an image on top of an another one, using the alpha channel of the
	source image

	This exists because IImage::copyToWithAlpha() doesn't seem to always
	work.
*/
static void blit_with_alpha(video::IImage *src, video::IImage *dst,
		v2s32 src_pos, v2s32 dst_pos, v2u32 size)
{
	for(u32 y0=0; y0<size.Y; y0++)
	for(u32 x0=0; x0<size.X; x0++)
	{
		s32 src_x = src_pos.X + x0;
		s32 src_y = src_pos.Y + y0;
		s32 dst_x = dst_pos.X + x0;
		s32 dst_y = dst_pos.Y + y0;
		video::SColor src_c = src->getPixel(src_x, src_y);
		video::SColor dst_c = dst->getPixel(dst_x, dst_y);
		dst_c = src_c.getInterpolated(dst_c, (float)src_c.getAlpha()/255.0f);
		dst->setPixel(dst_x, dst_y, dst_c);
	}
}

/*
	Draw an image on top of an another one, using the alpha channel of the
	source image; only modify fully opaque pixels in destinaion
*/
static void blit_with_alpha_overlay(video::IImage *src, video::IImage *dst,
		v2s32 src_pos, v2s32 dst_pos, v2u32 size)
{
	for(u32 y0=0; y0<size.Y; y0++)
	for(u32 x0=0; x0<size.X; x0++)
	{
		s32 src_x = src_pos.X + x0;
		s32 src_y = src_pos.Y + y0;
		s32 dst_x = dst_pos.X + x0;
		s32 dst_y = dst_pos.Y + y0;
		video::SColor src_c = src->getPixel(src_x, src_y);
		video::SColor dst_c = dst->getPixel(dst_x, dst_y);
		if(dst_c.getAlpha() == 255 && src_c.getAlpha() != 0)
		{
			dst_c = src_c.getInterpolated(dst_c, (float)src_c.getAlpha()/255.0f);
			dst->setPixel(dst_x, dst_y, dst_c);
		}
	}
}

static void draw_crack(video::IImage *crack, video::IImage *dst,
		bool use_overlay, s32 frame_count, s32 progression,
		video::IVideoDriver *driver)
{
	// Dimension of destination image
	core::dimension2d<u32> dim_dst = dst->getDimension();
	// Dimension of original image
	core::dimension2d<u32> dim_crack = crack->getDimension();
	// Count of crack stages
	s32 crack_count = dim_crack.Height / dim_crack.Width;
	// Limit frame_count
	if(frame_count > (s32) dim_dst.Height)
		frame_count = dim_dst.Height;
	if(frame_count < 1)
		frame_count = 1;
	// Limit progression
	if(progression > crack_count-1)
		progression = crack_count-1;
	// Dimension of a single crack stage
	core::dimension2d<u32> dim_crack_cropped(
		dim_crack.Width,
		dim_crack.Width
	);
	// Dimension of the scaled crack stage,
	// which is the same as the dimension of a single destination frame
	core::dimension2d<u32> dim_crack_scaled(
		dim_dst.Width,
		dim_dst.Height / frame_count
	);
	// Create cropped and scaled crack images
	video::IImage *crack_cropped = driver->createImage(
			video::ECF_A8R8G8B8, dim_crack_cropped);
	video::IImage *crack_scaled = driver->createImage(
			video::ECF_A8R8G8B8, dim_crack_scaled);

	if(crack_cropped && crack_scaled)
	{
		// Crop crack image
		v2s32 pos_crack(0, progression*dim_crack.Width);
		crack->copyTo(crack_cropped,
				v2s32(0,0),
				core::rect<s32>(pos_crack, dim_crack_cropped));
		// Scale crack image by copying
		crack_cropped->copyToScaling(crack_scaled);
		// Copy or overlay crack image onto each frame
		for(s32 i = 0; i < frame_count; ++i)
		{
			v2s32 dst_pos(0, dim_crack_scaled.Height * i);
			if(use_overlay)
			{
				blit_with_alpha_overlay(crack_scaled, dst,
						v2s32(0,0), dst_pos,
						dim_crack_scaled);
			}
			else
			{
				blit_with_alpha(crack_scaled, dst,
						v2s32(0,0), dst_pos,
						dim_crack_scaled);
			}
		}
	}

	if(crack_scaled)
		crack_scaled->drop();

	if(crack_cropped)
		crack_cropped->drop();
}

void brighten(video::IImage *image)
{
	if(image == NULL)
		return;

	core::dimension2d<u32> dim = image->getDimension();

	for(u32 y=0; y<dim.Height; y++)
	for(u32 x=0; x<dim.Width; x++)
	{
		video::SColor c = image->getPixel(x,y);
		c.setRed(0.5 * 255 + 0.5 * (float)c.getRed());
		c.setGreen(0.5 * 255 + 0.5 * (float)c.getGreen());
		c.setBlue(0.5 * 255 + 0.5 * (float)c.getBlue());
		image->setPixel(x,y,c);
	}
}

u32 parseImageTransform(const std::string& s)
{
	int total_transform = 0;

	std::string transform_names[8];
	transform_names[0] = "i";
	transform_names[1] = "r90";
	transform_names[2] = "r180";
	transform_names[3] = "r270";
	transform_names[4] = "fx";
	transform_names[6] = "fy";

	std::size_t pos = 0;
	while(pos < s.size())
	{
		int transform = -1;
		for(int i = 0; i <= 7; ++i)
		{
			const std::string &name_i = transform_names[i];

			if(s[pos] == ('0' + i))
			{
				transform = i;
				pos++;
				break;
			}
			else if(!(name_i.empty()) &&
				lowercase(s.substr(pos, name_i.size())) == name_i)
			{
				transform = i;
				pos += name_i.size();
				break;
			}
		}
		if(transform < 0)
			break;

		// Multiply total_transform and transform in the group D4
		int new_total = 0;
		if(transform < 4)
			new_total = (transform + total_transform) % 4;
		else
			new_total = (transform - total_transform + 8) % 4;
		if((transform >= 4) ^ (total_transform >= 4))
			new_total += 4;

		total_transform = new_total;
	}
	return total_transform;
}

core::dimension2d<u32> imageTransformDimension(u32 transform, core::dimension2d<u32> dim)
{
	if(transform % 2 == 0)
		return dim;
	else
		return core::dimension2d<u32>(dim.Height, dim.Width);
}

void imageTransform(u32 transform, video::IImage *src, video::IImage *dst)
{
	if(src == NULL || dst == NULL)
		return;

	core::dimension2d<u32> srcdim = src->getDimension();
	core::dimension2d<u32> dstdim = dst->getDimension();

	assert(dstdim == imageTransformDimension(transform, srcdim));
	assert(transform >= 0 && transform <= 7);

	/*
		Compute the transformation from source coordinates (sx,sy)
		to destination coordinates (dx,dy).
	*/
	int sxn = 0;
	int syn = 2;
	if(transform == 0)         // identity
		sxn = 0, syn = 2;  //   sx = dx, sy = dy
	else if(transform == 1)    // rotate by 90 degrees ccw
		sxn = 3, syn = 0;  //   sx = (H-1) - dy, sy = dx
	else if(transform == 2)    // rotate by 180 degrees
		sxn = 1, syn = 3;  //   sx = (W-1) - dx, sy = (H-1) - dy
	else if(transform == 3)    // rotate by 270 degrees ccw
		sxn = 2, syn = 1;  //   sx = dy, sy = (W-1) - dx
	else if(transform == 4)    // flip x
		sxn = 1, syn = 2;  //   sx = (W-1) - dx, sy = dy
	else if(transform == 5)    // flip x then rotate by 90 degrees ccw
		sxn = 2, syn = 0;  //   sx = dy, sy = dx
	else if(transform == 6)    // flip y
		sxn = 0, syn = 3;  //   sx = dx, sy = (H-1) - dy
	else if(transform == 7)    // flip y then rotate by 90 degrees ccw
		sxn = 3, syn = 1;  //   sx = (H-1) - dy, sy = (W-1) - dx

	for(u32 dy=0; dy<dstdim.Height; dy++)
	for(u32 dx=0; dx<dstdim.Width; dx++)
	{
		u32 entries[4] = {dx, dstdim.Width-1-dx, dy, dstdim.Height-1-dy};
		u32 sx = entries[sxn];
		u32 sy = entries[syn];
		video::SColor c = src->getPixel(sx,sy);
		dst->setPixel(dx,dy,c);
	}
}
