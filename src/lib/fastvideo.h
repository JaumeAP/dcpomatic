#include <dcp/data.h>
#include <boost/shared_ptr.hpp>

class OpenJPEGImage;

boost::shared_ptr<OpenJPEGImage> fastvideo_decompress_j2k (dcp::Data data, int reduce);
