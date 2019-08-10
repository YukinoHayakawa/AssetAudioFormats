#pragma once

#include <istream>

#include <Usagi/Runtime/Audio/AudioBuffer.hpp>

namespace usagi
{
struct LibNyquistAudioAssetDecoder
{
    AudioBuffer operator()(std::istream &in) const;
};
}
