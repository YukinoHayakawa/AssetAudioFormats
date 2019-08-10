#pragma once

#include "LibNyquistAudioAssetDecoder.hpp"

namespace usagi
{
struct AssetLoadingContext;

struct PassthroughAudioAssetConverter
{
    using DefaultDecoder = LibNyquistAudioAssetDecoder;

    std::shared_ptr<AudioBuffer> operator()(
        AssetLoadingContext *ctx,
        AudioBuffer &&buffer
    ) const;
};
}
