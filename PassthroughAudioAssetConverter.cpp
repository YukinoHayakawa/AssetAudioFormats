#include "PassthroughAudioAssetConverter.hpp"

namespace usagi
{
std::shared_ptr<AudioBuffer> PassthroughAudioAssetConverter::operator()(
    AssetLoadingContext *ctx,
    AudioBuffer &&buffer) const
{
    return std::make_shared<AudioBuffer>(std::move(buffer));
}
}
