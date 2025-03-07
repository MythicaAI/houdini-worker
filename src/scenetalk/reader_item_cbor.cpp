#include <qcbor/qcbor_decode.h>

#include "frame.h"
#include "reader.h"

template<>
bool scene_talk::decode(
    const frame_payload& payload,
    reader::partial_header &out_header) {

    QCBORDecodeContext decodeContext;
    QCBORDecode_Init(
        &decodeContext,
        UsefulBufC{(void *) payload.data(), payload.size()},
        QCBOR_DECODE_MODE_NORMAL);

    // Start decoding the top-level map
    QCBORItem item;
    QCBORDecode_GetNext(&decodeContext, &item);
    if (item.uDataType != QCBOR_TYPE_MAP) {
        // Not a map, can't process
        return false;
    }

    // Variables to store our extracted values
    bool seq_found = false;
    bool id_found = false;

    // Parse the map entries to find "seq" and "id"
    size_t mapEntries = item.val.uCount;
    for (size_t i = 0; i < mapEntries; i++) {
        // Get map key
        QCBORDecode_GetNext(&decodeContext, &item);
        if (item.uDataType != QCBOR_TYPE_TEXT_STRING) {
            // Skip non-string keys and their values
            QCBORDecode_GetNext(&decodeContext, &item); // Skip the value
            continue;
        }

        UsefulBufC key = item.val.string;

        // Get map value
        QCBORDecode_GetNext(&decodeContext, &item);

        // Check if key is "seq"
        if (UsefulBuf_Compare(key, UsefulBuf_FromSZ("seq")) == 0) {
            if (item.uDataType == QCBOR_TYPE_INT64 || item.uDataType == QCBOR_TYPE_UINT64) {
                out_header.seq = (uint32_t) item.val.uint64;
                seq_found = true;
            }
        }
        // Check if key is "id"
        else if (UsefulBuf_Compare(key, UsefulBuf_FromSZ("id")) == 0) {
            if (item.uDataType == QCBOR_TYPE_INT64 || item.uDataType == QCBOR_TYPE_UINT64) {
                out_header.id = (uint32_t) item.val.uint64;
                id_found = true;
            }
        }
    }

    // Check if we found both required fields
    if (!seq_found || !id_found) {
        return false;
    }
    return true;
}

template<>
bool scene_talk::decode(
    const frame_payload &payload,
    reader::begin_context &out) {
    QCBORDecodeContext decodeContext;
    QCBORDecode_Init(
        &decodeContext,
        UsefulBufC{(void *) payload.data(), payload.size()},
        QCBOR_DECODE_MODE_NORMAL);

    QCBORItem topItem;
    QCBORDecode_GetNext(&decodeContext, &topItem);
    if (topItem.uDataType != QCBOR_TYPE_MAP) {
        return false;
    }

    size_t mapEntries = topItem.val.uCount;
    bool type_found = false;
    bool loc_found = false;

    for (size_t i = 0; i < mapEntries; i++) {
        QCBORItem item;
        QCBORDecode_GetNext(&decodeContext, &item);
        if (item.uDataType != QCBOR_TYPE_TEXT_STRING) {
            QCBORDecode_GetNext(&decodeContext, &item);
            continue;
        }

        UsefulBufC key = item.label.string;
        UsefulBufC value = item.val.string;

        if (UsefulBuf_Compare(key, UsefulBuf_FromSZ("type")) == 0) {
            if (item.uDataType == QCBOR_TYPE_TEXT_STRING) {
                out.entity_type.assign((const char *) value.ptr, value.len);
                type_found = true;
            }
        } else if (UsefulBuf_Compare(key, UsefulBuf_FromSZ("loc")) == 0) {
            if (item.uDataType == QCBOR_TYPE_TEXT_STRING) {
                out.location.assign((const char *) value.ptr, value.len);
                loc_found = true;
            }
        }
    }

    return type_found && loc_found;
}

// static
template<>
bool scene_talk::decode(
    const frame_payload &payload,
    reader::end_context &out) {
    QCBORDecodeContext decodeContext;
    QCBORDecode_Init(
        &decodeContext,
        UsefulBufC{(void *) payload.data(), payload.size()},
        QCBOR_DECODE_MODE_NORMAL);

    QCBORItem topItem;
    QCBORDecode_GetNext(&decodeContext, &topItem);
    if (topItem.uDataType != QCBOR_TYPE_MAP) {
        return false;
    }

    // End context may not contain additional data, so if it's a valid map, we accept it
    return true;
}

