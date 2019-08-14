#include "OggVorbisAudioAssetDecoder.hpp"

#include <vorbis/vorbisfile.h>

#include <Usagi/Core/Logging.hpp>
#include <Usagi/Utility/RawResource.hpp>

namespace usagi
{
namespace
{
size_t read_func(
    void *buffer,
    size_t size,
    size_t count,
    void *stream)
{
    if(size == 0) return 0;
    auto &s = *static_cast<std::istream*>(stream);
    // LOG(trace, "read@{} {} {}", s.tellg(), size, count);
    s.read(static_cast<char*>(buffer), size * count);
    // LOG(trace, "fail? {} eof? {} bad? {}", s.fail(), s.eof(), s.bad());
    return s.gcount() / size;
}

int seek_func(void *stream, ogg_int64_t offset, int origin)
{
    auto &s = *static_cast<std::istream*>(stream);
    // LOG(trace, "seek@{2} {1} {0}", offset, [&]() {
    //     switch(origin)
    //     {
    //         case SEEK_SET:
    //             return "set";
    //         case SEEK_CUR:
    //             return "beg";
    //         case SEEK_END:
    //             return "end";
    //         default: return "???";
    //     }
    // }(), s.tellg());
    switch(origin)
    {
        case SEEK_SET:
            s.clear(); // clear potential EOF flag set by previous read()
            s.seekg(offset, s.beg);
            break;
        case SEEK_CUR:
            s.seekg(offset, s.cur);
            break;
        case SEEK_END:
            s.clear();
            s.seekg(offset, s.end);
            break;
        default:
            USAGI_THROW(std::logic_error { "invalid direction" });
    }
    // LOG(trace, " -> {}", s.tellg());
    return s.fail();
}

long tell_func(void *stream)
{
    auto &s = *static_cast<std::istream*>(stream);
    // LOG(trace, "tell {}", s.tellg());
    return static_cast<long>(s.tellg());
}

constexpr ov_callbacks ISTREAM_CALLBACKS = {
    &read_func, &seek_func, nullptr, &tell_func
};
}

// ref to https://github.com/xiph/vorbis/blob/master/examples/vorbisfile_example.c
AudioBuffer OggVorbisAudioAssetDecoder::operator()(std::istream &in) const
{
    LOG(info, "Decoding Ogg Vorbis stream");

    USAGI_TRIVIAL_RAW_RESOURCE(OggVorbis_File, vf, {
        // open stream
        switch(ov_open_callbacks(&in, &vf, nullptr, 0, ISTREAM_CALLBACKS))
        {
            case OV_EREAD:
                LOG(error, "OV_EREAD - A read from media returned an error.");
                USAGI_THROW(std::runtime_error("decoder error: OV_EREAD"));
            case OV_ENOTVORBIS:
                LOG(error, "OV_ENOTVORBIS - "
                    "Bitstream does not contain any Vorbis data.");
                USAGI_THROW(std::runtime_error("decoder error: OV_ENOTVORBIS"));
            case OV_EVERSION:
                LOG(error, "OV_EVERSION - Vorbis version mismatch.");
                USAGI_THROW(std::runtime_error("decoder error: OV_EVERSION"));
            case OV_EBADHEADER:
                LOG(error, "OV_EBADHEADER - Invalid Vorbis bitstream header.");
                USAGI_THROW(std::runtime_error("decoder error: OV_EBADHEADER"));
            case OV_EFAULT:
                LOG(error, "OV_EFAULT - Internal logic fault; "
                    "indicates a bug or heap/stack corruption.");
                USAGI_THROW(std::runtime_error("decoder error: OV_EFAULT"));
            default: break;
        }
    }, {
        ov_clear(&vf);
    });

    // read comments
    {
        char **ptr = ov_comment(vf.addr(), -1)->user_comments;
        while(*ptr)
        {
            LOG(info, "    [{}]", *ptr);
            ++ptr;
        }
    }

    AudioBuffer buf;

    // read stream info
    {
        vorbis_info *vi = ov_info(vf.addr(), -1);
        LOG(info, "    Number of channels: {}", vi->channels);
        LOG(info, "    Sample Rate:        {}",
            buf.sample_rate = vi->rate);
        LOG(info, "    Total Frames:       {}",
            buf.frames = ov_pcm_total(vf.addr(), -1));
        LOG(info, "    Total Time:         {}s",
            buf.time = ov_time_total(vf.addr(), -1));
        LOG(info, "    Encoder:            {}",
            ov_comment(vf.addr(), -1)->vendor);

        // prepare buffers
        buf.channels.resize(vi->channels);
        for(auto &&c : buf.channels)
        {
            c.resize(buf.frames);
        }
    }

    int current_section = 0;
    float **pcm = nullptr;
    std::size_t decoded_frames = 0;
    while(true)
    {
        auto ret = ov_read_float(vf.addr(), &pcm, 1024, &current_section);
        switch(ret)
        {
            case 0:
                LOG(info, "Ended reading stream, got {} frames",
                    decoded_frames);
                break;
            case OV_HOLE:
                LOG(warn, "OV_HOLE: there was an interruption in the data.");
                continue;
            case OV_EBADLINK:
                LOG(error, "OV_EBADLINK: an invalid stream section was "
                    "supplied to libvorbisfile, or the requested "
                    "link is corrupt.");
                USAGI_THROW(std::runtime_error("OV_EBADLINK"));
            case OV_EINVAL:
                LOG(error, "OV_EINVAL: the initial file headers couldn't "
                    "be read or are corrupt");
                USAGI_THROW(std::runtime_error("OV_EINVAL"));
            default: ;
        }
        if(ret == 0) break;
        // copy channel data
        assert(pcm);
        for(std::size_t i = 0; i < buf.channels.size(); ++i)
        {
            memcpy(
                &buf.channels[i][decoded_frames],
                pcm[i],
                ret * sizeof(float)
            );
        }
        decoded_frames += ret;
    }
    if(decoded_frames != buf.frames)
    {
        LOG(warn, "read fewer frames than declared");
        for(auto &&c : buf.channels)
        {
            c.resize(decoded_frames);
            c.shrink_to_fit();
        }
        buf.frames = decoded_frames;
    }

    return buf;
}
}
