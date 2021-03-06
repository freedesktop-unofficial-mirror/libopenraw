/*
 * libopenraw - ordiag.cpp
 *
 * Copyright (C) 2007-2015 Hubert Figuiere
 * Copyright (C) 2008 Novell, Inc.
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


#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>

#include <libopenraw/libopenraw.h>
#include <libopenraw++/rawfile.h>
#include <libopenraw++/thumbnail.h>
#include <libopenraw++/rawdata.h>
#include <libopenraw++/metavalue.h>

using OpenRaw::RawFile;
using OpenRaw::Thumbnail;
using OpenRaw::RawData;
using OpenRaw::CfaPattern;
using OpenRaw::MetaValue;

/**
 * Dump on RawFile. (functor)
 */
class OrDiag
{
public:
    /** constructor
     * @param out the output stream
     */
    OrDiag(std::ostream & out, const std::string & extract_thumbs)
        : m_out(out)
        , m_extract_all_thumbs(false)
        {
            m_extract_all_thumbs = (extract_thumbs == "all");
            if (!m_extract_all_thumbs) {
                try {
                    int size = boost::lexical_cast<int>(extract_thumbs);
                    m_thumb_sizes.insert(size);
                }
                catch(...) {

                }
            }
        }

    std::string cfaPatternToString(const CfaPattern* pattern)
        {
            if(pattern == NULL) {
                return "(null)";
            }

            std::string out;
            uint16_t size = 0;
            const uint8_t* patternPattern = pattern->patternPattern(size);

            for(uint16_t i = 0; i < size; ++i) {
                switch(patternPattern[i]) {
                case OR_PATTERN_COLOUR_RED:
                    out.push_back('R');
                    break;
                case OR_PATTERN_COLOUR_GREEN:
                    out.push_back('G');
                    break;

                case OR_PATTERN_COLOUR_BLUE:
                    out.push_back('B');
                    break;

                default:
                    out.push_back('*');
                    break;
                }
            }

            return out;
        }
    std::string cfaPatternToString(::or_cfa_pattern t)
        {
            switch(t) {
            case OR_CFA_PATTERN_NONE:
                return "None";
                break;
            case OR_CFA_PATTERN_NON_RGB22:
                return "Non RGB 2x2";
                break;
            case OR_CFA_PATTERN_RGGB:
                return "R,G,G,B";
                break;
            case OR_CFA_PATTERN_GBRG:
                return "G,B,R,G";
                break;
            case OR_CFA_PATTERN_BGGR:
                return "B,G,G,R";
                break;
            case OR_CFA_PATTERN_GRBG:
                return "G,R,B,G";
                break;
            default:
                break;
            }
            return str(boost::format("Unknown %1%") % t);
        };

    std::string dataTypeToString(Thumbnail::DataType t)
        {
            switch(t) {
            case OR_DATA_TYPE_NONE:
                return "None";
                break;
            case OR_DATA_TYPE_PIXMAP_8RGB:
                return "8bits per channel RGB pixmap";
                break;
            case OR_DATA_TYPE_JPEG:
                return "JPEG data";
                break;
            case OR_DATA_TYPE_TIFF:
                return "TIFF container";
                break;
            case OR_DATA_TYPE_PNG:
                return "PNG container";
                break;
            case OR_DATA_TYPE_RAW:
                return "RAW data";
                break;
            case OR_DATA_TYPE_COMPRESSED_RAW:
                return "Compressed RAW data";
                break;
            case OR_DATA_TYPE_UNKNOWN:
                return "Unknown type";
                break;
            default:
                break;
            }
            return "Invalid";
        }

    /** return a string for the raw file type
     */
    std::string typeToString(RawFile::Type t)
        {
            switch(t) {
            case OR_RAWFILE_TYPE_UNKNOWN:
                break;
            case OR_RAWFILE_TYPE_CR2:
                return "Canon CR2";
                break;
            case OR_RAWFILE_TYPE_CRW:
                return "Canon CRW";
                break;
            case OR_RAWFILE_TYPE_NEF:
                return "Nikon NEF";
                break;
            case OR_RAWFILE_TYPE_MRW:
                return "Minolta MRW";
                break;
            case OR_RAWFILE_TYPE_ARW:
                return "Sony ARW";
                break;
            case OR_RAWFILE_TYPE_DNG:
                return "Adobe DNG";
                break;
            case OR_RAWFILE_TYPE_ORF:
                return "Olympus ORF";
                break;
            case OR_RAWFILE_TYPE_PEF:
                return "Pentax PEF";
                break;
            case OR_RAWFILE_TYPE_ERF:
                return "Epson ERF";
                break;
            case OR_RAWFILE_TYPE_RW2:
                return "Panasonic RAW";
                break;
            case OR_RAWFILE_TYPE_RAF:
                return "FujiFilm RAF";
                break;
            default:
                break;
            }
            return "Unknown";
        }

