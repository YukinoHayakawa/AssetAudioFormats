#pragma once

#include "OggVorbisAudioAssetDecoder.hpp"

namespace usagi
{
struct AssetLoadingContext;

struct PassthroughAudioAssetConverter
{
    using DefaultDecoder = OggVorbisAudioAssetDecoder;

    std::shared_ptr<AudioBuffer> operator()(
        AssetLoadingContext *ctx,
        AudioBuffer &&buffer
    ) const;
};
}
