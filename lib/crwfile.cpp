/*
 * libopenraw - crwfile.cpp
 *
 * Copyright (C) 2006-2014 Hubert Figuiere
 * Copyright (c) 2008 Novell, Inc.
 *
 * This library is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <algorithm>
#include <utility>
#include <functional>
#include <memory>

#include <libopenraw/cameraids.h>
#include <libopenraw++/thumbnail.h>
#include <libopenraw++/rawdata.h>
#include <libopenraw++/metavalue.h>

#include "trace.h"
#include "io/file.h"
#include "io/streamclone.h"
#include "io/memstream.h"
#include "crwfile.h"
#include "ciffcontainer.h"
#include "jfifcontainer.h"
#include "crwdecompressor.h"
#include "rawfile_private.h"

#include "rawfilefactory.h"

using namespace Debug;

namespace OpenRaw {

namespace Internals {

using namespace CIFF;

#define OR_MAKE_CANON_TYPEID(camid) \
    OR_MAKE_FILE_TYPEID(OR_TYPEID_VENDOR_CANON,camid)

/* taken from dcraw, by default */
static const BuiltinColourMatrix s_matrices[] = {
    { OR_MAKE_CANON_TYPEID(OR_TYPEID_CANON_D30), 0, 0,
	{ 9805,-2689,-1312,-5803,13064,3068,-2438,3075,8775 } },
    { OR_MAKE_CANON_TYPEID(OR_TYPEID_CANON_D60), 0, 0xfa0,
	{ 6188,-1341,-890,-7168,14489,2937,-2640,3228,8483 } },
    { OR_MAKE_CANON_TYPEID(OR_TYPEID_CANON_10D), 0, 0xfa0,
	{ 8197,-2000,-1118,-6714,14335,2592,-2536,3178,8266 } },
    { OR_MAKE_CANON_TYPEID(OR_TYPEID_CANON_300D), 0, 0xfa0,
	{ 8197,-2000,-1118,-6714,14335,2592,-2536,3178,8266 } },
//    { OR_MAKE_CANON_TYPEID(OR_TYPEID_CANON_G1), 0, 0,
//	{ -4778,9467,2172,4743,-1141,4344,-5146,9908,6077,-1566,11051,557 } },
    { OR_MAKE_CANON_TYPEID(OR_TYPEID_CANON_G2), 0, 0,
	{ 9087,-2693,-1049,-6715,14382,2537,-2291,2819,7790 } },
    { OR_MAKE_CANON_TYPEID(OR_TYPEID_CANON_G3), 0, 0,
	{ 9212,-2781,-1073,-6573,14189,2605,-2300,2844,7664 } },
    { OR_MAKE_CANON_TYPEID(OR_TYPEID_CANON_G5), 0, 0,
	{ 9757,-2872,-933,-5972,13861,2301,-1622,2328,7212 } },
    { OR_MAKE_CANON_TYPEID(OR_TYPEID_CANON_G6), 0, 0,
	{ 9877,-3775,-871,-7613,14807,3072,-1448,1305,7485 } },
    { OR_MAKE_CANON_TYPEID(OR_TYPEID_CANON_PRO1), 0, 0,
	{ 10062,-3522,-999,-7643,15117,2730,-765,817,7323 } },
    { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0, 0 } }
};

const RawFile::camera_ids_t CRWFile::s_def[] = {
    { "Canon EOS D30" , OR_MAKE_CANON_TYPEID(OR_TYPEID_CANON_D30) },
    { "Canon EOS D60" , OR_MAKE_CANON_TYPEID(OR_TYPEID_CANON_D60) },
    { "Canon EOS 10D" , OR_MAKE_CANON_TYPEID(OR_TYPEID_CANON_10D) },
    { "Canon EOS 300D DIGITAL", OR_MAKE_CANON_TYPEID(OR_TYPEID_CANON_300D) },
    { "Canon PowerShot G1", OR_MAKE_CANON_TYPEID(OR_TYPEID_CANON_G1) },
    { "Canon PowerShot G2", OR_MAKE_CANON_TYPEID(OR_TYPEID_CANON_G2) },
    { "Canon PowerShot G3", OR_MAKE_CANON_TYPEID(OR_TYPEID_CANON_G3) },
    { "Canon PowerShot G5", OR_MAKE_CANON_TYPEID(OR_TYPEID_CANON_G5) },
    { "Canon PowerShot G6", OR_MAKE_CANON_TYPEID(OR_TYPEID_CANON_G6) },
    { "Canon PowerShot G7", OR_MAKE_CANON_TYPEID(OR_TYPEID_CANON_G7) },
    { "Canon PowerShot Pro1", OR_MAKE_CANON_TYPEID(OR_TYPEID_CANON_PRO1) },
    { 0, 0 }
};

