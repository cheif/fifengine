/***************************************************************************
 *   Copyright (C) 2005-2007 by the FIFE Team                              *
 *   fife-public@lists.sourceforge.net                                     *
 *   This file is part of FIFE.                                            *
 *                                                                         *
 *   FIFE is free software; you can redistribute it and/or modify          *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA              *
 ***************************************************************************/

// Standard C++ library includes

// 3rd party library includes
#include "SDL_image.h"

// FIFE includes
// These includes are split up in two parts, separated by one empty line
// First block: files included from the FIFE root src directory
// Second block: files included from the same folder
#include "vfs/vfs.h"
#include "vfs/raw/rawdata.h"
#include "video/rect.h"
#include "video/renderable.h"
#include "video/rendermanager.h"
#include "video/renderbackend.h"
#include "video/image.h"
#include "video/pixelbuffer.h"
#include "video/animation.h"
#include "map/loaders/util/subimage_provider.h"
#include "imagecache.h"
#include "log.h"

#include "imgcache.h"

namespace FIFE { namespace map { namespace loaders { namespace xml {

	Imgcache::Imgcache() : m_isSharing(false) {
		m_imageIndexMap = type_imageIndexMapPtr(new type_imageIndexMap());
	}

	Imgcache::Imgcache(type_imageIndexMapPtr thisIndex) : 
		m_imageIndexMap(thisIndex), m_isSharing(true) {}

	Imgcache::type_imageIndexMapPtr Imgcache::getIndices() {
		if (!m_isSharing) {
			Log("ImgcacheLoader") 
				<< "getIndices() called on an instance already using shared indices.";
		}
		return m_imageIndexMap;
	}

	void Imgcache::clear() {
		m_imageIndexMap->clear();
	}

	SDL_Surface* Imgcache::getSubImage(SDL_Surface* src, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
		SDL_Surface* result = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 32, 0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff);
		SDL_FillRect(result, NULL, 0);
		SDL_SetAlpha(src,0,SDL_ALPHA_OPAQUE);
		SDL_Rect src_rect = { x, y, w, h };
		SDL_BlitSurface(src,&src_rect,result,0);

		return result;
	}

	int Imgcache::loadTileset(TiXmlElement* element) {
 		// assume "element" is a valid node
 		int tilewidth, tileheight = 0;
 		int firstgid = 0;
 		int rowskip = 0;
 		const char* tilesetname = element->Attribute("name");
 		if (!tilesetname) {
 			Log("ImgcacheLoader") << "Warning: tileset with no name!";
 			tilesetname = "[invalid]";
 		}
 		element->QueryIntAttribute("tilewidth", &tilewidth);
 		element->QueryIntAttribute("tileheight", &tileheight);
 		if (!tilewidth || !tileheight) {
 			Log("ImgcacheLoader") 
				<< "Error: both tilewidth and tileheight attribute need to be defined!";
 			return 1;
 		}
 		if (tilewidth > 65535 || tileheight > 65535) {
 			Log("ImgcacheLoader") << "Error: tile sizes have to be <= 65535";
 			return 1;
 		}

 		TiXmlElement* imagenode = element->FirstChildElement("image");
 		if (!imagenode) {
 			Log("ImgcacheLoader") 
				<< "Error: tileset needs to have at least one <image> child!";
 			return 1;
 		}

 		ImageCache* ic = ImageCache::instance();
 		while (imagenode) {
 			const char* imgsrc = imagenode->Attribute("source");
 			if (!imgsrc) {
 				Log("ImgcacheLoader") 
					<< "Error: image node with no 'source' attribute!";
 				return 1;
 			}
			size_t base = ic->addImageFromFile(imgsrc);
			PixelBufferPtr pbuffer = ic->getPixelBuffer(base);

 			imagenode->QueryIntAttribute("firstgid", &firstgid);
 			imagenode->QueryIntAttribute("rowskip", &rowskip);
 			int xsteps = pbuffer->getSurface()->w / tilewidth;
 			int ysteps = pbuffer->getSurface()->h / tileheight;

 			for (uint8_t y = 0; y < static_cast<uint8_t>(ysteps); y++) {
 				for (uint8_t x = 0; x < static_cast<uint8_t>(xsteps); x++) {

					RenderableLocation location(RenderAble::RT_SUBIMAGE);
					location.addExtension(RenderableLocation::IMAGE_ID, base);
					location.addExtension(RenderableLocation::X, x*tilewidth);
					location.addExtension(RenderableLocation::Y, y*tileheight);
					location.addExtension(RenderableLocation::W, tilewidth);
					location.addExtension(RenderableLocation::H, tileheight);

					size_t iid = ic->addImageFromLocation(location);

 					if (!rowskip) {
 						Log("ImgcacheLoader") 
							<< "DEBUG1 " << firstgid+x+y*ysteps 
							<< " internal: " << iid;

 						(*m_imageIndexMap)[firstgid+x+y*ysteps] = iid;
					} else {
 						Log("ImgcacheLoader")
							<< "DEBUG2 " << firstgid+x+y*rowskip
							<< " internal: " << iid;

						(*m_imageIndexMap)[firstgid+x+y*rowskip] = iid;
					}
 				}
 			}
 			imagenode = imagenode->NextSiblingElement("image");
 		}
		return 0;
	}

