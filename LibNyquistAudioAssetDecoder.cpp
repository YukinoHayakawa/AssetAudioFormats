#include "LibNyquistAudioAssetDecoder.hpp"

#include <libnyquist/Common.h>
#include <libnyquist/Decoders.h>

#include <Usagi/Utility/Stream.hpp>

namespace usagi
{
namespace
{
DataFormat translate(nqr::PCMFormat format)
{
    switch(format)
    {
        case nqr::PCM_U8: return DataFormat::UINT8;
        case nqr::PCM_S8: return DataFormat::INT8;
        case nqr::PCM_16:return DataFormat::INT16;
        case nqr::PCM_24:return DataFormat::INT24;
        case nqr::PCM_32:return DataFormat::INT32;
        case nqr::PCM_64:return DataFormat::INT64;
        case nqr::PCM_FLT:return DataFormat::FLOAT32;
        case nqr::PCM_DBL:return DataFormat::FLOAT64;
        default: throw std::runtime_error("invalid format");
    }
}
}

AudioBuffer LibNyquistAudioAssetDecoder::operator()(std::istream &in) const
{
    const auto bytes = readStreamToByteVector(in);

    nqr::AudioData decoded;
    nqr::NyquistIO loader;
    loader.Load(&decoded, bytes);

    AudioBuffer ret;

    // todo use source format
    ret.format.format = DataFormat::FLOAT32;
    ret.format.interleaved = true;
    ret.format.num_channels = decoded.channelCount;
    ret.format.sample_rate = decoded.sampleRate;
    ret.num_frames = decoded.samples.size() / decoded.channelCount;
    ret.samples = std::move(decoded.samples);

    return ret;
}
}
