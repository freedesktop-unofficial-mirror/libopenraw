/*
 * libopenraw - decompressor.h
 *
 * Copyright (C) 2007 Hubert Figuiere
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#ifndef __OPENRAW_DECOMPRESS_H__
#define __OPENRAW_DECOMPRESS_H__

#include <boost/noncopyable.hpp>

#include <libopenraw/libopenraw.h>


namespace OpenRaw {

	class BitmapData;
		
	namespace Internals {

		class Decompressor
			: private boost::noncopyable
		{
		public:
			Decompressor(const BitmapData *);
			virtual ~Decompressor();
			
			/** decompress the bitmapdata and return a new bitmap
			 * @return the new bitmap decompressed. NULL is failure.
			 * Caller must free it.
			 * @todo use a shared_ptr here, or something
			 */
			virtual BitmapData *decompress() = 0;
		protected:
			const BitmapData *m_data;
		};

	}
}



#endif
