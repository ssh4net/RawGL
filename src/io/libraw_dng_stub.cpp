// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#if defined(_MSC_VER)

class dng_abort_sniffer;
class dng_fingerprint;
class dng_host;
class dng_ifd;
class dng_image;
class dng_lossy_compressed_image;
class dng_rect;

class dng_memory_allocator {
};

class dng_stream {
protected:
    dng_stream(dng_abort_sniffer* sniffer, unsigned int bufferSize, unsigned long long offsetInOriginalFile);
    virtual void DoSetLength(unsigned long long length);
    virtual void DoWrite(const void* data, unsigned int count, unsigned long long offset);

public:
    virtual ~dng_stream();
    void SetReadPosition(unsigned long long offset);
    unsigned int TagValue_uint32(unsigned int tagType);
    virtual void CopyToStream(dng_stream& stream, unsigned long long count);
};

class dng_host {
public:
    dng_memory_allocator& Allocator();
};

class dng_pixel_buffer {
public:
    dng_pixel_buffer();
    dng_pixel_buffer& operator=(const dng_pixel_buffer& other);
    virtual ~dng_pixel_buffer();
};

class dng_negative {
public:
    void BuildStage2Image(dng_host& host);
    void BuildStage3Image(dng_host& host, int rawImageDigest);
};

class dng_simple_image {
public:
    dng_simple_image(const dng_rect& bounds, unsigned int planes, unsigned int pixelType, dng_memory_allocator& allocator);
};

class dng_info {
public:
    dng_info();
    virtual ~dng_info();
    virtual void Parse(dng_host& host, dng_stream& stream);
    virtual void PostParse(dng_host& host);
    virtual bool IsValidDNG();
};

class dng_read_image {
public:
    dng_read_image();
    virtual ~dng_read_image();
    virtual void Read(dng_host& host,
                      const dng_ifd& ifd,
                      dng_stream& stream,
                      dng_image& image,
                      dng_lossy_compressed_image* lossyImage,
                      dng_fingerprint* fingerprint);
};

void
Throw_dng_error(int err, const char* message, const char* subMessage, bool silent)
{
    (void)err;
    (void)message;
    (void)subMessage;
    (void)silent;
}

bool
SafeInt32Sub(int a, int b, int* result)
{
    if (result) {
        *result = a - b;
    }
    return true;
}

unsigned int
TagTypeSize(unsigned int tagType)
{
    (void)tagType;
    return 0u;
}

dng_stream::dng_stream(dng_abort_sniffer* sniffer, unsigned int bufferSize, unsigned long long offsetInOriginalFile)
{
    (void)sniffer;
    (void)bufferSize;
    (void)offsetInOriginalFile;
}

dng_stream::~dng_stream() = default;

void
dng_stream::DoSetLength(unsigned long long length)
{
    (void)length;
}

void
dng_stream::DoWrite(const void* data, unsigned int count, unsigned long long offset)
{
    (void)data;
    (void)count;
    (void)offset;
}

void
dng_stream::SetReadPosition(unsigned long long offset)
{
    (void)offset;
}

unsigned int
dng_stream::TagValue_uint32(unsigned int tagType)
{
    (void)tagType;
    return 0u;
}

void
dng_stream::CopyToStream(dng_stream& stream, unsigned long long count)
{
    (void)stream;
    (void)count;
}

dng_memory_allocator&
dng_host::Allocator()
{
    static dng_memory_allocator allocator;
    return allocator;
}

dng_pixel_buffer::dng_pixel_buffer() = default;

dng_pixel_buffer&
dng_pixel_buffer::operator=(const dng_pixel_buffer& other)
{
    (void)other;
    return *this;
}

dng_pixel_buffer::~dng_pixel_buffer() = default;

void
dng_negative::BuildStage2Image(dng_host& host)
{
    (void)host;
}

void
dng_negative::BuildStage3Image(dng_host& host, int rawImageDigest)
{
    (void)host;
    (void)rawImageDigest;
}

dng_simple_image::dng_simple_image(const dng_rect& bounds,
                                   unsigned int planes,
                                   unsigned int pixelType,
                                   dng_memory_allocator& allocator)
{
    (void)bounds;
    (void)planes;
    (void)pixelType;
    (void)allocator;
}

dng_info::dng_info() = default;

dng_info::~dng_info() = default;

void
dng_info::Parse(dng_host& host, dng_stream& stream)
{
    (void)host;
    (void)stream;
}

void
dng_info::PostParse(dng_host& host)
{
    (void)host;
}

bool
dng_info::IsValidDNG()
{
    return false;
}

dng_read_image::dng_read_image() = default;

dng_read_image::~dng_read_image() = default;

void
dng_read_image::Read(dng_host& host,
                     const dng_ifd& ifd,
                     dng_stream& stream,
                     dng_image& image,
                     dng_lossy_compressed_image* lossyImage,
                     dng_fingerprint* fingerprint)
{
    (void)host;
    (void)ifd;
    (void)stream;
    (void)image;
    (void)lossyImage;
    (void)fingerprint;
}

#endif
