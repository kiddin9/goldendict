#ifndef __TIFF_HH_INCLUDED__
#define __TIFF_HH_INCLUDED__

#include <QImage>

namespace GdTiff
{

// QImage don't handle TIFF files if TIFFTAG_PHOTOMETRIC is not set
// We will handle such 1-bit b/w images with default photometric

bool tiffToQImage( const char * data, int size, QImage & image );

}

#endif // TIFF_HH
