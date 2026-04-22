// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "metadata_internal.h"

#include <bit>
#include <cstring>
#include <iomanip>
#include <map>
#include <memory>
#include <sstream>
#include <string_view>
#include <vector>

#if defined(RAWGL_HAS_OPENMETA)
#include <openmeta/container_scan.h>
#include <openmeta/interop_export.h>
#include <openmeta/mapped_file.h>
#include <openmeta/oiio_adapter.h>
#include <openmeta/meta_flags.h>
#include <openmeta/meta_key.h>
#include <openmeta/meta_store.h>
#include <openmeta/meta_value.h>
#include <openmeta/simple_meta.h>
#endif

namespace rawgl::io {
#if defined(RAWGL_HAS_OPENMETA)

struct MetadataDocumentStorage final {
    openmeta::MetaStore store;
};

namespace {

static openmeta::ExportNameStyle
to_openmeta_name_style(const MetadataNameStyle style) noexcept
{
    switch (style) {
    case MetadataNameStyle::Canonical: return openmeta::ExportNameStyle::Canonical;
    case MetadataNameStyle::XmpPortable: return openmeta::ExportNameStyle::XmpPortable;
    case MetadataNameStyle::Oiio: return openmeta::ExportNameStyle::Oiio;
    }

    return openmeta::ExportNameStyle::Canonical;
}

static openmeta::ExportNamePolicy
to_openmeta_name_policy(const MetadataNamePolicy policy) noexcept
{
    switch (policy) {
    case MetadataNamePolicy::Spec: return openmeta::ExportNamePolicy::Spec;
    case MetadataNamePolicy::ExifToolAlias: return openmeta::ExportNamePolicy::ExifToolAlias;
    }

    return openmeta::ExportNamePolicy::Spec;
}

static MetadataKeyKind
to_rawgl_key_kind(const openmeta::MetaKeyKind kind) noexcept
{
    switch (kind) {
    case openmeta::MetaKeyKind::ExifTag: return MetadataKeyKind::ExifTag;
    case openmeta::MetaKeyKind::Comment: return MetadataKeyKind::Comment;
    case openmeta::MetaKeyKind::ExrAttribute: return MetadataKeyKind::ExrAttribute;
    case openmeta::MetaKeyKind::IptcDataset: return MetadataKeyKind::IptcDataset;
    case openmeta::MetaKeyKind::XmpProperty: return MetadataKeyKind::XmpProperty;
    case openmeta::MetaKeyKind::IccHeaderField: return MetadataKeyKind::IccHeaderField;
    case openmeta::MetaKeyKind::IccTag: return MetadataKeyKind::IccTag;
    case openmeta::MetaKeyKind::PhotoshopIrb: return MetadataKeyKind::PhotoshopIrb;
    case openmeta::MetaKeyKind::PhotoshopIrbField: return MetadataKeyKind::PhotoshopIrbField;
    case openmeta::MetaKeyKind::GeotiffKey: return MetadataKeyKind::GeotiffKey;
    case openmeta::MetaKeyKind::PrintImField: return MetadataKeyKind::PrintImField;
    case openmeta::MetaKeyKind::BmffField: return MetadataKeyKind::BmffField;
    case openmeta::MetaKeyKind::JumbfField: return MetadataKeyKind::JumbfField;
    case openmeta::MetaKeyKind::JumbfCborKey: return MetadataKeyKind::JumbfCborKey;
    case openmeta::MetaKeyKind::PngText: return MetadataKeyKind::PngText;
    }

    return MetadataKeyKind::ExifTag;
}

static MetadataValueKind
to_rawgl_value_kind(const openmeta::MetaValueKind kind) noexcept
{
    switch (kind) {
    case openmeta::MetaValueKind::Empty: return MetadataValueKind::Empty;
    case openmeta::MetaValueKind::Scalar: return MetadataValueKind::Scalar;
    case openmeta::MetaValueKind::Array: return MetadataValueKind::Array;
    case openmeta::MetaValueKind::Bytes: return MetadataValueKind::Bytes;
    case openmeta::MetaValueKind::Text: return MetadataValueKind::Text;
    }

    return MetadataValueKind::Empty;
}

static MetadataElementType
to_rawgl_element_type(const openmeta::MetaElementType type) noexcept
{
    switch (type) {
    case openmeta::MetaElementType::U8: return MetadataElementType::U8;
    case openmeta::MetaElementType::I8: return MetadataElementType::I8;
    case openmeta::MetaElementType::U16: return MetadataElementType::U16;
    case openmeta::MetaElementType::I16: return MetadataElementType::I16;
    case openmeta::MetaElementType::U32: return MetadataElementType::U32;
    case openmeta::MetaElementType::I32: return MetadataElementType::I32;
    case openmeta::MetaElementType::U64: return MetadataElementType::U64;
    case openmeta::MetaElementType::I64: return MetadataElementType::I64;
    case openmeta::MetaElementType::F32: return MetadataElementType::F32;
    case openmeta::MetaElementType::F64: return MetadataElementType::F64;
    case openmeta::MetaElementType::URational: return MetadataElementType::URational;
    case openmeta::MetaElementType::SRational: return MetadataElementType::SRational;
    }

    return MetadataElementType::U8;
}

static MetadataTextEncoding
to_rawgl_text_encoding(const openmeta::TextEncoding encoding) noexcept
{
    switch (encoding) {
    case openmeta::TextEncoding::Unknown: return MetadataTextEncoding::Unknown;
    case openmeta::TextEncoding::Ascii: return MetadataTextEncoding::Ascii;
    case openmeta::TextEncoding::Utf8: return MetadataTextEncoding::Utf8;
    case openmeta::TextEncoding::Utf16LE: return MetadataTextEncoding::Utf16LE;
    case openmeta::TextEncoding::Utf16BE: return MetadataTextEncoding::Utf16BE;
    }

    return MetadataTextEncoding::Unknown;
}

static MetadataEntryFlags
to_rawgl_entry_flags(const openmeta::EntryFlags flags) noexcept
{
    MetadataEntryFlags result = MetadataEntryFlags::None;

    if (openmeta::any(flags, openmeta::EntryFlags::Deleted)) {
        result = result | MetadataEntryFlags::Deleted;
    }
    if (openmeta::any(flags, openmeta::EntryFlags::Dirty)) {
        result = result | MetadataEntryFlags::Dirty;
    }
    if (openmeta::any(flags, openmeta::EntryFlags::Derived)) {
        result = result | MetadataEntryFlags::Derived;
    }
    if (openmeta::any(flags, openmeta::EntryFlags::Truncated)) {
        result = result | MetadataEntryFlags::Truncated;
    }
    if (openmeta::any(flags, openmeta::EntryFlags::Unreadable)) {
        result = result | MetadataEntryFlags::Unreadable;
    }
    if (openmeta::any(flags, openmeta::EntryFlags::ContextualName)) {
        result = result | MetadataEntryFlags::ContextualName;
    }

    return result;
}

static std::string
mapped_file_error_string(const openmeta::MappedFileStatus status)
{
    switch (status) {
    case openmeta::MappedFileStatus::Ok: return {};
    case openmeta::MappedFileStatus::OpenFailed: return "failed to open metadata file";
    case openmeta::MappedFileStatus::StatFailed: return "failed to stat metadata file";
    case openmeta::MappedFileStatus::TooLarge: return "metadata file is too large";
    case openmeta::MappedFileStatus::MapFailed: return "failed to map metadata file";
    }

    return "unknown metadata file mapping error";
}

static std::string
scan_status_error_string(const openmeta::ScanStatus status)
{
    switch (status) {
    case openmeta::ScanStatus::Ok: return {};
    case openmeta::ScanStatus::OutputTruncated: return "metadata block scan scratch buffer was too small";
    case openmeta::ScanStatus::Unsupported: return "metadata container format is unsupported";
    case openmeta::ScanStatus::Malformed: return "metadata container is malformed";
    }

    return "metadata container scan failed";
}

static std::string
exif_status_error_string(const openmeta::ExifDecodeStatus status)
{
    switch (status) {
    case openmeta::ExifDecodeStatus::Ok: return {};
    case openmeta::ExifDecodeStatus::OutputTruncated: return "metadata IFD scratch buffer was too small";
    case openmeta::ExifDecodeStatus::Unsupported: return {};
    case openmeta::ExifDecodeStatus::Malformed: return "EXIF metadata is malformed";
    case openmeta::ExifDecodeStatus::LimitExceeded: return "EXIF metadata exceeded configured limits";
    }

    return "EXIF metadata decode failed";
}

static void
append_u16_bytes(std::vector<std::byte>& out, const uint16_t value)
{
    out.resize(out.size() + sizeof(uint16_t));
    std::memcpy(out.data() + out.size() - sizeof(uint16_t), &value, sizeof(uint16_t));
}

static void
append_u32_bytes(std::vector<std::byte>& out, const uint32_t value)
{
    out.resize(out.size() + sizeof(uint32_t));
    std::memcpy(out.data() + out.size() - sizeof(uint32_t), &value, sizeof(uint32_t));
}

static void
append_i32_bytes(std::vector<std::byte>& out, const int32_t value)
{
    out.resize(out.size() + sizeof(int32_t));
    std::memcpy(out.data() + out.size() - sizeof(int32_t), &value, sizeof(int32_t));
}

static MetadataValue
to_rawgl_metadata_value(const openmeta::ByteArena& arena, const openmeta::MetaValue& value)
{
    MetadataValue result;
    result.kind = to_rawgl_value_kind(value.kind);
    result.elementType = to_rawgl_element_type(value.elem_type);
    result.textEncoding = to_rawgl_text_encoding(value.text_encoding);
    result.count = value.count;

    switch (value.kind) {
    case openmeta::MetaValueKind::Empty: return result;
    case openmeta::MetaValueKind::Scalar:
        switch (value.elem_type) {
        case openmeta::MetaElementType::U8: result.bytes.push_back(static_cast<std::byte>(value.data.u64 & 0xffU)); break;
        case openmeta::MetaElementType::I8: result.bytes.push_back(static_cast<std::byte>(value.data.i64 & 0xff)); break;
        case openmeta::MetaElementType::U16: append_u16_bytes(result.bytes, static_cast<uint16_t>(value.data.u64 & 0xffffU)); break;
        case openmeta::MetaElementType::I16: append_u16_bytes(result.bytes, static_cast<uint16_t>(value.data.i64 & 0xffff)); break;
        case openmeta::MetaElementType::U32: append_u32_bytes(result.bytes, static_cast<uint32_t>(value.data.u64 & 0xffffffffULL)); break;
        case openmeta::MetaElementType::I32: append_i32_bytes(result.bytes, static_cast<int32_t>(value.data.i64 & 0xffffffffULL)); break;
        case openmeta::MetaElementType::U64:
            result.bytes.resize(sizeof(uint64_t));
            std::memcpy(result.bytes.data(), &value.data.u64, sizeof(uint64_t));
            break;
        case openmeta::MetaElementType::I64:
            result.bytes.resize(sizeof(int64_t));
            std::memcpy(result.bytes.data(), &value.data.i64, sizeof(int64_t));
            break;
        case openmeta::MetaElementType::F32:
            result.bytes.resize(sizeof(uint32_t));
            std::memcpy(result.bytes.data(), &value.data.f32_bits, sizeof(uint32_t));
            break;
        case openmeta::MetaElementType::F64:
            result.bytes.resize(sizeof(uint64_t));
            std::memcpy(result.bytes.data(), &value.data.f64_bits, sizeof(uint64_t));
            break;
        case openmeta::MetaElementType::URational:
            append_u32_bytes(result.bytes, value.data.ur.numer);
            append_u32_bytes(result.bytes, value.data.ur.denom);
            break;
        case openmeta::MetaElementType::SRational:
            append_i32_bytes(result.bytes, value.data.sr.numer);
            append_i32_bytes(result.bytes, value.data.sr.denom);
            break;
        }
        return result;
    case openmeta::MetaValueKind::Array:
    case openmeta::MetaValueKind::Bytes:
    case openmeta::MetaValueKind::Text: {
        const std::span<const std::byte> raw = arena.span(value.data.span);
        result.bytes.assign(raw.begin(), raw.end());
        return result;
    }
    }

    return result;
}

static bool
is_preview_printable_ascii(std::span<const std::byte> bytes) noexcept
{
    for (size_t index = 0; index < bytes.size(); ++index) {
        const unsigned char value = static_cast<unsigned char>(bytes[index]);
        if (value == 0U) {
            continue;
        }

        if (value < 32U && value != '\t' && value != '\n' && value != '\r') {
            return false;
        }

        if (value > 126U) {
            return false;
        }
    }

    return true;
}

static void
trim_trailing_nuls(std::string& text)
{
    while (!text.empty() && text.back() == '\0') {
        text.pop_back();
    }
}

static std::string
truncate_preview_text(const std::string_view text, const size_t maxBytes)
{
    if (text.size() <= maxBytes) {
        return std::string(text);
    }

    std::string result(text.substr(0, maxBytes));
    result += "...";
    return result;
}

static std::string
decode_utf16_preview(std::span<const std::byte> bytes,
                     const bool littleEndian,
                     const size_t maxCodeUnits)
{
    const size_t codeUnitCount = bytes.size() / 2U;
    const size_t previewCount = codeUnitCount < maxCodeUnits ? codeUnitCount : maxCodeUnits;

    std::string result;
    result.reserve(previewCount + 3U);

    for (size_t index = 0; index < previewCount; ++index) {
        const uint8_t byte0 = std::to_integer<uint8_t>(bytes[index * 2U + 0U]);
        const uint8_t byte1 = std::to_integer<uint8_t>(bytes[index * 2U + 1U]);
        const uint16_t codeUnit = littleEndian ? static_cast<uint16_t>(byte0 | (byte1 << 8U))
                                               : static_cast<uint16_t>((byte0 << 8U) | byte1);
        if (codeUnit == 0U) {
            continue;
        }

        if (codeUnit >= 32U && codeUnit <= 126U) {
            result.push_back(static_cast<char>(codeUnit));
        } else {
            result.push_back('?');
        }
    }

    if (codeUnitCount > previewCount) {
        result += "...";
    }

    return result;
}

static std::string
format_bytes_preview(const std::span<const std::byte> bytes, const size_t maxBytes)
{
    const size_t previewCount = bytes.size() < maxBytes ? bytes.size() : maxBytes;

    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (size_t index = 0; index < previewCount; ++index) {
        if (index != 0U) {
            stream << ' ';
        }
        stream << std::setw(2) << static_cast<unsigned>(std::to_integer<uint8_t>(bytes[index]));
    }

    if (bytes.size() > previewCount) {
        stream << " ...";
    }

    return stream.str();
}

static size_t
element_size_bytes(const openmeta::MetaElementType type) noexcept
{
    switch (type) {
    case openmeta::MetaElementType::U8:
    case openmeta::MetaElementType::I8: return 1U;
    case openmeta::MetaElementType::U16:
    case openmeta::MetaElementType::I16: return 2U;
    case openmeta::MetaElementType::U32:
    case openmeta::MetaElementType::I32:
    case openmeta::MetaElementType::F32: return 4U;
    case openmeta::MetaElementType::U64:
    case openmeta::MetaElementType::I64:
    case openmeta::MetaElementType::F64: return 8U;
    case openmeta::MetaElementType::URational:
    case openmeta::MetaElementType::SRational: return 8U;
    }

    return 0U;
}

static void
append_scalar_text(std::ostringstream& stream, const openmeta::MetaValue& value)
{
    switch (value.elem_type) {
    case openmeta::MetaElementType::U8: stream << static_cast<unsigned>(value.data.u64 & 0xffU); return;
    case openmeta::MetaElementType::I8: stream << static_cast<int>(static_cast<int8_t>(value.data.i64 & 0xff)); return;
    case openmeta::MetaElementType::U16: stream << static_cast<unsigned>(value.data.u64 & 0xffffU); return;
    case openmeta::MetaElementType::I16: stream << static_cast<int16_t>(value.data.i64 & 0xffff); return;
    case openmeta::MetaElementType::U32: stream << static_cast<uint32_t>(value.data.u64 & 0xffffffffULL); return;
    case openmeta::MetaElementType::I32: stream << static_cast<int32_t>(value.data.i64 & 0xffffffffULL); return;
    case openmeta::MetaElementType::U64: stream << value.data.u64; return;
    case openmeta::MetaElementType::I64: stream << value.data.i64; return;
    case openmeta::MetaElementType::F32: {
        const float floatValue = std::bit_cast<float>(value.data.f32_bits);
        stream << std::setprecision(7) << floatValue;
        return;
    }
    case openmeta::MetaElementType::F64: {
        const double doubleValue = std::bit_cast<double>(value.data.f64_bits);
        stream << std::setprecision(15) << doubleValue;
        return;
    }
    case openmeta::MetaElementType::URational:
        stream << value.data.ur.numer << '/' << value.data.ur.denom;
        return;
    case openmeta::MetaElementType::SRational:
        stream << value.data.sr.numer << '/' << value.data.sr.denom;
        return;
    }
}

static void
append_array_element_text(std::ostringstream& stream,
                          const openmeta::MetaElementType type,
                          const std::span<const std::byte> bytes,
                          const size_t elementIndex)
{
    const size_t elementSize = element_size_bytes(type);
    const size_t byteOffset = elementIndex * elementSize;

    switch (type) {
    case openmeta::MetaElementType::U8:
        stream << static_cast<unsigned>(std::to_integer<uint8_t>(bytes[byteOffset]));
        return;
    case openmeta::MetaElementType::I8:
        stream << static_cast<int>(static_cast<int8_t>(std::to_integer<uint8_t>(bytes[byteOffset])));
        return;
    case openmeta::MetaElementType::U16: {
        uint16_t value = 0;
        std::memcpy(&value, bytes.data() + byteOffset, sizeof(value));
        stream << value;
        return;
    }
    case openmeta::MetaElementType::I16: {
        int16_t value = 0;
        std::memcpy(&value, bytes.data() + byteOffset, sizeof(value));
        stream << value;
        return;
    }
    case openmeta::MetaElementType::U32: {
        uint32_t value = 0;
        std::memcpy(&value, bytes.data() + byteOffset, sizeof(value));
        stream << value;
        return;
    }
    case openmeta::MetaElementType::I32: {
        int32_t value = 0;
        std::memcpy(&value, bytes.data() + byteOffset, sizeof(value));
        stream << value;
        return;
    }
    case openmeta::MetaElementType::U64: {
        uint64_t value = 0;
        std::memcpy(&value, bytes.data() + byteOffset, sizeof(value));
        stream << value;
        return;
    }
    case openmeta::MetaElementType::I64: {
        int64_t value = 0;
        std::memcpy(&value, bytes.data() + byteOffset, sizeof(value));
        stream << value;
        return;
    }
    case openmeta::MetaElementType::F32: {
        uint32_t bits = 0;
        std::memcpy(&bits, bytes.data() + byteOffset, sizeof(bits));
        stream << std::setprecision(7) << std::bit_cast<float>(bits);
        return;
    }
    case openmeta::MetaElementType::F64: {
        uint64_t bits = 0;
        std::memcpy(&bits, bytes.data() + byteOffset, sizeof(bits));
        stream << std::setprecision(15) << std::bit_cast<double>(bits);
        return;
    }
    case openmeta::MetaElementType::URational: {
        openmeta::URational value;
        std::memcpy(&value, bytes.data() + byteOffset, sizeof(value));
        stream << value.numer << '/' << value.denom;
        return;
    }
    case openmeta::MetaElementType::SRational: {
        openmeta::SRational value;
        std::memcpy(&value, bytes.data() + byteOffset, sizeof(value));
        stream << value.numer << '/' << value.denom;
        return;
    }
    }
}

static std::string
format_value_preview(const openmeta::ByteArena& arena,
                     const openmeta::MetaValue& value,
                     const size_t maxPreviewBytes,
                     const size_t maxPreviewElements)
{
    if (value.kind == openmeta::MetaValueKind::Empty) {
        return {};
    }

    if (value.kind == openmeta::MetaValueKind::Scalar) {
        std::ostringstream stream;
        append_scalar_text(stream, value);
        return stream.str();
    }

    if (value.kind == openmeta::MetaValueKind::Text) {
        const std::span<const std::byte> raw = arena.span(value.data.span);
        if (raw.empty()) {
            return {};
        }

        if (value.text_encoding == openmeta::TextEncoding::Utf16LE) {
            return decode_utf16_preview(raw, true, maxPreviewBytes / 2U);
        }
        if (value.text_encoding == openmeta::TextEncoding::Utf16BE) {
            return decode_utf16_preview(raw, false, maxPreviewBytes / 2U);
        }

        if (!is_preview_printable_ascii(raw)) {
            return format_bytes_preview(raw, maxPreviewBytes);
        }

        std::string text(reinterpret_cast<const char*>(raw.data()), raw.size());
        trim_trailing_nuls(text);
        return truncate_preview_text(text, maxPreviewBytes);
    }

    if (value.kind == openmeta::MetaValueKind::Bytes) {
        const std::span<const std::byte> raw = arena.span(value.data.span);
        return format_bytes_preview(raw, maxPreviewBytes);
    }

    if (value.kind == openmeta::MetaValueKind::Array) {
        const std::span<const std::byte> raw = arena.span(value.data.span);
        const size_t previewCount = value.count < maxPreviewElements ? value.count : maxPreviewElements;
        std::ostringstream stream;
        for (size_t index = 0; index < previewCount; ++index) {
            if (index != 0U) {
                stream << ", ";
            }
            append_array_element_text(stream, value.elem_type, raw, index);
        }

        if (value.count > previewCount) {
            stream << ", ...";
        }

        return stream.str();
    }

    return {};
}

class MetadataExportSink final : public openmeta::MetadataSink {
public:
    MetadataExportSink(const openmeta::MetaStore& store,
                       std::vector<MetadataEntry>* entries,
                       const size_t maxPreviewBytes,
                       const size_t maxPreviewElements) noexcept
        : store_(store)
        , entries_(entries)
        , maxPreviewBytes_(maxPreviewBytes)
        , maxPreviewElements_(maxPreviewElements)
    {
    }