RawFile *CRWFile::factory(const IO::Stream::Ptr &s)
{
    return new CRWFile(s);
}

CRWFile::CRWFile(const IO::Stream::Ptr &s)
    : RawFile(OR_RAWFILE_TYPE_CRW),
      m_io(s),
      m_container(new CIFFContainer(m_io)),
      m_x(0), m_y(0)
{
    _setIdMap(s_def);
    _setMatrices(s_matrices);
}

CRWFile::~CRWFile()
{
    delete m_container;
}

::or_error CRWFile::_enumThumbnailSizes(std::vector<uint32_t> &list)
{
    ::or_error err = OR_ERROR_NOT_FOUND;

    Heap::Ref heap = m_container->heap();
    if(!heap) {
        // this is not a CIFF file.
        return err;
    }
    const RecordEntry::List & records = heap->records();
    RecordEntry::List::const_iterator iter;
    iter = std::find_if(records.begin(), records.end(), std::bind(
                            &RecordEntry::isA, std::placeholders::_1,
                            static_cast<uint16_t>(TAG_JPEGIMAGE)));
    if (iter != records.end()) {
        Trace(DEBUG2) << "JPEG @" << (*iter).offset << "\n";
        m_x = m_y = 0;
	uint32_t offset = heap->offset() + (*iter).offset;
        IO::StreamClone::Ptr s(new IO::StreamClone(m_io, offset));
        std::unique_ptr<JfifContainer> jfif(new JfifContainer(s, 0));

        jfif->getDimensions(m_x, m_y);
        Trace(DEBUG1) << "JPEG dimensions x=" << m_x
                      << " y=" << m_y << "\n";
        uint32_t dim = std::max(m_x,m_y);
        _addThumbnail(dim, ThumbDesc(m_x, m_y, OR_DATA_TYPE_JPEG, offset, (*iter).length));
        list.push_back(dim);
        err = OR_ERROR_NONE;
    }

    return err;
}

RawContainer* CRWFile::getContainer() const
{
  return m_container;
}

::or_error CRWFile::_getRawData(RawData & data, uint32_t options)
{
    ::or_error err = OR_ERROR_NOT_FOUND;
    Heap::Ref props = m_container->getImageProps();

    if(!props) {
        return OR_ERROR_NOT_FOUND;
    }
    const ImageSpec * img_spec = m_container->getImageSpec();
    uint32_t x, y;
    x = y = 0;
    if(img_spec) {
        x = img_spec->imageWidth;
        y = img_spec->imageHeight;
    }

    // locate decoder table
    const CIFF::RecordEntry::List & propsRecs = props->records();
    auto iter = std::find_if(propsRecs.begin(), propsRecs.end(), std::bind(
                                 &RecordEntry::isA, std::placeholders::_1,
                                 static_cast<uint16_t>(TAG_EXIFINFORMATION)));
    if (iter == propsRecs.end()) {
        Trace(ERROR) << "Couldn't find the Exif information.\n";
        return err;
    }

    Heap exifProps(iter->offset + props->offset(), iter->length, m_container);

    const RecordEntry::List & exifPropsRecs = exifProps.records();
    iter = std::find_if(exifPropsRecs.begin(), exifPropsRecs.end(),
                        std::bind(
                            &RecordEntry::isA, std::placeholders::_1,
                            static_cast<uint16_t>(TAG_DECODERTABLE)));
    if (iter == exifPropsRecs.end()) {
        Trace(ERROR) << "Couldn't find the decoder table.\n";
        return err;
    }
    Trace(DEBUG2) << "length = " << iter->length << "\n";
    Trace(DEBUG2) << "offset = " << exifProps.offset() + iter->offset << "\n";
    auto file = m_container->file();
    file->seek(exifProps.offset() + iter->offset, SEEK_SET);
    uint32_t decoderTable;
    if(m_container->readUInt32(file, decoderTable)) {
        Trace(DEBUG2) << "decoder table = " << decoderTable << "\n";
    }

    // locate the CFA info
    uint16_t cfa_x, cfa_y;
    iter = std::find_if(exifPropsRecs.begin(), exifPropsRecs.end(), std::bind(
                            &RecordEntry::isA, std::placeholders::_1,
                            static_cast<uint16_t>(TAG_SENSORINFO)));
    if (iter == exifPropsRecs.end()) {
        Trace(ERROR) << "Couldn't find the sensor info.\n";
        return err;
    }
    Trace(DEBUG2) << "length = " << iter->length << "\n";
    Trace(DEBUG2) << "offset = " << exifProps.offset() + iter->offset << "\n";

    // go figure what the +2 is. looks like it is the byte #
    file->seek(exifProps.offset() + iter->offset + 2, SEEK_SET);
    if(!(m_container->readUInt16(file, cfa_x)
         && m_container->readUInt16(file, cfa_y))) {
        Trace(ERROR) << "Couldn't find the sensor size.\n";
        return err;
    }


    const CIFF::RecordEntry *entry = m_container->getRawDataRecord();
    if (entry) {
        CIFF::Heap::Ref heap = m_container->heap();
        Trace(DEBUG2) << "RAW @" << heap->offset() + entry->offset << "\n";
        size_t byte_size = entry->length;
        void *buf = data.allocData(byte_size);
        size_t real_size = entry->fetchData(heap.get(), buf, byte_size);
        if (real_size != byte_size) {
            Trace(WARNING) << "wrong size\n";
        }
        data.setDimensions(x, y);
        data.setCfaPatternType(OR_CFA_PATTERN_RGGB);
        data.setDataType(OR_DATA_TYPE_COMPRESSED_RAW);

        // decompress if we need
        if((options & OR_OPTIONS_DONT_DECOMPRESS) == 0) {
            std::unique_ptr<IO::Stream> s(new IO::MemStream(data.data(),
                                                            data.size()));
            s->open(); // TODO check success

            CrwDecompressor decomp(s.get(), m_container);

            decomp.setOutputDimensions(cfa_x, cfa_y);
            decomp.setDecoderTable(decoderTable);
            RawData *dData = decomp.decompress();
            if (dData != NULL) {
                Trace(DEBUG1) << "Out size is " << dData->width()
                              << "x" << dData->height() << "\n";
                dData->setCfaPatternType(data.cfaPattern()->patternType());
                data.swap(*dData);
                delete dData;
            }
        }
        err = OR_ERROR_NONE;
    }
    return err;
}