    /** Extract thumbnail to a file
     */
    std::string extractThumb(const Thumbnail & thumb)
        {
            FILE* f;
            size_t s;
            std::string ext;
            switch(thumb.dataType()) {
            case OR_DATA_TYPE_PIXMAP_8RGB:
                ext = "ppm";
                break;
            case OR_DATA_TYPE_JPEG:
                ext = "jpg";
                break;
            default:
                break;
            }
            if (ext.empty()) {
                return "";
            }

            uint32_t x, y;
            x = thumb.width();
            y = thumb.height();
            uint32_t dim = std::max(x, y);
            std::string name(str(boost::format("thumb_%1%.%2%") % dim  % ext));
            f = fopen(name.c_str(), "wb");
            if (thumb.dataType() == OR_DATA_TYPE_PIXMAP_8RGB) {
                // ppm preemble.
                fprintf(f, "P6\n");
                fprintf(f, "%d %d\n", x, y);
                fprintf(f, "%d\n", /*(componentsize == 2) ? 0xffff :*/ 0xff);
            }
            s = fwrite(thumb.data(), 1, thumb.size(), f);
            if(s != thumb.size()) {
                std::cerr << "short write of " << s << " bytes\n";
            }
            fclose(f);

            return name;
        }

    /** dump the previews of the raw file to mout
     */
    void dumpPreviews(const std::unique_ptr<RawFile> & rf)
        {
            const std::vector<uint32_t> & previews = rf->listThumbnailSizes();
            m_out << boost::format("\tNumber of previews: %1%\n")
                % previews.size();

            m_out << "\tAvailable previews:\n";
            for(auto iter = previews.begin(); iter != previews.end(); iter++) {

                m_out << boost::format("\t\tSize %1%\n") % *iter;

                Thumbnail thumb;
                ::or_error err = rf->getThumbnail(*iter, thumb);
                if (err != OR_ERROR_NONE) {
                    m_out << boost::format("\t\t\tError getting thumbnail %1%\n") % err;
                }
                else {
                    m_out << boost::format("\t\t\tFormat %1%\n")
                        % dataTypeToString(thumb.dataType());
                    m_out << boost::format("\t\t\tDimensions: width = %1% height = %2%\n")
                        % thumb.width() % thumb.height();
                    m_out << boost::format("\t\t\tByte size: %1%\n")
                        % thumb.size();
                }
                if (m_extract_all_thumbs
                    || m_thumb_sizes.find(*iter) != m_thumb_sizes.end()) {

                    std::string name = extractThumb(thumb);

                    m_out << boost::format("\t\t\tOutput as %1%\n") % name;
                }
            }
        }

