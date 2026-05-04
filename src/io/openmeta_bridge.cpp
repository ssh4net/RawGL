// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "metadata_internal.h"

#include "exr_backend.h"
#include "gl_utils.h"
#include "image_backend.h"
#include "texture_loader.h"

#include <bit>
#include <cstring>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <span>
#include <sstream>
#include <string_view>
#include <vector>

#if defined(RAWGL_HAS_OPENMETA)
#include <openmeta/container_scan.h>
#include <openmeta/exr_adapter.h>
#include <openmeta/interop_export.h>
#include <openmeta/mapped_file.h>
#include <openmeta/meta_flags.h>
#include <openmeta/meta_key.h>
#include <openmeta/meta_store.h>
#include <openmeta/metadata_transfer.h>
#include <openmeta/meta_value.h>
#include <openmeta/simple_meta.h>
#endif

namespace rawgl::io {

#if defined(RAWGL_HAS_OPENMETA)
namespace {

struct OpenMetaBackendStorage final {
    openmeta::TransferSourceSnapshot sourceSnapshot;
};

static const OpenMetaBackendStorage*
get_openmeta_storage(const MetadataDocument& document) noexcept
{
    if (!document.storage || !document.storage->backendStorage) {
        return nullptr;
    }

    return static_cast<const OpenMetaBackendStorage*>(document.storage->backendStorage.get());
}

static openmeta::ExportNameStyle
to_openmeta_name_style(const MetadataNameStyle style) noexcept
{
    switch (style) {
    case MetadataNameStyle::Canonical: return openmeta::ExportNameStyle::Canonical;
    case MetadataNameStyle::XmpPortable: return openmeta::ExportNameStyle::XmpPortable;
    case MetadataNameStyle::Oiio: return openmeta::ExportNameStyle::FlatHost;
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

static openmeta::TransferSafetyMode
to_openmeta_transfer_safety(const MetadataTransferSafety safety) noexcept
{
    switch (safety) {
    case MetadataTransferSafety::CompatibleFile: return openmeta::TransferSafetyMode::CompatibleFile;
    case MetadataTransferSafety::RenderedImage: return openmeta::TransferSafetyMode::RenderedImage;
    }

    return openmeta::TransferSafetyMode::RenderedImage;
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

        const bool allowUnsupportedScan = readResult.scan.status == openmeta::ScanStatus::Unsupported
                                          && readResult.exr.status == openmeta::ExrDecodeStatus::Ok;
        if (readResult.scan.status != openmeta::ScanStatus::Ok && !allowUnsupportedScan) {
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

static bool
write_file_bytes(const std::string& path,
                 std::span<const std::byte> bytes,
                 const char* formatName,
                 std::string* errorMessage)
{
    const std::filesystem::path outputPath(path);
    const std::filesystem::path tempPath = outputPath.string() + ".rawgl_meta_tmp";

    {
        std::ofstream output(tempPath, std::ios::binary | std::ios::trunc);
        if (!output) {
            *errorMessage = std::string("failed to open temporary ") + formatName + " metadata patch file";
            return false;
        }

        if (!bytes.empty()) {
            output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            if (!output) {
                *errorMessage = std::string("failed to write temporary ") + formatName + " metadata patch file";
                return false;
            }
        }
    }

    std::error_code filesystemError;
    std::filesystem::copy_file(tempPath, outputPath, std::filesystem::copy_options::overwrite_existing, filesystemError);
    if (filesystemError) {
        std::filesystem::remove(tempPath, filesystemError);
        *errorMessage = std::string("failed to replace ") + formatName + " file with metadata-patched output";
        return false;
    }

    std::filesystem::remove(tempPath, filesystemError);

    return true;
}

static bool
set_transfer_target_image_spec_from_host_image(const HostImageData& image,
                                               openmeta::TransferTargetImageSpec* spec,
                                               std::string* errorMessage)
{
    if (!spec) {
        if (errorMessage) {
            *errorMessage = "OpenMeta target image spec output is null";
        }
        return false;
    }

    if (image.width <= 0 || image.height <= 0) {
        if (errorMessage) {
            *errorMessage = "target image dimensions are invalid for metadata transfer";
        }
        return false;
    }

    if (image.channels <= 0
        || image.channels > static_cast<int>(openmeta::kTransferTargetImageSpecMaxSamples)) {
        if (errorMessage) {
            *errorMessage = "target image channel count is invalid for metadata transfer";
        }
        return false;
    }

    uint16_t bitsPerSample = 0U;
    uint16_t sampleFormat = 1U;
    switch (image.glType) {
    case GL_UNSIGNED_BYTE:
        bitsPerSample = 8U;
        sampleFormat = 1U;
        break;
    case GL_UNSIGNED_SHORT:
        bitsPerSample = 16U;
        sampleFormat = 1U;
        break;
    case GL_UNSIGNED_INT:
        bitsPerSample = 32U;
        sampleFormat = 1U;
        break;
    case GL_HALF_FLOAT:
        bitsPerSample = 16U;
        sampleFormat = 3U;
        break;
    case GL_FLOAT:
        bitsPerSample = 32U;
        sampleFormat = 3U;
        break;
    default:
        if (errorMessage) {
            *errorMessage = "target image component type is unsupported for metadata transfer";
        }
        return false;
    }

    spec->has_dimensions = true;
    spec->width = static_cast<uint32_t>(image.width);
    spec->height = static_cast<uint32_t>(image.height);
    spec->has_orientation = true;
    spec->orientation = 1U;
    spec->has_samples_per_pixel = true;
    spec->samples_per_pixel = static_cast<uint16_t>(image.channels);
    spec->bits_per_sample_count = 1U;
    spec->bits_per_sample[0] = bitsPerSample;
    spec->sample_format_count = 1U;
    spec->sample_format[0] = sampleFormat;
    spec->has_photometric_interpretation = true;
    spec->photometric_interpretation = image.channels >= 3 ? 2U : 1U;
    spec->has_planar_configuration = true;
    spec->planar_configuration = 1U;

    return true;
}

static bool
set_transfer_target_image_spec_from_file(const std::string& path,
                                         const char* formatName,
                                         openmeta::TransferTargetImageSpec* spec,
                                         std::string* errorMessage)
{
    try {
        const HostImageData image = load_host_image_data(path, std::map<std::string, std::string>());
        return set_transfer_target_image_spec_from_host_image(image, spec, errorMessage);
    } catch (const std::exception& exception) {
        if (errorMessage) {
            *errorMessage = std::string("failed to inspect ") + formatName
                            + " target image for metadata transfer: " + exception.what();
        }
        return false;
    }
}

static ImageMetadataApplyResult
apply_source_metadata_to_file_with_openmeta_impl(const MetadataDocument& document,
                                                 const std::string& path,
                                                 const openmeta::TransferTargetFormat targetFormat,
                                                 const char* formatName,
                                                 const HostImageData* targetImage,
                                                 const MetadataTransferSafety safety)
{
    ImageMetadataApplyResult result;

    const OpenMetaBackendStorage* storage = get_openmeta_storage(document);
    if (!storage) {
        result.errorMessage = std::string("metadata document does not have source storage for ")
                              + formatName + " transfer";
        return result;
    }

    openmeta::ExecutePreparedTransferSnapshotOptions options;
    options.prepare.target_format = targetFormat;
    options.prepare.include_xmp_app1 = true;
    options.prepare.include_icc_app2 = true;
    options.prepare.include_iptc_app13 = false;
    options.prepare.xmp_include_existing = true;
    options.prepare.profile.safety = to_openmeta_transfer_safety(safety);
    if (targetImage) {
        if (!set_transfer_target_image_spec_from_host_image(*targetImage,
                                                            &options.prepare.target_image_spec,
                                                            &result.errorMessage)) {
            return result;
        }
    } else {
        if (!set_transfer_target_image_spec_from_file(path,
                                                      formatName,
                                                      &options.prepare.target_image_spec,
                                                      &result.errorMessage)) {
            return result;
        }
    }
    options.edit_target_path = path;
    options.execute.edit_apply = true;

    const openmeta::ExecutePreparedTransferFileResult executed =
        openmeta::execute_prepared_transfer_snapshot(storage->sourceSnapshot, options);
    if (executed.prepared.file_status != openmeta::TransferFileStatus::Ok) {
        result.errorMessage = executed.prepared.prepare.message.empty()
                                  ? std::string("OpenMeta snapshot ") + formatName + " metadata prepare failed"
                                  : executed.prepared.prepare.message;
        return result;
    }
    if (executed.prepared.prepare.status != openmeta::TransferStatus::Ok) {
        result.errorMessage = executed.prepared.prepare.message.empty()
                                  ? std::string("OpenMeta snapshot ") + formatName + " metadata bundle prepare failed"
                                  : executed.prepared.prepare.message;
        return result;
    }
    if (executed.execute.edit_apply.status != openmeta::TransferStatus::Ok) {
        result.errorMessage = executed.execute.edit_apply.message.empty()
                                  ? std::string("OpenMeta ") + formatName + " metadata edit apply failed"
                                  : executed.execute.edit_apply.message;
        return result;
    }
    if (executed.execute.edited_output.empty()) {
        result.success = true;
        return result;
    }
    if (!write_file_bytes(path,
                          std::span<const std::byte>(executed.execute.edited_output.data(),
                                                     executed.execute.edited_output.size()),
                          formatName,
                          &result.errorMessage)) {
        return result;
    }

    result.success = true;
    return result;
}

static ImageMetadataApplyResult
apply_source_metadata_to_exr_file_with_openmeta_impl(const MetadataDocument& document,
                                                     const std::string& path,
                                                     const HostImageData* targetImage,
                                                     const MetadataTransferSafety safety)
{
    ImageMetadataApplyResult result;

    const OpenMetaBackendStorage* storage = get_openmeta_storage(document);
    if (!storage) {
        result.errorMessage = "metadata document does not have source storage for EXR transfer";
        return result;
    }

    HostImageData loadedImage;
    const HostImageData* image = targetImage;
    if (!image) {
        try {
            loadedImage = load_host_image_data(path, std::map<std::string, std::string>());
            image = &loadedImage;
        } catch (const std::exception& exception) {
            result.errorMessage = std::string("failed to inspect EXR target image for metadata transfer: ")
                                  + exception.what();
            return result;
        }
    }

    int bits = 0;
    if (image->glType == GL_HALF_FLOAT) {
        bits = 16;
    } else if (image->glType == GL_FLOAT) {
        bits = 32;
    } else {
        result.errorMessage = "EXR metadata apply currently supports only half and float EXR outputs";
        return result;
    }

    openmeta::PrepareTransferRequest request;
    request.target_format = openmeta::TransferTargetFormat::Exr;
    request.profile.safety = to_openmeta_transfer_safety(safety);
    if (!set_transfer_target_image_spec_from_host_image(*image, &request.target_image_spec, &result.errorMessage)) {
        return result;
    }

    openmeta::PreparedTransferBundle bundle;
    const openmeta::PrepareTransferResult prepared =
        openmeta::prepare_metadata_for_target_snapshot(storage->sourceSnapshot, request, &bundle);
    if (prepared.status != openmeta::TransferStatus::Ok) {
        result.errorMessage = prepared.message.empty()
                                  ? "OpenMeta EXR metadata prepare failed"
                                  : prepared.message;
        return result;
    }

    openmeta::ExrAdapterBatch batch;
    const openmeta::ExrAdapterResult adapted =
        openmeta::build_prepared_exr_attribute_batch(bundle, &batch);
    if (adapted.status != openmeta::ExrAdapterStatus::Ok) {
        result.errorMessage = adapted.message.empty()
                                  ? "OpenMeta EXR attribute batch build failed"
                                  : adapted.message;
        return result;
    }

    std::map<std::string, std::string> attributes;
    if (!extract_exr_reencode_attributes(path, attributes, result.errorMessage)) {
        return result;
    }

    for (const openmeta::ExrAdapterAttribute& attribute : batch.attributes) {
        if (attribute.part_index != 0U) {
            result.errorMessage = "EXR metadata apply supports only part 0";
            return result;
        }
        if (attribute.is_opaque || attribute.type_name != "string") {
            result.errorMessage = "EXR metadata apply supports only string header attributes";
            return result;
        }

        std::string value(reinterpret_cast<const char*>(attribute.value.data()), attribute.value.size());
        while (!value.empty() && value.back() == '\0') {
            value.pop_back();
        }

        attributes[std::string("openexr:attribute:string:") + attribute.name] = std::move(value);
    }

    ImageSaveRequest saveRequest;
    saveRequest.path = path;
    saveRequest.bits = bits;
    saveRequest.image = *image;
    for (const auto& attribute : attributes) {
        Attribute one;
        one.name = attribute.first;
        one.value = attribute.second;
        saveRequest.attributes.push_back(std::move(one));
    }

    const ImageSaveResult saved = SaveImageFile(saveRequest);
    if (!saved.success) {
        result.errorMessage = saved.errorMessage.empty()
                                  ? "EXR metadata rewrite failed"
                                  : saved.errorMessage;
        return result;
    }

    result.success = true;
    return result;
}

}  // namespace
#endif

void
MetadataBackendStorageDeleter::operator()(void* storage) const noexcept
{
#if defined(RAWGL_HAS_OPENMETA)
    delete static_cast<OpenMetaBackendStorage*>(storage);
#else
    (void)storage;
#endif
}

MetadataReadResult
read_metadata_file_impl(const MetadataReadRequest& request)
{
#if defined(RAWGL_HAS_OPENMETA)
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
#else
    MetadataReadResult result;
    (void)request;
    result.errorMessage = "RawGL metadata support was built without OpenMeta";
    return result;
#endif
}

MetadataDocumentReadResult
read_metadata_document_file_impl(const MetadataDocumentReadRequest& request)
{
#if defined(RAWGL_HAS_OPENMETA)
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

    OpenMetaBackendStorage* backendStorage = new OpenMetaBackendStorage();
    backendStorage->sourceSnapshot = openmeta::build_transfer_source_snapshot(store);

    std::shared_ptr<MetadataDocumentStorage> storage = std::make_shared<MetadataDocumentStorage>();
    storage->backendStorage.reset(backendStorage);
    result.document.storage = std::move(storage);
    result.success = true;
    return result;
#else
    MetadataDocumentReadResult result;
    (void)request;
    result.errorMessage = "RawGL metadata support was built without OpenMeta";
    return result;
#endif
}

ImageMetadataApplyResult
apply_source_metadata_to_tiff_file_impl(const MetadataDocument& document,
                                        const std::string& path,
                                        const MetadataTransferSafety safety)
{
#if defined(RAWGL_HAS_OPENMETA)
    return apply_source_metadata_to_file_with_openmeta_impl(
        document, path, openmeta::TransferTargetFormat::Tiff, "TIFF", nullptr, safety);
#else
    (void)document;
    (void)path;
    (void)safety;
    ImageMetadataApplyResult result;
    result.errorMessage = "RawGL metadata support was built without OpenMeta";
    return result;
#endif
}

ImageMetadataApplyResult
apply_source_metadata_to_tiff_file_impl(const MetadataDocument& document,
                                        const std::string& path,
                                        const HostImageData& targetImage,
                                        const MetadataTransferSafety safety)
{
#if defined(RAWGL_HAS_OPENMETA)
    return apply_source_metadata_to_file_with_openmeta_impl(
        document, path, openmeta::TransferTargetFormat::Tiff, "TIFF", &targetImage, safety);
#else
    (void)document;
    (void)path;
    (void)targetImage;
    (void)safety;
    ImageMetadataApplyResult result;
    result.errorMessage = "RawGL metadata support was built without OpenMeta";
    return result;
#endif
}

ImageMetadataApplyResult
apply_source_metadata_to_jpeg_file_impl(const MetadataDocument& document,
                                        const std::string& path,
                                        const MetadataTransferSafety safety)
{
#if defined(RAWGL_HAS_OPENMETA)
    return apply_source_metadata_to_file_with_openmeta_impl(
        document, path, openmeta::TransferTargetFormat::Jpeg, "JPEG", nullptr, safety);
#else
    (void)document;
    (void)path;
    (void)safety;
    ImageMetadataApplyResult result;
    result.errorMessage = "RawGL metadata support was built without OpenMeta";
    return result;
#endif
}

ImageMetadataApplyResult
apply_source_metadata_to_jpeg_file_impl(const MetadataDocument& document,
                                        const std::string& path,
                                        const HostImageData& targetImage,
                                        const MetadataTransferSafety safety)
{
#if defined(RAWGL_HAS_OPENMETA)
    return apply_source_metadata_to_file_with_openmeta_impl(
        document, path, openmeta::TransferTargetFormat::Jpeg, "JPEG", &targetImage, safety);
#else
    (void)document;
    (void)path;
    (void)targetImage;
    (void)safety;
    ImageMetadataApplyResult result;
    result.errorMessage = "RawGL metadata support was built without OpenMeta";
    return result;
#endif
}

ImageMetadataApplyResult
apply_source_metadata_to_png_file_impl(const MetadataDocument& document,
                                       const std::string& path,
                                       const MetadataTransferSafety safety)
{
#if defined(RAWGL_HAS_OPENMETA)
    return apply_source_metadata_to_file_with_openmeta_impl(
        document, path, openmeta::TransferTargetFormat::Png, "PNG", nullptr, safety);
#else
    (void)document;
    (void)path;
    (void)safety;
    ImageMetadataApplyResult result;
    result.errorMessage = "RawGL metadata support was built without OpenMeta";
    return result;
#endif
}

ImageMetadataApplyResult
apply_source_metadata_to_png_file_impl(const MetadataDocument& document,
                                       const std::string& path,
                                       const HostImageData& targetImage,
                                       const MetadataTransferSafety safety)
{
#if defined(RAWGL_HAS_OPENMETA)
    return apply_source_metadata_to_file_with_openmeta_impl(
        document, path, openmeta::TransferTargetFormat::Png, "PNG", &targetImage, safety);
#else
    (void)document;
    (void)path;
    (void)targetImage;
    (void)safety;
    ImageMetadataApplyResult result;
    result.errorMessage = "RawGL metadata support was built without OpenMeta";
    return result;
#endif
}

ImageMetadataApplyResult
apply_source_metadata_to_exr_file_impl(const MetadataDocument& document,
                                       const std::string& path,
                                       const MetadataTransferSafety safety)
{
#if defined(RAWGL_HAS_OPENMETA)
    return apply_source_metadata_to_exr_file_with_openmeta_impl(document, path, nullptr, safety);
#else
    (void)document;
    (void)path;
    (void)safety;
    ImageMetadataApplyResult result;
    result.errorMessage = "RawGL metadata support was built without OpenMeta";
    return result;
#endif
}

ImageMetadataApplyResult
apply_source_metadata_to_exr_file_impl(const MetadataDocument& document,
                                       const std::string& path,
                                       const HostImageData& targetImage,
                                       const MetadataTransferSafety safety)
{
#if defined(RAWGL_HAS_OPENMETA)
    return apply_source_metadata_to_exr_file_with_openmeta_impl(document, path, &targetImage, safety);
#else
    (void)document;
    (void)path;
    (void)targetImage;
    (void)safety;
    ImageMetadataApplyResult result;
    result.errorMessage = "RawGL metadata support was built without OpenMeta";
    return result;
#endif
}

ImageMetadataTransferResult
transfer_image_metadata_file_impl(const ImageMetadataTransferRequest& request)
{
    ImageMetadataTransferResult result;

    ImageMetadataApplyResult applied;
    const HostImageData* targetImage = request.hasTargetImage ? &request.targetImage : nullptr;
    switch (get_image_codec_family(request.path)) {
    case ImageCodecFamily::Jpeg:
        applied = targetImage ? apply_source_metadata_to_jpeg_file_impl(request.sourceMetadata,
                                                                        request.path,
                                                                        *targetImage,
                                                                        request.safety)
                              : apply_source_metadata_to_jpeg_file_impl(request.sourceMetadata,
                                                                        request.path,
                                                                        request.safety);
        break;
    case ImageCodecFamily::Png:
        applied = targetImage ? apply_source_metadata_to_png_file_impl(request.sourceMetadata,
                                                                       request.path,
                                                                       *targetImage,
                                                                       request.safety)
                              : apply_source_metadata_to_png_file_impl(request.sourceMetadata,
                                                                       request.path,
                                                                       request.safety);
        break;
    case ImageCodecFamily::Tiff:
        applied = targetImage ? apply_source_metadata_to_tiff_file_impl(request.sourceMetadata,
                                                                        request.path,
                                                                        *targetImage,
                                                                        request.safety)
                              : apply_source_metadata_to_tiff_file_impl(request.sourceMetadata,
                                                                        request.path,
                                                                        request.safety);
        break;
    case ImageCodecFamily::Exr:
        applied = targetImage ? apply_source_metadata_to_exr_file_impl(request.sourceMetadata,
                                                                       request.path,
                                                                       *targetImage,
                                                                       request.safety)
                              : apply_source_metadata_to_exr_file_impl(request.sourceMetadata,
                                                                       request.path,
                                                                       request.safety);
        break;
    default:
        result.errorMessage = "metadata transfer currently supports JPEG, PNG, TIFF, and EXR targets";
        return result;
    }

    result.success = applied.success;
    result.errorMessage = std::move(applied.errorMessage);
    return result;
}

}  // namespace rawgl::io
