#! /usr/bin/env python
"""Whimpy test script for the cl module
   Roger E. Masse
"""
import cl
from test_support import verify, verbose

clattrs = ['ADDED_ALGORITHM_ERROR', 'ALAW', 'ALGORITHM_ID',
'ALGORITHM_VERSION', 'AUDIO', 'AWARE_ERROR', 'AWARE_MPEG_AUDIO',
'AWARE_MULTIRATE', 'AWCMP_CONST_QUAL', 'AWCMP_FIXED_RATE',
'AWCMP_INDEPENDENT', 'AWCMP_JOINT_STEREO', 'AWCMP_LOSSLESS',
'AWCMP_MPEG_LAYER_I', 'AWCMP_MPEG_LAYER_II', 'AWCMP_STEREO',
'Algorithm', 'AlgorithmNumber', 'AlgorithmType', 'AudioFormatName',
'BAD_ALGORITHM_NAME', 'BAD_ALGORITHM_TYPE', 'BAD_BLOCK_SIZE',
'BAD_BOARD', 'BAD_BUFFERING', 'BAD_BUFFERLENGTH_NEG',
'BAD_BUFFERLENGTH_ODD', 'BAD_BUFFER_EXISTS', 'BAD_BUFFER_HANDLE',
'BAD_BUFFER_POINTER', 'BAD_BUFFER_QUERY_SIZE', 'BAD_BUFFER_SIZE',
'BAD_BUFFER_SIZE_POINTER', 'BAD_BUFFER_TYPE',
'BAD_COMPRESSION_SCHEME', 'BAD_COMPRESSOR_HANDLE',
'BAD_COMPRESSOR_HANDLE_POINTER', 'BAD_FRAME_SIZE',
'BAD_FUNCTIONALITY', 'BAD_FUNCTION_POINTER', 'BAD_HEADER_SIZE',
'BAD_INITIAL_VALUE', 'BAD_INTERNAL_FORMAT', 'BAD_LICENSE',
'BAD_MIN_GT_MAX', 'BAD_NO_BUFFERSPACE', 'BAD_NUMBER_OF_BLOCKS',
'BAD_PARAM', 'BAD_PARAM_ID_POINTER', 'BAD_PARAM_TYPE', 'BAD_POINTER',
'BAD_PVBUFFER', 'BAD_SCHEME_POINTER', 'BAD_STREAM_HEADER',
'BAD_STRING_POINTER', 'BAD_TEXT_STRING_PTR', 'BEST_FIT',
'BIDIRECTIONAL', 'BITRATE_POLICY', 'BITRATE_TARGET',
'BITS_PER_COMPONENT', 'BLENDING', 'BLOCK_SIZE', 'BOTTOM_UP',
'BUFFER_NOT_CREATED', 'BUF_DATA', 'BUF_FRAME', 'BytesPerPixel',
'BytesPerSample', 'CHANNEL_POLICY', 'CHROMA_THRESHOLD', 'CODEC',
'COMPONENTS', 'COMPRESSED_BUFFER_SIZE', 'COMPRESSION_RATIO',
'COMPRESSOR', 'CONTINUOUS_BLOCK', 'CONTINUOUS_NONBLOCK',
'CompressImage', 'DATA', 'DECOMPRESSOR', 'DecompressImage',
'EDGE_THRESHOLD', 'ENABLE_IMAGEINFO', 'END_OF_SEQUENCE', 'ENUM_VALUE',
'EXACT_COMPRESSION_RATIO', 'EXTERNAL_DEVICE', 'FLOATING_ENUM_VALUE',
'FLOATING_RANGE_VALUE', 'FRAME', 'FRAME_BUFFER_SIZE',
'FRAME_BUFFER_SIZE_ZERO', 'FRAME_RATE', 'FRAME_TYPE', 'G711_ALAW',
'G711_ULAW', 'GRAYSCALE', 'GetAlgorithmName', 'HDCC',
'HDCC_SAMPLES_PER_TILE', 'HDCC_TILE_THRESHOLD', 'HEADER_START_CODE',
'IMAGE_HEIGHT', 'IMAGE_WIDTH', 'INTERNAL_FORMAT',
'INTERNAL_IMAGE_HEIGHT', 'INTERNAL_IMAGE_WIDTH', 'INTRA', 'JPEG',
'JPEG_ERROR', 'JPEG_NUM_PARAMS', 'JPEG_QUALITY_FACTOR',
'JPEG_QUANTIZATION_TABLES', 'JPEG_SOFTWARE', 'JPEG_STREAM_HEADERS',
'KEYFRAME', 'LAST_FRAME_INDEX', 'LAYER', 'LUMA_THRESHOLD',
'MAX_NUMBER_OF_AUDIO_ALGORITHMS', 'MAX_NUMBER_OF_ORIGINAL_FORMATS',
'MAX_NUMBER_OF_PARAMS', 'MAX_NUMBER_OF_VIDEO_ALGORITHMS', 'MONO',
'MPEG_VIDEO', 'MVC1', 'MVC2', 'MVC2_BLENDING', 'MVC2_BLENDING_OFF',
'MVC2_BLENDING_ON', 'MVC2_CHROMA_THRESHOLD', 'MVC2_EDGE_THRESHOLD',
'MVC2_ERROR', 'MVC2_LUMA_THRESHOLD', 'NEXT_NOT_AVAILABLE',
'NOISE_MARGIN', 'NONE', 'NUMBER_OF_FRAMES', 'NUMBER_OF_PARAMS',
'ORIENTATION', 'ORIGINAL_FORMAT', 'OpenCompressor',
'OpenDecompressor', 'PARAM_OUT_OF_RANGE', 'PREDICTED', 'PREROLL',
'ParamID', 'ParamNumber', 'ParamType', 'QUALITY_FACTOR',
'QUALITY_LEVEL', 'QueryAlgorithms', 'QueryMaxHeaderSize',
'QueryScheme', 'QuerySchemeFromName', 'RANGE_VALUE', 'RGB', 'RGB332',
'RGB8', 'RGBA', 'RGBX', 'RLE', 'RLE24', 'RTR', 'RTR1',
'RTR_QUALITY_LEVEL', 'SAMPLES_PER_TILE', 'SCHEME_BUSY',
'SCHEME_NOT_AVAILABLE', 'SPEED', 'STEREO_INTERLEAVED',
'STREAM_HEADERS', 'SetDefault', 'SetMax', 'SetMin', 'TILE_THRESHOLD',
'TOP_DOWN', 'ULAW', 'UNCOMPRESSED', 'UNCOMPRESSED_AUDIO',
'UNCOMPRESSED_VIDEO', 'UNKNOWN_SCHEME', 'VIDEO', 'VideoFormatName',
'Y', 'YCbCr', 'YCbCr422', 'YCbCr422DC', 'YCbCr422HC', 'YUV', 'YUV422',
'YUV422DC', 'YUV422HC', '__doc__', '__name__', 'cvt_type', 'error']


# This is a very inobtrusive test for the existence of the cl
# module and all it's attributes.

def main():
    # touch all the attributes of al without doing anything
    if verbose:
        print 'Touching cl module attributes...'
    for attr in clattrs:
        if verbose:
            print 'touching: ', attr
        getattr(cl, attr)

main()