MetaValue *CRWFile::_getMetaValue(int32_t meta_index)
{
    MetaValue * val = NULL;

    switch(META_INDEX_MASKOUT(meta_index)) {
    case META_NS_TIFF:
    {
        uint32_t index = META_NS_MASKOUT(meta_index);
        switch(index) {
        case EXIF_TAG_ORIENTATION:
        {
            const ImageSpec * img_spec = m_container->getImageSpec();
            if(img_spec) {
                val = new MetaValue(static_cast<uint32_t>(
                                            img_spec->exifOrientation()));
            }
            break;
        }
        case EXIF_TAG_MAKE:
        case EXIF_TAG_MODEL:
        {
            if (index == EXIF_TAG_MAKE && !m_make.empty()) {
                val = new MetaValue(m_make);
                break;
            }
            if (index == EXIF_TAG_MODEL && !m_model.empty()) {
                val = new MetaValue(m_model);
                break;
            }

            CIFF::Heap::Ref heap = m_container->getCameraProps();
            if(heap) {
                auto propsRecs = heap->records();
                auto iter
                    = std::find_if(propsRecs.begin(), propsRecs.end(),
                                   [](const CIFF::RecordEntry &e){
                                       return e.isA(static_cast<uint16_t>(CIFF::TAG_RAWMAKEMODEL));
                                   });
                if (iter == propsRecs.end()) {
                    Trace(ERROR) << "Couldn't find the image info.\n";
                }
                else {
                    char buf[256];
                    size_t sz = iter->length;
                    if(sz > 256) {
                        sz = 256;
                    }
                    /*size_t sz2 = */iter->fetchData(heap.get(),
                                                     (void*)buf, sz);
                    const char *p = buf;
                    while(*p) {
                        p++;
                    }
                    m_make = std::string(buf, p - buf);
                    p++;
                    m_model = p;

                    if (index == EXIF_TAG_MODEL) {
                        val = new MetaValue(m_model);
                    }
                    else if (index == EXIF_TAG_MAKE) {
                        val = new MetaValue(m_make);
                    }
                    Trace(DEBUG1) << "Make " << m_make << "\n";
                    Trace(DEBUG1) << "Model " << m_model << "\n";
                }
            }


            break;
        }
        }
        break;
    }
    case META_NS_EXIF:
        break;
    default:
        Trace(ERROR) << "Unknown Meta Namespace\n";
        break;
    }

    return val;
}

void CRWFile::_identifyId()
{
    std::string model;
    std::string make;
    try {
        MetaValue * v = _getMetaValue(META_NS_TIFF | EXIF_TAG_MODEL);
        if(v) {
            model = v->getString(0);
        }
        delete v;
        v = _getMetaValue(META_NS_TIFF | EXIF_TAG_MAKE);
        if(v) {
            make = v->getString(0);
        }
        delete v;
        _setTypeId(_typeIdFromModel(make, model));
    }
    catch(...)
    {
    }
}

}
}

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0))
  indent-tabs-mode:nil
  fill-column:80
  End:
*/
