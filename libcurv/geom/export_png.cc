// Copyright 2016-2018 Doug Moen
// Licensed under the Apache License, version 2.0
// See accompanying file LICENSE or https://www.apache.org/licenses/LICENSE-2.0

#include <libcurv/geom/export_png.h>

#include <libcurv/geom/shape.h>
#include <libcurv/geom/viewer/viewer.h>
#include <libcurv/context.h>
#include <libcurv/exception.h>
#include <libcurv/output_file.h>

#include <gl/texture.h>
#include <iostream>
#include <cstring>
#include <cerrno>

namespace curv { namespace geom {

#define STB_IMAGE_IMPLEMENTATION
#include "std/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "std/stb_image_write.h"

// The input is 4 bytes per pixel (RGBA).
// The output is 3 bytes per pixel (RGB), and the image is flipped on Y.
void
write_png_rgb(
    const std::string& path, unsigned char* pixels, int width, int height)
{
    using uchar = unsigned char;
    std::unique_ptr<uchar[]> result(new uchar[width*height*3]);

    for (int y = 0; y < height; ++y) {
        uchar* irow = &pixels[(height - 1 - y) * width * 4];
        uchar* orow = &result[y * width * 3];
        for (int x = 0; x < width; ++x) {
            uchar* ipix = &irow[x * 4];
            uchar* opix = &orow[x * 3];
            opix[0] = ipix[0];
            opix[1] = ipix[1];
            opix[2] = ipix[2];
        }
    }

    int r = stbi_write_png(path.c_str(), width, height, 3, result.get(), width * 3);
    if (!r) {
        throw Exception({},
            stringify("Can't create file ",path,": ", strerror(errno)));
    }
}

void
export_png(
    const Shape_Program& shape,
    const Image_Export& p,
    Output_File& ofile)
{
    glm::dvec2 shape_size = shape.bbox_.size2();
    glm::dvec2 image_coverage = glm::dvec2(p.size) * p.pixel_size;
    glm::dvec2 overpaint = image_coverage - shape_size;
    glm::dvec2 origin = {
        shape.bbox_.xmin - overpaint.x/2.0 + p.pixel_size/2.0,
        shape.bbox_.ymax + overpaint.y/2.0 - p.pixel_size/2.0,
    };
    (void) origin; // TODO

    Frag_Export opts;
    opts.aa_ = p.aa_;
    opts.taa_ = p.taa_;
    opts.delay_ = p.delay_;

    viewer::Viewer v;
    v.window_pos_and_size_.z = p.size.x;
    v.window_pos_and_size_.w = p.size.y;
    v.headless_ = true;
    v.set_shape(shape, opts);
    v.open();
    v.current_time_ = p.time;
    v.draw_frame();
#if 1
    // On macOS, the second call to draw_frame() is needed, or glReadPixels
    // will store zeroes in `pixels`. (Calling glFinish() doesn't help.)
    // I think the problem is related to double buffering: only after the
    // second call to draw_frame() do both of the buffers contain the image.
    // On Linux, I don't need 2 calls.
    v.current_time_ = p.time;
    v.draw_frame();
#endif
    glFinish();

    // Read the pixels into CPU memory.
    // We request GL_RGBA format (which has 4 byte alignment), instead of GL_RGB
    // format (which has 3 byte alignment), to avoid a problem with the driver
    // substituting formats due to alignment.
    // See: https://www.khronos.org/opengl/wiki/Common_Mistakes
    std::unique_ptr<unsigned char[]> pixels(new unsigned char[p.size.x*p.size.y*4]);
    glGetError();
    glReadPixels(0, 0, p.size.x, p.size.y, GL_RGBA, GL_UNSIGNED_BYTE, pixels.get());
    auto err = glGetError();

    v.close();
#if 0
    std::cerr << "err="<<int(err)<<" RGBA[0,0]: "
        <<int(pixels[0])<<","
        <<int(pixels[1])<<","
        <<int(pixels[2])<<","
        <<int(pixels[3])<<"\n";
#else
    (void) err;
#endif
    write_png_rgb(ofile.path().c_str(), pixels.get(), p.size.x, p.size.y);
}

}} // namespace