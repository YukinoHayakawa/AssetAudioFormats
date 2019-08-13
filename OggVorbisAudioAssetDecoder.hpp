#pragma once

#include <istream>

#include <Usagi/Runtime/Audio/AudioBuffer.hpp>

namespace usagi
{
struct OggVorbisAudioAssetDecoder
{
    AudioBuffer operator()(std::istream &in) const;
};
}