	int Imgcache::loadAnimation(TiXmlElement* element) {
		int frameDelay = 0;
		int actionFrame = 0;
		int id = 0;
		element->QueryIntAttribute("delay", &frameDelay);
		element->QueryIntAttribute("action", &actionFrame);
		element->QueryIntAttribute("id", &id);
		if (!id) {
			Log("ImgcacheLoader") << "Error: animation without id!";
			return 1;
		}

		std::vector<Image*> frames;
		
		TiXmlElement* subel = element->FirstChildElement("frame");
		if (!subel) {
			Log("ImgcacheLoader") << "Error: animation with no frame children!";
			return 1;
		}
		
		while (subel) {
			const char* imgsrc = subel->Attribute("source");
 			if (!imgsrc) {
 				Log("ImgcacheLoader") 
					<< "Error: frame node with no 'source' attribute!";
 				continue;
 			}

			SDL_Surface *base = loadImageAsSurface(imgsrc);
			if (!base) {
				Log("ImgcacheLoader")
					<< "Error: could not load frame image: " << imgsrc;
				continue;
			}
			Image* as_image = CRenderBackend()->createStaticImageFromSDL(base);
			int x_off = 0;
			int y_off = 0;
			subel->QueryIntAttribute("x_off", &x_off);
			subel->QueryIntAttribute("y_off", &y_off);

			as_image->setXShift(x_off);
			as_image->setYShift(y_off);
			
			frames.push_back(as_image);
			subel = subel->NextSiblingElement("frame");
		}

		Animation* anim = new Animation(frames.size());
		anim->setFrameDuration(frameDelay);
		anim->setActionFrame(actionFrame);
		for (size_t i = 0; i < frames.size(); ++i) {
			anim->setFrame(i, frames[i]);
		}
		anim->setCurrentFrame(0);
		
		(*m_imageIndexMap)[id] = ImageCache::instance()->addImage(anim);

		return 0;
	}

	SDL_Surface* Imgcache::loadImageAsSurface(const char* filename) {
		SDL_RWops* rwops = createRWops( filename );
		SDL_Surface* base = IMG_Load_RW(rwops,1);
		if (!base) {
			Log("ImgcacheLoader") << "Error loading " << filename;
			return NULL;
		}
		return base;
	}

	int Imgcache::loadImageAs(const char* filename, int id) {
		ImageCache *ic = ImageCache::instance();
		(*m_imageIndexMap)[id] = ic->addImageFromFile(filename);
		return 0;
	}

	size_t Imgcache::getImageForId(int id) {
		return (*m_imageIndexMap)[id];
	}

	SDL_RWops* Imgcache::createRWops(const char *filename) {
		RawDataPtr data = VFS::instance()->open(filename);
		size_t size = data->getDataLength();
		uint8_t* filedata = new uint8_t[ size ];
		data->readInto(filedata,size);

		return SDL_RWFromMem(filedata, size);
	}
} } } }
/* vim: set noexpandtab: set shiftwidth=2: set tabstop=2: */