    void
    on_item(const openmeta::ExportItem& item) noexcept override
    {
        if (!item.entry || !entries_) {
            return;
        }

        try {
            MetadataEntry entry;
            entry.keyKind = to_rawgl_key_kind(item.entry->key.kind);
            entry.valueKind = to_rawgl_value_kind(item.entry->value.kind);
            entry.elementType = to_rawgl_element_type(item.entry->value.elem_type);
            entry.textEncoding = to_rawgl_text_encoding(item.entry->value.text_encoding);
            entry.flags = to_rawgl_entry_flags(item.entry->flags);
            entry.count = item.entry->value.count;
            entry.name = std::string(item.name);
            entry.valueText = format_value_preview(store_.arena(),
                                                  item.entry->value,
                                                  maxPreviewBytes_,
                                                  maxPreviewElements_);
            entries_->push_back(std::move(entry));
        } catch (...) {
            failed_ = true;
        }
    }

    bool
    failed() const noexcept
    {
        return failed_;
    }

private:
    const openmeta::MetaStore& store_;
    std::vector<MetadataEntry>* entries_ = nullptr;
    size_t maxPreviewBytes_ = 0;
    size_t maxPreviewElements_ = 0;
    bool failed_ = false;
};

class MetadataDocumentExportSink final : public openmeta::MetadataSink {
public:
    MetadataDocumentExportSink(const openmeta::MetaStore& store,
                               std::vector<MetadataField>* fields) noexcept
        : store_(store)
        , fields_(fields)
    {
    }