    void dumpRawData(const std::unique_ptr<RawFile> & rf)
        {
            RawData rd;
            ::or_error err = rf->getRawData(rd, 0);
            if (err == OR_ERROR_NONE) {
                m_out << "\tRAW data\n";
                m_out << boost::format("\t\tType: %1%")
                    % dataTypeToString(rd.dataType());
                if(rd.dataType() == OR_DATA_TYPE_COMPRESSED_RAW)  {
                    m_out << boost::format(" (compression = %1%)\n") % rd.compression();
                }
                else {
                    m_out << "\n";
                }
                m_out << boost::format("\t\tByte size: %1%\n")
                    % rd.size();
                m_out << boost::format("\t\tDimensions: width = %1% height = %2%\n")
                    % rd.width() % rd.height();
                m_out << boost::format("\t\tROI: %1% %2% %3% %4%\n")
                    % rd.roi_x() % rd.roi_y() % rd.roi_width() % rd.roi_height();
                const CfaPattern* pattern = rd.cfaPattern();
                ::or_cfa_pattern patternType
                      = pattern ? pattern->patternType()
                      : OR_CFA_PATTERN_NON_RGB22;
                m_out << boost::format("\t\tBayer Type: %1%\n")
                    % cfaPatternToString(patternType);

                if(patternType == OR_CFA_PATTERN_NON_RGB22) {
                    m_out << boost::format("\t\tPattern: %1%\n")
                        % cfaPatternToString(rd.cfaPattern());
                }

                m_out << boost::format("\t\tBits per channel: %1%\n")
                    % rd.bpc();
                m_out << boost::format(
                    "\t\tValues: black = %1% white = %2%\n")
                    % rd.blackLevel() % rd.whiteLevel();

                uint32_t matrix_size = 0;
                const double* matrix = rd.getColourMatrix1(matrix_size);
                if (matrix) {
                    m_out << boost::format("\t\tColour Matrix 1: ");
                    for (uint32_t i = 0; i < matrix_size; i++) {
                        if (i > 0) {
                            m_out << ", ";
                        }
                        m_out << matrix[i];
                    }
                    m_out << "\n";
                }
            }
            else {
                m_out << boost::format("\tNo Raw Data found! (error = %1%)\n")
                    % err;
            }
        }
    void dumpMetaData(const std::unique_ptr<RawFile> & rf)
        {
            int32_t o;
            o = rf->getOrientation();
            m_out << "\tMeta data\n";
            m_out << boost::format("\t\tOrientation: %1%\n")
                % o;
            double matrix[9];
            uint32_t size = 9;

            ExifLightsourceValue calIll;
            calIll = rf->getCalibrationIlluminant1();
            m_out << boost::format("\t\tCalibration Illuminant 1: %1%\n")
                % static_cast<int>(calIll);

            ::or_error err = rf->getColourMatrix1(matrix, size);
            if(err == OR_ERROR_NONE) {
                m_out << boost::format("\t\tColour Matrix 1: %1%, %2%, %3%, "
                                       "%4%, %5%, %6%, %7%, %8%, %9%\n")
                    % matrix[0] % matrix[1] % matrix[2]
                    % matrix[3] % matrix[4] % matrix[5]
                    % matrix[6] % matrix[7] % matrix[8];
            }
            else {
                m_out << "\t\tNo Colour Matrix 1\n";
            }

            calIll = rf->getCalibrationIlluminant2();
            m_out << boost::format("\t\tCalibration Illuminant 2: %1%\n")
                % static_cast<int>(calIll);

            size = 9;
            err = rf->getColourMatrix2(matrix, size);
            if(err == OR_ERROR_NONE) {
                m_out << boost::format("\t\tColour Matrix 2: %1%, %2%, %3%, "
                                       "%4%, %5%, %6%, %7%, %8%, %9%\n")
                    % matrix[0] % matrix[1] % matrix[2]
                    % matrix[3] % matrix[4] % matrix[5]
                    % matrix[6] % matrix[7] % matrix[8];
            }
            else {
                m_out << "\t\tNo Colour Matrix 2\n";
            }
        }
    void operator()(const std::string &s)
        {
            m_out << boost::format("Dumping %1%\n") % s;

            std::unique_ptr<RawFile> rf(RawFile::newRawFile(s.c_str()));

            if (rf == NULL) {
                m_out << "unrecognized file\n";
            }
            else {
                m_out << boost::format("\tType = %1% (%2%)\n") % rf->type()
                    % typeToString(rf->type());
                std::string typeId
                    = str(boost::format("%1%, %2%")
                          % OR_GET_FILE_TYPEID_VENDOR(rf->typeId())
                          % OR_GET_FILE_TYPEID_CAMERA(rf->typeId()));
                m_out << boost::format("\tType ID = %1%\n") % typeId;

                const MetaValue* make
                    = rf->getMetaValue(META_NS_TIFF | EXIF_TAG_MAKE);
                if (make) {
                    m_out << boost::format("\tMake = %1%\n")
                        % make->getString(0);
                }
                const MetaValue* model
                    = rf->getMetaValue(META_NS_TIFF | EXIF_TAG_MODEL);
                if (model) {
                    m_out << boost::format("\tModel = %1%\n")
                        % model->getString(0);
                }
                auto uniqueCameraModel
                    = rf->getMetaValue(META_NS_TIFF
                                       | DNG_TAG_UNIQUE_CAMERA_MODEL);
                if (uniqueCameraModel) {
                    m_out << boost::format("\tUnique Camera Model = %1%\n")
                        % uniqueCameraModel->getString(0);
                }
                dumpPreviews(rf);
                dumpRawData(rf);
                dumpMetaData(rf);
            }
        }
private:
    std::ostream & m_out;
    bool m_extract_all_thumbs;
    std::set<int> m_thumb_sizes;
};


void print_help()
{
    std::cerr << "ordiag [-v] [-h] [-t all|<size>] [-d 0-9] [files...]\n";
    std::cerr << "Print libopenraw diagnostics\n";
    std::cerr << "\t-h: show this help\n";
    std::cerr << "\t-v: show version\n";
    std::cerr << "\t-d level: set debug / verbosity to level\n";
    std::cerr << "\t-t [all|<size>]: extract thumbnails. all or <size>.\n";
    std::cerr << "\tfiles: the files to diagnose\n";
}

void print_version()
{
    std::cerr << "ordiag version 0.1 - (c) 2007-2014 Hubert Figuiere\n";
}



int main(int argc, char **argv)
{
    int done = 0;
    int dbl = 0;
    std::string extract_thumbs;
    std::vector<std::string> files;

    OpenRaw::init();

    int o;
    while((o = getopt(argc, argv, "hvdt:")) != -1) {
        switch (o) {
        case 'h':
            print_help();
            done = 1;
            break;
        case 'v':
            print_version();
            done = 1;
            break;
        case 'd':
            dbl++;
            break;
        case 't':
            if(optarg) {
                extract_thumbs = optarg;
            }
            else {
                print_help();
                done = 1;
            }
            break;
        case '?':
            break;
        default:
            break;
        }
    }
    if (done) {
        return 1;
    }
    for ( ; optind < argc; optind++) {
        files.push_back(argv[optind]);
    }

    if (files.empty()) {
        std::cerr << "missing file name.\n";
        if (dbl) {
            print_version();
        }
        print_help();
        return 1;
    }

    if (dbl >=2) {
        or_debug_set_level(DEBUG2);
    }
    // do the business.
    for_each(files.begin(), files.end(), OrDiag(std::cout, extract_thumbs));

    return 0;
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