// static
template<>
bool scene_talk::decode(
    const frame_payload &payload,
    reader::attr_stream &out) {
    QCBORDecodeContext decodeContext;
    QCBORDecode_Init(
        &decodeContext,
        UsefulBufC{(void *) payload.data(), payload.size()},
        QCBOR_DECODE_MODE_NORMAL);

    QCBORItem topItem;
    QCBORDecode_GetNext(&decodeContext, &topItem);
    if (topItem.uDataType != QCBOR_TYPE_MAP) {
        return false;
    }

    size_t mapEntries = topItem.val.uCount;
    bool name_found = false;
    bool type_found = false;
    bool value_found = false;

    for (size_t i = 0; i < mapEntries; i++) {
        QCBORItem item;
        QCBORDecode_GetNext(&decodeContext, &item);
        if (item.uDataType != QCBOR_TYPE_TEXT_STRING) {
            QCBORDecode_GetNext(&decodeContext, &item);
            continue;
        }

        UsefulBufC key = item.val.string;
        QCBORDecode_GetNext(&decodeContext, &item);

        if (UsefulBuf_Compare(key, UsefulBuf_FromSZ("name")) == 0) {
            if (item.uDataType == QCBOR_TYPE_TEXT_STRING) {
                out.name.assign((const char *) item.val.string.ptr, item.val.string.len);
                name_found = true;
            }
        } else if (UsefulBuf_Compare(key, UsefulBuf_FromSZ("type")) == 0) {
            if (item.uDataType == QCBOR_TYPE_TEXT_STRING) {
                out.attr_type.assign((const char *) item.val.string.ptr, item.val.string.len);
                type_found = true;
            }
        } else if (UsefulBuf_Compare(key, UsefulBuf_FromSZ("value")) == 0) {
            if (item.uDataType == QCBOR_TYPE_BYTE_STRING) {
                //out.value.assign(
                    //(const char *) item.val.string.ptr,
                    //item.val.string.len
                //);
                value_found = true;
            }
        }
    }

    return name_found && type_found && value_found;
}

// static
template<>
bool scene_talk::decode(
    const frame_payload &payload,
    reader::file_stream &out) {
    QCBORDecodeContext decodeContext;
    QCBORDecode_Init(
        &decodeContext,
        UsefulBufC{(void *) payload.data(), payload.size()},
        QCBOR_DECODE_MODE_NORMAL);

    QCBORItem topItem;
    QCBORDecode_GetNext(&decodeContext, &topItem);
    if (topItem.uDataType != QCBOR_TYPE_MAP) {
        return false;
    }

    size_t mapEntries = topItem.val.uCount;
    bool name_found = false;
    bool hash_found = false;
    bool data_found = false;

    for (size_t i = 0; i < mapEntries; i++) {
        QCBORItem item;
        QCBORDecode_GetNext(&decodeContext, &item);
        if (item.uDataType != QCBOR_TYPE_TEXT_STRING) {
            QCBORDecode_GetNext(&decodeContext, &item);
            continue;
        }

        UsefulBufC key = item.val.string;
        QCBORDecode_GetNext(&decodeContext, &item);

        if (UsefulBuf_Compare(key, UsefulBuf_FromSZ("name")) == 0) {
            if (item.uDataType == QCBOR_TYPE_TEXT_STRING) {
                out.name.assign((const char *) item.val.string.ptr, item.val.string.len);
                name_found = true;
            }
        } else if (UsefulBuf_Compare(key, UsefulBuf_FromSZ("id")) == 0) {
            if (item.uDataType == QCBOR_TYPE_BYTE_STRING) {
                out.id.assign(
                    (const char *) item.val.string.ptr,
                    item.val.string.len
                );
                hash_found = true;
            }
        } else if (UsefulBuf_Compare(key, UsefulBuf_FromSZ("hash")) == 0) {
            if (item.uDataType == QCBOR_TYPE_BYTE_STRING) {
                out.hash.assign(
                    (const char *) item.val.string.ptr,
                    item.val.string.len
                );
                hash_found = true;
            }
        } else if (UsefulBuf_Compare(key, UsefulBuf_FromSZ("data")) == 0) {
            if (item.uDataType == QCBOR_TYPE_BYTE_STRING) {
                //file_stream.data.assign(
                 //   (const char *) item.val.string.ptr,
                  //  item.val.string.len
                //)//;
                data_found = true;
            }
        }
    }

    return name_found && hash_found && data_found;
}
