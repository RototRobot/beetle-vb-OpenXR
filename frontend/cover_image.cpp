// SPDX-License-Identifier: GPL-2.0-or-later
#include "library.h"
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace bvb {
CoverImage load_cover_image(const std::filesystem::path& file, unsigned width, unsigned height) {
    if (!width || !height || width > 512 || height > 512) throw std::invalid_argument("Invalid cover thumbnail dimensions");
    const auto size = std::filesystem::file_size(file);
    if (!size || size > 32*1024*1024) throw std::runtime_error("Cover file too large or empty");
    const auto initialized = CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE) throw std::runtime_error("Cannot initialize cover decoder");
    struct ComCleanup { bool owned; ~ComCleanup() { if (owned) CoUninitialize(); } } cleanup{SUCCEEDED(initialized)};
    using Microsoft::WRL::ComPtr;
    auto check = [](HRESULT result) { if (FAILED(result)) throw std::runtime_error("Cannot decode cover image"); };
    ComPtr<IWICImagingFactory> factory;
    check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
    ComPtr<IWICBitmapDecoder> decoder;
    check(factory->CreateDecoderFromFilename(file.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnDemand,&decoder));
    ComPtr<IWICBitmapFrameDecode> frame; check(decoder->GetFrame(0,&frame));
    UINT source_width=0,source_height=0; check(frame->GetSize(&source_width,&source_height));
    if (!source_width || !source_height || source_width > 8192 || source_height > 8192
        || std::uint64_t(source_width)*source_height > 32*1024*1024) throw std::runtime_error("Unsupported cover dimensions");
    const double scale = std::min(double(width)/source_width,double(height)/source_height);
    CoverImage result;
    result.width = std::max(1u,static_cast<unsigned>(std::floor(source_width*scale)));
    result.height = std::max(1u,static_cast<unsigned>(std::floor(source_height*scale)));
    ComPtr<IWICBitmapScaler> scaler; check(factory->CreateBitmapScaler(&scaler));
    check(scaler->Initialize(frame.Get(),result.width,result.height,WICBitmapInterpolationModeFant));
    ComPtr<IWICFormatConverter> converter; check(factory->CreateFormatConverter(&converter));
    check(converter->Initialize(scaler.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
    result.pixels.resize(result.width*result.height*4);
    check(converter->CopyPixels(nullptr,result.width*4,static_cast<UINT>(result.pixels.size()),result.pixels.data()));
    return result;
}
}
