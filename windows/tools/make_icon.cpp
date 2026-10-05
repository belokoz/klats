// Generates res/klats.ico from the 1024-pixel app icon of the site (site/assets/icon-1024.png):
// every size Windows asks for, each scaled down from the original with a high-quality filter and
// stored as PNG inside the .ico, which Windows reads since Vista.
//
//   make_icon.exe <source png> <output ico>
#include <windows.h>
#include <wincodec.h>

#include <cstdio>
#include <cstring>
#include <iterator>
#include <vector>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

namespace {

template <class T>
struct Com {
    T* p = nullptr;
    ~Com() {
        if (p) p->Release();
    }
    T** operator&() { return &p; }
    T* operator->() const { return p; }
};

bool check(HRESULT result, const char* what) {
    if (SUCCEEDED(result)) return true;
    std::fprintf(stderr, "%s failed: 0x%08lX\n", what, static_cast<unsigned long>(result));
    return false;
}

std::vector<BYTE> encodePng(IWICImagingFactory* factory, IWICBitmapSource* source) {
    std::vector<BYTE> bytes;
    Com<IStream> stream;
    Com<IWICBitmapEncoder> encoder;
    Com<IWICBitmapFrameEncode> frame;
    if (!check(CreateStreamOnHGlobal(nullptr, TRUE, &stream), "stream") ||
        !check(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder), "encoder") ||
        !check(encoder->Initialize(stream.p, WICBitmapEncoderNoCache), "encoder init") ||
        !check(encoder->CreateNewFrame(&frame, nullptr), "frame") || !check(frame->Initialize(nullptr), "frame init") ||
        !check(frame->WriteSource(source, nullptr), "write") || !check(frame->Commit(), "frame commit") ||
        !check(encoder->Commit(), "encoder commit")) {
        return bytes;
    }
    HGLOBAL memory = nullptr;
    GetHGlobalFromStream(stream.p, &memory);
    STATSTG stat{};
    stream->Stat(&stat, STATFLAG_NONAME);
    auto size = static_cast<size_t>(stat.cbSize.QuadPart);
    const BYTE* data = static_cast<const BYTE*>(GlobalLock(memory));
    bytes.assign(data, data + size);
    GlobalUnlock(memory);
    return bytes;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: make_icon <source png> <output ico>\n");
        return 2;
    }
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) return 1;
    int result = 1;
    {
        Com<IWICImagingFactory> factory;
        Com<IWICBitmapDecoder> decoder;
        Com<IWICBitmapFrameDecode> source;
        if (check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)), "factory") &&
            check(factory->CreateDecoderFromFilename(argv[1], nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder), "decoder") &&
            check(decoder->GetFrame(0, &source), "frame")) {
            const UINT sizes[] = {16, 20, 24, 32, 40, 48, 64, 256};
            std::vector<std::vector<BYTE>> images;
            for (UINT size : sizes) {
                Com<IWICBitmapScaler> scaler;
                Com<IWICFormatConverter> converter;
                if (!check(factory->CreateBitmapScaler(&scaler), "scaler") ||
                    !check(scaler->Initialize(source.p, size, size, WICBitmapInterpolationModeHighQualityCubic), "scale") ||
                    !check(factory->CreateFormatConverter(&converter), "converter") ||
                    !check(converter->Initialize(scaler.p, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0,
                                                 WICBitmapPaletteTypeCustom),
                           "convert")) {
                    break;
                }
                images.push_back(encodePng(factory.p, converter.p));
                if (images.back().empty()) break;
            }
            FILE* out = nullptr;
            if (images.size() == std::size(sizes) && _wfopen_s(&out, argv[2], L"wb") == 0 && out) {
                // ICONDIR, then one ICONDIRENTRY per image, then the images.
                WORD header[3] = {0, 1, static_cast<WORD>(images.size())};
                std::fwrite(header, sizeof header, 1, out);
                DWORD offset = sizeof header + static_cast<DWORD>(images.size()) * 16;
                for (size_t i = 0; i < images.size(); ++i) {
                    BYTE side = sizes[i] >= 256 ? 0 : static_cast<BYTE>(sizes[i]);
                    BYTE entry[16] = {side, side, 0, 0, 1, 0, 32, 0};
                    DWORD length = static_cast<DWORD>(images[i].size());
                    std::memcpy(entry + 8, &length, 4);
                    std::memcpy(entry + 12, &offset, 4);
                    std::fwrite(entry, sizeof entry, 1, out);
                    offset += length;
                }
                for (const auto& image : images) std::fwrite(image.data(), 1, image.size(), out);
                std::fclose(out);
                std::printf("%zu sizes written\n", images.size());
                result = 0;
            }
        }
    }
    CoUninitialize();
    return result;
}
