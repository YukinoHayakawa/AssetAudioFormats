#include "OggVorbisAudioAssetDecoder.hpp"

#include <vorbis/vorbisfile.h>

#include <Usagi/Core/Logging.hpp>

namespace usagi
{
namespace
{
size_t read_func(
    void *ptr,
    size_t size,
    size_t nmemb,
    void *datasource)
{
    auto &s = *static_cast<std::istream*>(datasource);
    s.read(static_cast<char*>(ptr), size * nmemb);
    return s.gcount() / size;
}

int seek_func(void *datasource, ogg_int64_t offset, int whence)
{
    auto &s = *static_cast<std::istream*>(datasource);
    s.seekg(offset, whence);
    return !bool(s);
}

long tell_func(void *datasource)
{
    auto &s = *static_cast<std::istream*>(datasource);
    return static_cast<long>(s.tellg());
}

constexpr ov_callbacks STREAM_CALLBACKS = {
    &read_func, &seek_func, nullptr, &tell_func
};
}

// ref to https://github.com/xiph/vorbis/blob/master/examples/vorbisfile_example.c
AudioBuffer OggVorbisAudioAssetDecoder::operator()(std::istream &in) const
{
    OggVorbis_File vf;

    LOG(info, "Decoding Ogg Vorbis stream");

    // open stream
    switch(ov_open_callbacks(&in, &vf, nullptr, 0, STREAM_CALLBACKS))
    {
        case OV_EREAD:
            LOG(error, "OV_EREAD - A read from media returned an error.");
            throw std::runtime_error("decoder error: OV_EREAD");
        case OV_ENOTVORBIS:
            LOG(error, "OV_ENOTVORBIS - "
                "Bitstream does not contain any Vorbis data.");
            throw std::runtime_error("decoder error: OV_ENOTVORBIS");
        case OV_EVERSION:
            LOG(error, "OV_EVERSION - Vorbis version mismatch.");
            throw std::runtime_error("decoder error: OV_EVERSION");
        case OV_EBADHEADER:
            LOG(error, "OV_EBADHEADER - Invalid Vorbis bitstream header.");
            throw std::runtime_error("decoder error: OV_EBADHEADER");
        case OV_EFAULT:
            LOG(error, "OV_EFAULT - Internal logic fault; "
                "indicates a bug or heap/stack corruption.");
            throw std::runtime_error("decoder error: OV_EFAULT");
        default: break;
    }

    // read comments
    {
        char **ptr = ov_comment(&vf, -1)->user_comments;
        while(*ptr)
        {
            LOG(info, "    [{}]", *ptr);
            ++ptr;
        }
    }

    AudioBuffer buf;

    // read stream info
    {
        vorbis_info *vi = ov_info(&vf, -1);
        LOG(info, "    Number of channels: {}", vi->channels);
        LOG(info, "    Sample Rate:        {}",
            buf.sample_rate = vi->rate);
        LOG(info, "    Total Frames:       {}",
            buf.frames = ov_pcm_total(&vf, -1));
        LOG(info, "    Total Time:         {}s",
            buf.time = ov_time_total(&vf, -1));
        LOG(info, "    Encoder:            {}",
            ov_comment(&vf, -1)->vendor);

        // prepare buffers
        buf.channels.resize(vi->channels);
        for(auto &&c : buf.channels)
        {
            c.resize(buf.frames);
        }
    }

    int current_section = 0;
    float **pcm = nullptr;
    std::size_t num_samples = 0;
    auto ctn = true;
    while(ctn)
    {
        auto ret = ov_read_float(&vf, &pcm, 4096, &current_section);
        switch(ret)
        {
            case 0:
                LOG(info, "Ended reading stream");
                ctn = false;
                break;
            case OV_HOLE:
                LOG(error, "OV_HOLE: there was an interruption in the data.");
                throw std::runtime_error("OV_HOLE");
            case OV_EBADLINK:
                LOG(error, "OV_EBADLINK: an invalid stream section was "
                    "supplied to libvorbisfile, or the requested "
                    "link is corrupt.");
                throw std::runtime_error("OV_EBADLINK");
            case OV_EINVAL:
                LOG(error, "OV_EINVAL: the initial file headers couldn't "
                    "be read or are corrupt");
                throw std::runtime_error("OV_EINVAL");
            default: ;
        }
        // copy channel data
        for(std::size_t i = 0; i < buf.channels.size(); ++i)
        {
            memcpy(
                buf.channels[i].data() + num_samples,
                pcm[i],
                ret
            );
        }
        num_samples += ret;
    }
    assert(num_samples == buf.frames);

    return buf;
}
}