    void
    on_item(const openmeta::ExportItem& item) noexcept override
    {
        if (!item.entry || !fields_) {
            return;
        }

        try {
            MetadataField field;
            field.keyKind = to_rawgl_key_kind(item.entry->key.kind);
            field.flags = to_rawgl_entry_flags(item.entry->flags);
            field.name = std::string(item.name);
            field.value = to_rawgl_metadata_value(store_.arena(), item.entry->value);
            fields_->push_back(std::move(field));
        } catch (...) {
            failed_ = true;
        }
    }

    bool
    failed() const noexcept
    {
        return failed_;
    }

private:
    const openmeta::MetaStore& store_;
    std::vector<MetadataField>* fields_ = nullptr;
    bool failed_ = false;
};

}  // namespace

static bool
load_store_from_file(const std::string& path,
                     const bool includeMakernotes,
                     openmeta::MetaStore* outStore,
                     std::string* errorMessage)
{
    openmeta::MappedFile mappedFile;
    const openmeta::MappedFileStatus openStatus = mappedFile.open(path.c_str());
    if (openStatus != openmeta::MappedFileStatus::Ok) {
        *errorMessage = mapped_file_error_string(openStatus);
        return false;
    }

    const std::span<const std::byte> fileBytes = mappedFile.bytes();
    const openmeta::ScanResult scanMeasure = openmeta::measure_scan_auto(fileBytes);
    if (scanMeasure.status == openmeta::ScanStatus::Unsupported) {
        *errorMessage = scan_status_error_string(scanMeasure.status);
        return false;
    }
    if (scanMeasure.status == openmeta::ScanStatus::Malformed) {
        *errorMessage = scan_status_error_string(scanMeasure.status);
        return false;
    }

    openmeta::SimpleMetaDecodeOptions decodeOptions;
    decodeOptions.exif.decode_makernote = includeMakernotes;

    size_t blockCapacity = scanMeasure.needed != 0U ? scanMeasure.needed : 16U;
    size_t ifdCapacity = 64U;
    size_t payloadCapacity = fileBytes.size() < (1U << 20U) ? fileBytes.size() : (1U << 20U);
    if (payloadCapacity == 0U) {
        payloadCapacity = 1U;
    }
    size_t scratchIndexCapacity = blockCapacity;

    for (int attempt = 0; attempt < 4; ++attempt) {
        std::vector<openmeta::ContainerBlockRef> blocks(blockCapacity);
        std::vector<openmeta::ExifIfdRef> ifds(ifdCapacity);
        std::vector<std::byte> payload(payloadCapacity);
        std::vector<uint32_t> payloadScratchIndices(scratchIndexCapacity);

        openmeta::MetaStore store;
        const openmeta::SimpleMetaResult readResult =
            openmeta::simple_meta_read(fileBytes,
                                       store,
                                       std::span<openmeta::ContainerBlockRef>(blocks.data(), blocks.size()),
                                       std::span<openmeta::ExifIfdRef>(ifds.data(), ifds.size()),
                                       std::span<std::byte>(payload.data(), payload.size()),
                                       std::span<uint32_t>(payloadScratchIndices.data(), payloadScratchIndices.size()),
                                       decodeOptions);

        bool retry = false;
        if (readResult.scan.status == openmeta::ScanStatus::OutputTruncated
            && static_cast<size_t>(readResult.scan.needed) > blockCapacity) {
            blockCapacity = readResult.scan.needed;
            if (scratchIndexCapacity < blockCapacity) {
                scratchIndexCapacity = blockCapacity;
            }
            retry = true;
        }
        if (readResult.exif.status == openmeta::ExifDecodeStatus::OutputTruncated
            && static_cast<size_t>(readResult.exif.ifds_needed) > ifdCapacity) {
            ifdCapacity = readResult.exif.ifds_needed;
            retry = true;
        }
        if (readResult.payload.status == openmeta::PayloadStatus::OutputTruncated
            && static_cast<size_t>(readResult.payload.needed) > payloadCapacity) {
            payloadCapacity = static_cast<size_t>(readResult.payload.needed);
            retry = true;
        }

        if (retry) {
            continue;
        }

        if (readResult.scan.status != openmeta::ScanStatus::Ok) {
            *errorMessage = scan_status_error_string(readResult.scan.status);
            return false;
        }

        if (readResult.exif.status != openmeta::ExifDecodeStatus::Ok
            && readResult.exif.status != openmeta::ExifDecodeStatus::Unsupported) {
            *errorMessage = exif_status_error_string(readResult.exif.status);
            return false;
        }

        store.finalize();
        *outStore = std::move(store);
        return true;
    }

    *errorMessage = "metadata read scratch sizing failed";
    return false;
}

MetadataReadResult
read_metadata_file_impl(const MetadataReadRequest& request)
{
    MetadataReadResult result;

    openmeta::MetaStore store;
    if (!load_store_from_file(request.path, request.includeMakernotes, &store, &result.errorMessage)) {
        return result;
    }

    result.entries.reserve(store.entries().size());

    openmeta::ExportOptions exportOptions;
    exportOptions.style = to_openmeta_name_style(request.nameStyle);
    exportOptions.name_policy = to_openmeta_name_policy(request.namePolicy);
    exportOptions.include_makernotes = request.includeMakernotes;

    MetadataExportSink sink(store,
                            &result.entries,
                            request.maxValuePreviewBytes,
                            request.maxValuePreviewElements);
    openmeta::visit_metadata(store, exportOptions, sink);
    if (sink.failed()) {
        result.errorMessage = "metadata export failed";
        result.entries.clear();
        return result;
    }

    result.success = true;
    return result;
}

MetadataDocumentReadResult
read_metadata_document_file_impl(const MetadataDocumentReadRequest& request)
{
    MetadataDocumentReadResult result;

    openmeta::MetaStore store;
    if (!load_store_from_file(request.path, request.includeMakernotes, &store, &result.errorMessage)) {
        return result;
    }

    result.document.fields.reserve(store.entries().size());

    openmeta::ExportOptions exportOptions;
    exportOptions.style = to_openmeta_name_style(request.nameStyle);
    exportOptions.name_policy = to_openmeta_name_policy(request.namePolicy);
    exportOptions.include_makernotes = request.includeMakernotes;

    MetadataDocumentExportSink sink(store, &result.document.fields);
    openmeta::visit_metadata(store, exportOptions, sink);
    if (sink.failed()) {
        result.errorMessage = "metadata document export failed";
        result.document.fields.clear();
        return result;
    }

    auto storage = std::make_shared<MetadataDocumentStorage>();
    storage->store = std::move(store);
    result.document.storage = std::move(storage);
    result.success = true;
    return result;
}

static std::span<const std::byte>
metadata_value_bytes(const MetadataValue& value) noexcept
{
    return std::span<const std::byte>(value.bytes.data(), value.bytes.size());
}

static size_t
metadata_element_size_bytes(const MetadataElementType type) noexcept
{
    switch (type) {
    case MetadataElementType::U8:
    case MetadataElementType::I8: return 1U;
    case MetadataElementType::U16:
    case MetadataElementType::I16: return 2U;
    case MetadataElementType::U32:
    case MetadataElementType::I32:
    case MetadataElementType::F32: return 4U;
    case MetadataElementType::U64:
    case MetadataElementType::I64:
    case MetadataElementType::F64: return 8U;
    case MetadataElementType::URational:
    case MetadataElementType::SRational: return 8U;
    }

    return 0U;
}

template<typename TValue>
static TValue
read_pod_value(const std::span<const std::byte> bytes, const size_t offset)
{
    TValue value {};
    std::memcpy(&value, bytes.data() + offset, sizeof(TValue));
    return value;
}

static void
append_metadata_scalar_text(std::ostringstream& stream,
                            const MetadataElementType elementType,
                            const std::span<const std::byte> bytes,
                            const size_t offset)
{
    switch (elementType) {
    case MetadataElementType::U8: stream << static_cast<unsigned>(read_pod_value<uint8_t>(bytes, offset)); return;
    case MetadataElementType::I8: stream << static_cast<int>(read_pod_value<int8_t>(bytes, offset)); return;
    case MetadataElementType::U16: stream << read_pod_value<uint16_t>(bytes, offset); return;
    case MetadataElementType::I16: stream << read_pod_value<int16_t>(bytes, offset); return;
    case MetadataElementType::U32: stream << read_pod_value<uint32_t>(bytes, offset); return;
    case MetadataElementType::I32: stream << read_pod_value<int32_t>(bytes, offset); return;
    case MetadataElementType::U64: stream << read_pod_value<uint64_t>(bytes, offset); return;
    case MetadataElementType::I64: stream << read_pod_value<int64_t>(bytes, offset); return;
    case MetadataElementType::F32: stream << std::bit_cast<float>(read_pod_value<uint32_t>(bytes, offset)); return;
    case MetadataElementType::F64: stream << std::bit_cast<double>(read_pod_value<uint64_t>(bytes, offset)); return;
    case MetadataElementType::URational: {
        const uint32_t numer = read_pod_value<uint32_t>(bytes, offset + 0U);
        const uint32_t denom = read_pod_value<uint32_t>(bytes, offset + 4U);
        stream << numer << '/' << denom;
        return;
    }
    case MetadataElementType::SRational: {
        const int32_t numer = read_pod_value<int32_t>(bytes, offset + 0U);
        const int32_t denom = read_pod_value<int32_t>(bytes, offset + 4U);
        stream << numer << '/' << denom;
        return;
    }
    }
}

static std::string
format_metadata_value_text(const MetadataValue& value)
{
    const std::span<const std::byte> bytes = metadata_value_bytes(value);

    switch (value.kind) {
    case MetadataValueKind::Empty: return {};
    case MetadataValueKind::Text: {
        std::string text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        trim_trailing_nuls(text);
        return text;
    }
    case MetadataValueKind::Bytes: return format_bytes_preview(bytes, bytes.size());
    case MetadataValueKind::Scalar: {
        std::ostringstream stream;
        append_metadata_scalar_text(stream, value.elementType, bytes, 0U);
        return stream.str();
    }
    case MetadataValueKind::Array: {
        const size_t elementSize = metadata_element_size_bytes(value.elementType);
        if (elementSize == 0U || bytes.size() < elementSize) {
            return {};
        }

        const size_t count = value.count;
        std::ostringstream stream;
        for (size_t index = 0; index < count; ++index) {
            if (index != 0U) {
                stream << ", ";
            }
            append_metadata_scalar_text(stream, value.elementType, bytes, index * elementSize);
        }
        return stream.str();
    }
    }

    return {};
}

bool
flatten_metadata_document_to_oiio_attributes(const MetadataDocument& document,
                                             std::map<std::string, std::string>* out,
                                             std::string* errorMessage)
{
    if (!out || !errorMessage) {
        return false;
    }

    out->clear();
    errorMessage->clear();

    if (document.storage) {
        std::vector<openmeta::OiioAttribute> attributes;
        openmeta::OiioAdapterRequest request;
        request.name_policy = openmeta::ExportNamePolicy::ExifToolAlias;
        request.include_makernotes = true;
        openmeta::InteropSafetyError safetyError;
        const openmeta::InteropSafetyStatus status =
            openmeta::collect_oiio_attributes_safe(document.storage->store, &attributes, request, &safetyError);
        if (status != openmeta::InteropSafetyStatus::Ok) {
            *errorMessage = safetyError.message.empty() ? "failed to flatten metadata document for OIIO"
                                                        : safetyError.message;
            return false;
        }

        for (const openmeta::OiioAttribute& attribute : attributes) {
            (*out)[attribute.name] = attribute.value;
        }
        return true;
    }

    for (const MetadataField& field : document.fields) {
        if (field.name.empty()) {
            continue;
        }
        (*out)[field.name] = format_metadata_value_text(field.value);
    }

    return true;
}
#endif

MetadataReadResult
IoRuntime::readMetadataFile(const MetadataReadRequest& request) const
{
#if defined(RAWGL_HAS_OPENMETA)
    return read_metadata_file_impl(request);
#else
    MetadataReadResult result;
    (void)request;
    result.errorMessage = "RawGL metadata support was built without OpenMeta";
    return result;
#endif
}

MetadataReadResult
ReadMetadataFile(const MetadataReadRequest& request)
{
    return IoRuntime().readMetadataFile(request);
}

MetadataDocumentReadResult
IoRuntime::readMetadataDocumentFile(const MetadataDocumentReadRequest& request) const
{
#if defined(RAWGL_HAS_OPENMETA)
    return read_metadata_document_file_impl(request);
#else
    MetadataDocumentReadResult result;
    (void)request;
    result.errorMessage = "RawGL metadata support was built without OpenMeta";
    return result;
#endif
}

MetadataDocumentReadResult
ReadMetadataDocumentFile(const MetadataDocumentReadRequest& request)
{
    return IoRuntime().readMetadataDocumentFile(request);
}

}  // namespace rawgl::io
