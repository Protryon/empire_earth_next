// JPEG image decoder on libjpeg, reading through a FileReader
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <setjmp.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyAnimation.h"
#include "ee/wonapi/WONLobbyImageDecoder.h"
namespace WONJPG { extern "C" {
#include "ee/wonapi/codec/jpeglib.h"
#include "ee/wonapi/codec/jerror.h"
struct my_error_mgr { jpeg_error_mgr pub; jmp_buf setjmp_buffer; };
typedef my_error_mgr* my_error_ptr;
struct my_source_mgr {
    jpeg_source_mgr pub;
    WONAPI::FileReader* mReader;
    boolean start_of_file;
    JOCTET buffer[4096];
};
typedef my_source_mgr* my_src_ptr;

// libjpeg error handler: prints the message and jumps back to the decoder's setjmp (emitted at 0x007d2cb0)
static void my_error_exit(j_common_ptr cinfo)
{
    my_error_ptr error=(my_error_ptr)cinfo->err;
    (*cinfo->err->output_message)(cinfo);
    longjmp(error->setjmp_buffer,1);
}
// Source manager callbacks: init marks the start of the file, term does nothing
static void init_source(j_decompress_ptr cinfo)
{
    my_src_ptr source=(my_src_ptr)cinfo->src;
    source->start_of_file=TRUE;
}
static void term_source(j_decompress_ptr) {}

// FUNCTION: WON 0x007d2b30
// Refills the libjpeg input buffer with up to 4 KB from the file reader; at end of data, errors on an empty file
// or warns and inserts a fake end-of-image marker
static boolean fill_input_buffer(j_decompress_ptr cinfo)
{
    my_src_ptr source=(my_src_ptr)cinfo->src;
    size_t count=source->mReader->ReadMaxBytes(source->buffer,4096);
    if(count<=0) {
        if(source->start_of_file)
            ERREXIT(cinfo,JERR_INPUT_EMPTY);
        WARNMS(cinfo,JWRN_JPEG_EOF);
        source->buffer[0]=(JOCTET)0xff;
        source->buffer[1]=(JOCTET)JPEG_EOI;
        count=2;
    }
    source->pub.next_input_byte=source->buffer;
    source->pub.bytes_in_buffer=count;
    source->start_of_file=FALSE;
    return TRUE;
}
// FUNCTION: WON 0x007d2ba0
// Skips input bytes, first from the buffer, then by skipping the rest in the file reader
static void skip_input_data(j_decompress_ptr cinfo,long bytes)
{
    my_src_ptr source=(my_src_ptr)cinfo->src;
    if(bytes<=source->pub.bytes_in_buffer) {
        source->pub.bytes_in_buffer-=bytes;
        source->pub.next_input_byte+=bytes;
    } else {
        bytes-=source->pub.bytes_in_buffer;
        source->pub.bytes_in_buffer=0;
        source->mReader->SkipBytes(bytes);
    }
}
// FUNCTION: WON 0x007d2ac0
// Sets up a libjpeg source manager that reads from a FileReader (allocated once per decompressor)
void jpeg_filereader_src(j_decompress_ptr cinfo,WONAPI::FileReader* reader)
{
    if(cinfo->src==NULL)
        cinfo->src=(jpeg_source_mgr*)(*cinfo->mem->alloc_small)(reinterpret_cast<j_common_ptr>(cinfo),JPOOL_PERMANENT,sizeof(my_source_mgr));
    my_src_ptr source=(my_src_ptr)cinfo->src;
    source->mReader=reader;
    source->pub.init_source=init_source;
    source->pub.fill_input_buffer=fill_input_buffer;
    source->pub.skip_input_data=skip_input_data;
    source->pub.resync_to_restart=jpeg_resync_to_restart;
    source->pub.term_source=term_source;
    source->pub.bytes_in_buffer=0;
    source->pub.next_input_byte=NULL;
}
}}
using namespace WONJPG;
namespace WONAPI {
// FUNCTION: WON 0x007d2a20
// Registers one JPEG decoder with a multi-decoder for the jpg, jpe, jpeg and pjpeg extensions
void JPGDecoder::Add(MultiImageDecoder* decoder)
{
    JPGDecoder* jpg=new JPGDecoder;
    decoder->AddDecoder(jpg,"jpg");
    decoder->AddDecoder(jpg,"jpe");
    decoder->AddDecoder(jpg,"jpeg");
    decoder->AddDecoder(jpg,"pjpeg");
}
// FUNCTION: WON 0x007d2f50
// Creates a new JPEG decoder
ImageDecoder* JPGDecoder::Duplicate() { return new JPGDecoder; }

// FUNCTION: WON 0x007d2bf0
// Reads the JPEG header to get the image size; false on a libjpeg error
bool JPGDecoder::DoGetAttrib(ImageAttributes& attributes)
{
    jpeg_decompress_struct cinfo;
    my_error_mgr error;
    cinfo.err=jpeg_std_error(&error.pub);
    error.pub.error_exit=my_error_exit;
    if(setjmp(error.setjmp_buffer)) {
        jpeg_destroy_decompress(&cinfo);
        return false;
    }
    jpeg_create_decompress(&cinfo);
    jpeg_filereader_src(&cinfo,this->mFileReader);
    (void)jpeg_read_header(&cinfo,TRUE);
    (void)jpeg_start_decompress(&cinfo);
    attributes.mWidth=cinfo.output_width;
    attributes.mHeight=cinfo.output_height;
    jpeg_destroy_decompress(&cinfo);
    return true;
}

// FUNCTION: WON 0x007d2cd0
// Decodes the JPEG into a 32-bit raw image (greyscale expanded to RGB); NULL on a libjpeg error, leaking any
// image already allocated
SmartPtr<RawImage> JPGDecoder::DoDecodeRaw()
{
    jpeg_decompress_struct cinfo;
    my_error_mgr error;
    cinfo.err=jpeg_std_error(&error.pub);
    error.pub.error_exit=my_error_exit;
    if(setjmp(error.setjmp_buffer)) {
        jpeg_destroy_decompress(&cinfo);
        return NULL;
    }
    jpeg_create_decompress(&cinfo);
    jpeg_filereader_src(&cinfo,this->mFileReader);
    (void)jpeg_read_header(&cinfo,TRUE);
    (void)jpeg_start_decompress(&cinfo);
    int stride=cinfo.output_width*cinfo.output_components;
    JSAMPARRAY rows=(*cinfo.mem->alloc_sarray)(reinterpret_cast<j_common_ptr>(&cinfo),JPOOL_IMAGE,stride,1);
    int width=cinfo.output_width,height=cinfo.output_height;
    // Allocates the RawImage32 directly (not through CreateRawImage32)
    SmartPtr<RawImage32> image=new RawImage32(width,height);
    image->StartRowTraversal(0);
    while(cinfo.output_scanline<cinfo.output_height) {
        (void)jpeg_read_scanlines(&cinfo,rows,1);
        unsigned char* pixel=rows[0];
        if(cinfo.out_color_space==JCS_GRAYSCALE) {
            for(int x=0;x<width;++x,++pixel) {
                DWORD color=pixel[0]|(pixel[0]<<8)|(pixel[0]<<16);
                image->SetRowPixel(color);
                image->NextRowPixel();
            }
        } else {
            for(int x=0;x<width;++x,pixel+=3) {
                DWORD color=pixel[2]|(pixel[1]<<8)|(pixel[0]<<16);
                image->SetRowPixel(color);
                image->NextRowPixel();
            }
        }
        image->NextRow();
    }
    (void)jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    return image.get();
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007d2b20
// (_init_source)
// TEMPLATE: WON 0x007d2cb0
// (_my_error_exit)
// TEMPLATE: WON 0x007d2fa0
// TEMPLATE: WON 0x007d2fc0
// TEMPLATE: WON 0x007d2be0
