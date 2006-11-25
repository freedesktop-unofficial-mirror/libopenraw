/*
 * libopenraw - jfifcontainer.h
 *
 * Copyright (C) 2006 Hubert Figuiere
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */



#include <cstdio>

namespace JPEG {
#include <jpeglib.h>
}

#include "io/stream.h"
#include "debug.h"
#include "jfifcontainer.h"

namespace OpenRaw {

	using namespace Debug;

	namespace Internals {
		
		/** private source struct for libjpeg 
		 */
		#define BUF_SIZE 1024

		typedef struct {
			struct JPEG::jpeg_source_mgr pub; /**< the public libjpeg struct */
			JFIFContainer * self;       /**< pointer to the owner */
			off_t offset;
			JPEG::JOCTET* buf;
		} jpeg_src_t;

		JFIFContainer::JFIFContainer(IO::Stream *file, off_t offset)
			: RawContainer(file, offset),
				m_cinfo(), m_jerr(),
				m_headerLoaded(false)
		{
			/* this is a hack because jpeg_create_decompress is
			 * implemented as a Macro 
			 */
			using namespace JPEG;

			m_cinfo.err = JPEG::jpeg_std_error(&m_jerr);
			JPEG::jpeg_create_decompress(&m_cinfo);

			/* inspired by jdatasrc.c */

			jpeg_src_t *src = (jpeg_src_t *)
				(*m_cinfo.mem->alloc_small)((JPEG::j_common_ptr)&m_cinfo, 
																		JPOOL_PERMANENT,
																		sizeof(jpeg_src_t));
			m_cinfo.src = (JPEG::jpeg_source_mgr*)src;
			src->pub.init_source = j_init_source;
			src->pub.fill_input_buffer = j_fill_input_buffer;
			src->pub.skip_input_data = j_skip_input_data;
			src->pub.resync_to_restart = JPEG::jpeg_resync_to_restart;
			src->pub.term_source = j_term_source;
			src->self = this;
			src->pub.bytes_in_buffer = 0;
			src->pub.next_input_byte = NULL;
			src->buf = (JPEG::JOCTET*)(*m_cinfo.mem->alloc_small)
				((JPEG::j_common_ptr)&m_cinfo, 
				 JPOOL_PERMANENT,
				 BUF_SIZE * sizeof(JPEG::JOCTET));
		}

		JFIFContainer::~JFIFContainer()
		{
			JPEG::jpeg_destroy_decompress(&m_cinfo);
		}


		void JFIFContainer::getDimensions(uint32_t &x, uint32_t &y)
		{
			if(!m_headerLoaded) {
				_loadHeader();
			}
			x = m_cinfo.output_width;
			y = m_cinfo.output_height;
		}

		int JFIFContainer::_loadHeader()
		{
			int ret;
			ret = JPEG::jpeg_read_header(&m_cinfo, TRUE);
			//Trace(DEBUG1) << "jpeg_read_header " << ret << "\n";
			m_headerLoaded = (ret == 1);

			JPEG::jpeg_calc_output_dimensions(&m_cinfo);
			return ret;
		}


		void JFIFContainer::j_init_source(JPEG::j_decompress_ptr)
		{
		}


		JPEG::boolean 
		JFIFContainer::j_fill_input_buffer(JPEG::j_decompress_ptr cinfo)
		{
			jpeg_src_t *src = (jpeg_src_t*)cinfo->src;
			JFIFContainer *self = src->self;
			size_t n = self->file()->read(src->buf, BUF_SIZE * sizeof(*src->buf));
			if (n >= 0) {
				src->pub.next_input_byte = src->buf;
				src->pub.bytes_in_buffer = n;
			}
			else {
				src->pub.next_input_byte = NULL;
				src->pub.bytes_in_buffer = 0;
			}
			return TRUE;
		}


		void JFIFContainer::j_skip_input_data(JPEG::j_decompress_ptr cinfo, 
																					long num_bytes)
		{
			jpeg_src_t *src = (jpeg_src_t*)cinfo->src;
			if (num_bytes > 0) {
				while ((size_t)num_bytes > src->pub.bytes_in_buffer) {
					num_bytes -= src->pub.bytes_in_buffer;
					j_fill_input_buffer(cinfo);
				}
				src->pub.next_input_byte += (size_t) num_bytes;
				src->pub.bytes_in_buffer -= (size_t) num_bytes;				
			}
		}


		void JFIFContainer::j_term_source(JPEG::j_decompress_ptr)
		{
		}


	}
}